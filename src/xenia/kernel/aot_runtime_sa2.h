/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_AOT_RUNTIME_SA2_H_
#define XENIA_KERNEL_AOT_RUNTIME_SA2_H_

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace xe::kernel::aot_runtime {

constexpr size_t kSa2FrameSize = 13u;
constexpr uint16_t kSa2GamePort = 1000u;

struct Sa2Packet {
  std::array<uint8_t, 64> bytes{};
  size_t size = 0;
  uint32_t source_ipv4_network = 0;
  uint16_t source_port = 0;
};

class Sa2Transport {
 public:
  virtual ~Sa2Transport() = default;
  virtual bool Open(uint32_t bind_ipv4_network) = 0;
  virtual bool Send(uint32_t destination_ipv4_network,
                    uint16_t destination_port, const uint8_t* bytes,
                    size_t size) = 0;
  virtual bool Receive(Sa2Packet* packet,
                       std::chrono::milliseconds timeout) = 0;
  virtual void Close() = 0;
};

struct Sa2WorkerOptions {
  uint32_t maximum_attempts = 40u;
  uint32_t maximum_receive_calls = 640u;
  std::chrono::milliseconds retry_interval{250};
  std::chrono::milliseconds receive_slice{25};
};

enum class Sa2State : uint32_t {
  kIdle = 0,
  kPending = 1,
  kEstablished = 2,
};

enum class Sa2ObservationStage : uint32_t {
  kPreconnectPreparedForGuest = 1u,
  kXNetConnectManagerArmed = 2u,
  kPostconnectConsumedAckSent = 3u,
};

struct Sa2ObservationRecord {
  Sa2ObservationStage stage = Sa2ObservationStage::kPreconnectPreparedForGuest;
  uint64_t sequence = 0;
  uint64_t generation = 0;
  uint32_t own_ipv4_network = 0;
  uint32_t peer_ipv4_network = 0;
};

struct Sa2ConsumeToken {
  uint64_t manager_generation = 0;
  uint64_t observation_generation = 0;
  uint32_t own_ipv4_network = 0;
  uint32_t peer_ipv4_network = 0;
};

bool IsExactSa2Frame(const uint8_t* bytes, size_t size, uint8_t type,
                     uint32_t source_ipv4_network, uint32_t own_ipv4_network,
                     uint32_t peer_ipv4_network, uint16_t source_port);

class Sa2Manager {
 public:
  using TransportFactory = std::function<std::unique_ptr<Sa2Transport>()>;
  using AckSender =
      std::function<bool(const std::array<uint8_t, kSa2FrameSize>& ack)>;
  using ObservationSink = std::function<void(const Sa2ObservationRecord&)>;

  explicit Sa2Manager(ObservationSink observation_sink = {});
  ~Sa2Manager();
  Sa2Manager(const Sa2Manager&) = delete;
  Sa2Manager& operator=(const Sa2Manager&) = delete;

  bool Start(uint32_t own_ipv4_network, uint32_t peer_ipv4_network,
             TransportFactory transport_factory, Sa2WorkerOptions options = {});
  bool Matches(uint32_t own_ipv4_network, uint32_t peer_ipv4_network) const;
  bool ObservePreconnectFrame(const uint8_t* bytes, size_t size,
                              uint32_t source_ipv4_network,
                              uint32_t own_ipv4_network,
                              uint32_t peer_ipv4_network);

  // Accepts only an exact XSA1 REQ whose payload sender matches the native
  // source address, whose target matches this instance, and whose sender is
  // the configured peer. On success, returns the exact ACK bytes and marks the
  // association established only after ack_sender confirms that the ACK was
  // sent. The manager must already have been armed by Start for this exact
  // peer. Rejected data and send failures must remain visible to the guest.
  bool HandleRequest(const uint8_t* bytes, size_t size,
                     uint32_t source_ipv4_network, AckSender ack_sender,
                     Sa2ConsumeToken* consume_token = nullptr);
  bool RecordConsumedAcked(const Sa2ConsumeToken& consume_token);

  Sa2State state() const { return state_.load(std::memory_order_acquire); }
  bool running() const { return running_.load(std::memory_order_acquire); }
  void Stop();

  static std::array<uint8_t, kSa2FrameSize> BuildFrame(
      uint8_t type, uint32_t sender_ipv4_network, uint32_t target_ipv4_network);

 private:
  bool ValidateFrameLocked(const uint8_t* bytes, size_t size, uint8_t type,
                           uint32_t source_ipv4_network,
                           uint16_t source_port) const;
  bool ValidateFrame(const uint8_t* bytes, size_t size, uint8_t type,
                     uint32_t source_ipv4_network, uint16_t source_port) const;
  void EmitObservationLocked(Sa2ObservationStage stage, uint64_t generation);
  void InvalidatePendingObservationLocked();
  void StopLocked();
  void WorkerMain();

  // Serializes Start and Stop across worker creation, move and join. Worker
  // code never takes this mutex, so joining while it is held is bounded.
  mutable std::mutex lifecycle_mutex_;
  mutable std::mutex mutex_;
  uint32_t own_ipv4_network_ = 0;
  uint32_t peer_ipv4_network_ = 0;
  Sa2WorkerOptions options_{};
  std::unique_ptr<Sa2Transport> transport_;
  std::thread worker_;
  std::atomic<Sa2State> state_{Sa2State::kIdle};
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  bool connect_armed_ = false;
  uint64_t generation_ = 0;
  ObservationSink observation_sink_;
  uint64_t observation_sequence_ = 0;
  uint64_t observation_generation_ = 0;
  uint64_t pending_preconnect_generation_ = 0;
  uint32_t pending_preconnect_own_ipv4_network_ = 0;
  uint32_t pending_preconnect_peer_ipv4_network_ = 0;
  uint64_t active_observation_generation_ = 0;
  uint64_t postconnect_observation_generation_ = 0;
};

}  // namespace xe::kernel::aot_runtime

#endif  // XENIA_KERNEL_AOT_RUNTIME_SA2_H_
