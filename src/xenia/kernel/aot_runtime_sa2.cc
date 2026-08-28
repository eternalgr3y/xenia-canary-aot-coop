/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/aot_runtime_sa2.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace xe::kernel::aot_runtime {

namespace {

constexpr std::array<uint8_t, 4> kSa2Magic = {'X', 'S', 'A', '1'};
constexpr uint32_t kMaximumAttemptsLimit = 128u;
constexpr uint32_t kMaximumReceiveCallsLimit = 4096u;
constexpr std::chrono::milliseconds kMaximumRetryInterval{5000};
constexpr std::chrono::milliseconds kMaximumReceiveSlice{250};

uint32_t ReadAddress(const uint8_t* bytes) {
  uint32_t address = 0;
  std::memcpy(&address, bytes, sizeof(address));
  return address;
}

}  // namespace

bool IsExactSa2Frame(const uint8_t* bytes, size_t size, uint8_t type,
                     uint32_t source_ipv4_network, uint32_t own_ipv4_network,
                     uint32_t peer_ipv4_network, uint16_t source_port) {
  if (!bytes || size != kSa2FrameSize || !own_ipv4_network ||
      !peer_ipv4_network || own_ipv4_network == peer_ipv4_network ||
      !std::equal(kSa2Magic.begin(), kSa2Magic.end(), bytes) ||
      bytes[4] != type || source_ipv4_network != peer_ipv4_network ||
      ReadAddress(bytes + 5) != peer_ipv4_network ||
      ReadAddress(bytes + 9) != own_ipv4_network) {
    return false;
  }
  return type != 1u || source_port == kSa2GamePort;
}

Sa2Manager::Sa2Manager(ObservationSink observation_sink)
    : observation_sink_(std::move(observation_sink)) {}

Sa2Manager::~Sa2Manager() { Stop(); }

std::array<uint8_t, kSa2FrameSize> Sa2Manager::BuildFrame(
    uint8_t type, uint32_t sender_ipv4_network, uint32_t target_ipv4_network) {
  std::array<uint8_t, kSa2FrameSize> frame{};
  std::copy(kSa2Magic.begin(), kSa2Magic.end(), frame.begin());
  frame[4] = type;
  std::memcpy(frame.data() + 5, &sender_ipv4_network,
              sizeof(sender_ipv4_network));
  std::memcpy(frame.data() + 9, &target_ipv4_network,
              sizeof(target_ipv4_network));
  return frame;
}

bool Sa2Manager::Matches(uint32_t own_ipv4_network,
                         uint32_t peer_ipv4_network) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return connect_armed_ && own_ipv4_network_ == own_ipv4_network &&
         peer_ipv4_network_ == peer_ipv4_network;
}

void Sa2Manager::EmitObservationLocked(Sa2ObservationStage stage,
                                       uint64_t generation) {
  if (!observation_sink_) {
    return;
  }
  Sa2ObservationRecord record;
  record.stage = stage;
  record.sequence = ++observation_sequence_;
  record.generation = generation;
  record.own_ipv4_network = own_ipv4_network_;
  record.peer_ipv4_network = peer_ipv4_network_;
  try {
    observation_sink_(record);
  } catch (...) {
    // Acceptance evidence must never alter association behavior.
  }
}

bool Sa2Manager::ObservePreconnectFrame(const uint8_t* bytes, size_t size,
                                        uint32_t source_ipv4_network,
                                        uint32_t own_ipv4_network,
                                        uint32_t peer_ipv4_network) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (connect_armed_ || pending_preconnect_generation_ ||
      !IsExactSa2Frame(bytes, size, 0u, source_ipv4_network, own_ipv4_network,
                       peer_ipv4_network, 0u)) {
    return false;
  }
  own_ipv4_network_ = own_ipv4_network;
  peer_ipv4_network_ = peer_ipv4_network;
  pending_preconnect_generation_ = observation_generation_ + 1u;
  pending_preconnect_own_ipv4_network_ = own_ipv4_network;
  pending_preconnect_peer_ipv4_network_ = peer_ipv4_network;
  EmitObservationLocked(Sa2ObservationStage::kPreconnectPreparedForGuest,
                        pending_preconnect_generation_);
  return true;
}

bool Sa2Manager::Start(uint32_t own_ipv4_network, uint32_t peer_ipv4_network,
                       TransportFactory transport_factory,
                       Sa2WorkerOptions options) {
  if (!own_ipv4_network || !peer_ipv4_network ||
      own_ipv4_network == peer_ipv4_network || !transport_factory ||
      options.maximum_attempts == 0 ||
      options.maximum_attempts > kMaximumAttemptsLimit ||
      options.maximum_receive_calls < options.maximum_attempts ||
      options.maximum_receive_calls > kMaximumReceiveCallsLimit ||
      options.retry_interval.count() <= 0 ||
      options.retry_interval > kMaximumRetryInterval ||
      options.receive_slice.count() <= 0 ||
      options.receive_slice > kMaximumReceiveSlice) {
    return false;
  }

  std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool same_peer = connect_armed_ &&
                           own_ipv4_network_ == own_ipv4_network &&
                           peer_ipv4_network_ == peer_ipv4_network;
    if (same_peer &&
        (running_.load(std::memory_order_acquire) ||
         state_.load(std::memory_order_acquire) == Sa2State::kEstablished)) {
      return true;
    }
  }

  StopLocked();
  auto transport = transport_factory();
  if (!transport) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    own_ipv4_network_ = own_ipv4_network;
    peer_ipv4_network_ = peer_ipv4_network;
    connect_armed_ = true;
    ++generation_;
    const bool pending_preconnect_matches =
        pending_preconnect_generation_ == observation_generation_ + 1u &&
        pending_preconnect_own_ipv4_network_ == own_ipv4_network &&
        pending_preconnect_peer_ipv4_network_ == peer_ipv4_network;
    if (pending_preconnect_generation_ && !pending_preconnect_matches) {
      InvalidatePendingObservationLocked();
    }
    ++observation_generation_;
    active_observation_generation_ = observation_generation_;
    postconnect_observation_generation_ = 0;
    options_ = options;
    transport_ = std::move(transport);
    stop_requested_.store(false, std::memory_order_release);
    state_.store(Sa2State::kPending, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    EmitObservationLocked(Sa2ObservationStage::kXNetConnectManagerArmed,
                          active_observation_generation_);
    pending_preconnect_generation_ = 0;
    pending_preconnect_own_ipv4_network_ = 0;
    pending_preconnect_peer_ipv4_network_ = 0;
    worker_ = std::thread(&Sa2Manager::WorkerMain, this);
  }
  return true;
}

bool Sa2Manager::ValidateFrameLocked(const uint8_t* bytes, size_t size,
                                     uint8_t type, uint32_t source_ipv4_network,
                                     uint16_t source_port) const {
  return connect_armed_ &&
         IsExactSa2Frame(bytes, size, type, source_ipv4_network,
                         own_ipv4_network_, peer_ipv4_network_, source_port);
}

bool Sa2Manager::ValidateFrame(const uint8_t* bytes, size_t size, uint8_t type,
                               uint32_t source_ipv4_network,
                               uint16_t source_port) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return ValidateFrameLocked(bytes, size, type, source_ipv4_network,
                             source_port);
}

bool Sa2Manager::HandleRequest(const uint8_t* bytes, size_t size,
                               uint32_t source_ipv4_network,
                               AckSender ack_sender,
                               Sa2ConsumeToken* consume_token) {
  if (consume_token) {
    *consume_token = {};
  }
  if (!ack_sender) {
    return false;
  }
  uint32_t own = 0;
  uint32_t peer = 0;
  uint64_t generation = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidateFrameLocked(bytes, size, 0u, source_ipv4_network, 0u)) {
      return false;
    }
    own = own_ipv4_network_;
    peer = peer_ipv4_network_;
    generation = generation_;
  }
  const auto ack = BuildFrame(1u, own, peer);
  if (!ack_sender(ack)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!connect_armed_ || generation_ != generation ||
      own_ipv4_network_ != own || peer_ipv4_network_ != peer) {
    return false;
  }
  state_.store(Sa2State::kEstablished, std::memory_order_release);
  if (consume_token) {
    consume_token->manager_generation = generation_;
    consume_token->observation_generation = active_observation_generation_;
    consume_token->own_ipv4_network = own_ipv4_network_;
    consume_token->peer_ipv4_network = peer_ipv4_network_;
  }
  return true;
}

bool Sa2Manager::RecordConsumedAcked(const Sa2ConsumeToken& consume_token) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!connect_armed_ || !consume_token.manager_generation ||
      !consume_token.observation_generation ||
      generation_ != consume_token.manager_generation ||
      active_observation_generation_ != consume_token.observation_generation ||
      own_ipv4_network_ != consume_token.own_ipv4_network ||
      peer_ipv4_network_ != consume_token.peer_ipv4_network ||
      state_.load(std::memory_order_acquire) != Sa2State::kEstablished ||
      postconnect_observation_generation_ == active_observation_generation_) {
    return false;
  }
  postconnect_observation_generation_ = active_observation_generation_;
  EmitObservationLocked(Sa2ObservationStage::kPostconnectConsumedAckSent,
                        active_observation_generation_);
  return true;
}

void Sa2Manager::WorkerMain() {
  Sa2Transport* transport = nullptr;
  uint32_t own = 0;
  uint32_t peer = 0;
  Sa2WorkerOptions options;
  uint64_t generation = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    transport = transport_.get();
    own = own_ipv4_network_;
    peer = peer_ipv4_network_;
    options = options_;
    generation = generation_;
  }

  if (!transport || !transport->Open(own)) {
    if (transport) {
      transport->Close();
    }
    running_.store(false, std::memory_order_release);
    return;
  }

  const auto request = BuildFrame(0u, own, peer);
  auto next_send = std::chrono::steady_clock::now();
  uint32_t attempts = 0;
  uint32_t receive_calls = 0;
  while (!stop_requested_.load(std::memory_order_acquire) &&
         state_.load(std::memory_order_acquire) != Sa2State::kEstablished &&
         attempts < options.maximum_attempts &&
         receive_calls < options.maximum_receive_calls) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= next_send) {
      transport->Send(peer, kSa2GamePort, request.data(), request.size());
      ++attempts;
      next_send = now + options.retry_interval;
    }

    Sa2Packet packet;
    ++receive_calls;
    if (transport->Receive(&packet, options.receive_slice)) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (connect_armed_ && generation_ == generation &&
          ValidateFrameLocked(packet.bytes.data(), packet.size, 1u,
                              packet.source_ipv4_network, packet.source_port)) {
        state_.store(Sa2State::kEstablished, std::memory_order_release);
        break;
      }
    }
  }

  transport->Close();
  running_.store(false, std::memory_order_release);
}

void Sa2Manager::StopLocked() {
  stop_requested_.store(true, std::memory_order_release);
  std::thread worker;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    connect_armed_ = false;
    ++generation_;
    if (worker_.joinable()) {
      worker = std::move(worker_);
    }
  }
  if (worker.joinable()) {
    worker.join();
  }

  std::lock_guard<std::mutex> lock(mutex_);
  transport_.reset();
  own_ipv4_network_ = 0;
  peer_ipv4_network_ = 0;
  active_observation_generation_ = 0;
  postconnect_observation_generation_ = 0;
  state_.store(Sa2State::kIdle, std::memory_order_release);
  running_.store(false, std::memory_order_release);
}

void Sa2Manager::Stop() {
  std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
  StopLocked();
  std::lock_guard<std::mutex> lock(mutex_);
  InvalidatePendingObservationLocked();
}

void Sa2Manager::InvalidatePendingObservationLocked() {
  if (pending_preconnect_generation_) {
    observation_generation_ =
        std::max(observation_generation_, pending_preconnect_generation_);
  }
  pending_preconnect_generation_ = 0;
  pending_preconnect_own_ipv4_network_ = 0;
  pending_preconnect_peer_ipv4_network_ = 0;
}

}  // namespace xe::kernel::aot_runtime
