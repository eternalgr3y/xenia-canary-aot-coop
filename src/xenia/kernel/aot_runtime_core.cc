/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/aot_runtime_core.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <utility>

// clang-format off
#include "xenia/base/platform.h"
#ifdef XE_PLATFORM_WIN32
#include <WS2tcpip.h>
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
// clang-format on

#include "xenia/base/logging.h"
#include "xenia/cpu/aot_runtime_core.h"
#include "xenia/cpu/cpu_flags.h"
#include "xenia/kernel/XLiveAPI.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/user_module.h"

DECLARE_bool(network_synthetic_loopback);

namespace xe::kernel {

namespace {

using aot_runtime::Sa2Manager;
using aot_runtime::Sa2Packet;
using aot_runtime::Sa2State;
using aot_runtime::Sa2Transport;

struct RuntimeConfig {
  uint32_t own_guest = 0;
  uint32_t own_network = 0;
  uint32_t peer_guest = 0;
  uint32_t peer_network = 0;
};

bool ResolveRuntimeConfig(KernelState* kernel_state, RuntimeConfig* config) {
  if (!config || !kernel_state || !cvars::network_synthetic_loopback ||
      !cvars::aot_runtime_sa2) {
    return false;
  }
  const auto executable = kernel_state->GetExecutableModule();
  const auto module_hash = executable ? executable->hash() : std::nullopt;
  if (!executable || !module_hash ||
      !cpu::aot_runtime::IsSupportedBuild(executable->title_id(),
                                          *module_hash)) {
    return false;
  }

  uint32_t peer_guest = 0;
  if (!cpu::aot_runtime::ParseSyntheticPeerIpv4(cvars::aot_runtime_peer_ipv4,
                                                &peer_guest)) {
    return false;
  }
  auto* xlive = kernel_state->GetXboxLiveAPI();
  if (!xlive) {
    return false;
  }
  const uint32_t own_network = xlive->OnlineIP().sin_addr.s_addr;
  const uint32_t own_guest = ntohl(own_network);
  if ((own_guest >> 24u) != 127u || own_guest == 0x7F000000u ||
      own_guest == 0x7F000001u || own_guest == peer_guest) {
    return false;
  }

  config->own_guest = own_guest;
  config->own_network = own_network;
  config->peer_guest = peer_guest;
  config->peer_network = htonl(peer_guest);
  return true;
}

bool AddressMatchesPeer(uint32_t address, const RuntimeConfig& config) {
  return address == config.peer_guest || address == config.peer_network;
}

class NativeSa2Transport final : public Sa2Transport {
 public:
  ~NativeSa2Transport() override { Close(); }

  bool Open(uint32_t bind_ipv4_network) override {
    Close();
    socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!IsOpen()) {
      return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = 0;
    address.sin_addr.s_addr = bind_ipv4_network;
    if (::bind(socket_, reinterpret_cast<const sockaddr*>(&address),
               sizeof(address)) != 0) {
      Close();
      return false;
    }
    return true;
  }

  bool Send(uint32_t destination_ipv4_network, uint16_t destination_port,
            const uint8_t* bytes, size_t size) override {
    if (!IsOpen() || !bytes || !size) {
      return false;
    }
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(destination_port);
    destination.sin_addr.s_addr = destination_ipv4_network;
    const int sent = ::sendto(socket_, reinterpret_cast<const char*>(bytes),
                              static_cast<int>(size), 0,
                              reinterpret_cast<const sockaddr*>(&destination),
                              sizeof(destination));
    return sent == static_cast<int>(size);
  }

  bool Receive(Sa2Packet* packet, std::chrono::milliseconds timeout) override {
    if (!IsOpen() || !packet || timeout.count() < 0) {
      return false;
    }

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(socket_, &read_set);
    timeval wait{};
    wait.tv_sec = static_cast<long>(timeout.count() / 1000);
    wait.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
#ifdef XE_PLATFORM_WIN32
    const int selected = ::select(0, &read_set, nullptr, nullptr, &wait);
#else
    const int selected =
        ::select(socket_ + 1, &read_set, nullptr, nullptr, &wait);
#endif
    if (selected <= 0) {
      return false;
    }

    sockaddr_in source{};
    socklen_t source_size = sizeof(source);
    const int received =
        ::recvfrom(socket_, reinterpret_cast<char*>(packet->bytes.data()),
                   static_cast<int>(packet->bytes.size()), 0,
                   reinterpret_cast<sockaddr*>(&source), &source_size);
    if (received < 0) {
      return false;
    }
    packet->size = static_cast<size_t>(received);
    packet->source_ipv4_network = source.sin_addr.s_addr;
    packet->source_port = ntohs(source.sin_port);
    return true;
  }

  void Close() override {
    if (!IsOpen()) {
      return;
    }
#ifdef XE_PLATFORM_WIN32
    ::closesocket(socket_);
    socket_ = INVALID_SOCKET;
#else
    ::close(socket_);
    socket_ = -1;
#endif
  }

 private:
  bool IsOpen() const {
#ifdef XE_PLATFORM_WIN32
    return socket_ != INVALID_SOCKET;
#else
    return socket_ >= 0;
#endif
  }

#ifdef XE_PLATFORM_WIN32
  SOCKET socket_ = INVALID_SOCKET;
#else
  int socket_ = -1;
#endif
};

void LogSa2AcceptanceObservation(
    const aot_runtime::Sa2ObservationRecord& record) {
  switch (record.stage) {
    case aot_runtime::Sa2ObservationStage::kPreconnectPreparedForGuest:
      XELOGI(
          "[AOT-RUNTIME-SA2][ACCEPT] seq={} generation={} stage=1 "
          "event=PRECONNECT_XSA1_PREPARED_FOR_GUEST",
          record.sequence, record.generation);
      break;
    case aot_runtime::Sa2ObservationStage::kXNetConnectManagerArmed:
      XELOGI(
          "[AOT-RUNTIME-SA2][ACCEPT] seq={} generation={} stage=2 "
          "event=XNETCONNECT_MANAGER_ARMED",
          record.sequence, record.generation);
      break;
    case aot_runtime::Sa2ObservationStage::kPostconnectConsumedAckSent:
      XELOGI(
          "[AOT-RUNTIME-SA2][ACCEPT] seq={} generation={} stage=3 "
          "event=POSTCONNECT_XSA1_RETRANSMIT_CONSUMED_ACK_SENT",
          record.sequence, record.generation);
      break;
  }
}

Sa2Manager& RuntimeSa2Manager() {
  static Sa2Manager manager(LogSa2AcceptanceObservation);
  return manager;
}

}  // namespace

bool AotRuntimeSa2Connect(KernelState* kernel_state,
                          uint32_t guest_peer_address) {
  RuntimeConfig config;
  if (!ResolveRuntimeConfig(kernel_state, &config) ||
      !AddressMatchesPeer(guest_peer_address, config)) {
    return false;
  }
  return RuntimeSa2Manager().Start(
      config.own_network, config.peer_network,
      []() { return std::make_unique<NativeSa2Transport>(); });
}

bool AotRuntimeSa2Query(KernelState* kernel_state, uint32_t guest_peer_address,
                        Sa2State* state) {
  if (!state) {
    return false;
  }
  RuntimeConfig config;
  if (!ResolveRuntimeConfig(kernel_state, &config) ||
      !AddressMatchesPeer(guest_peer_address, config)) {
    return false;
  }
  auto& manager = RuntimeSa2Manager();
  // Query is deliberately side-effect free. Before a local XNetConnect, and
  // after unregister, the configured peer remains pending rather than falling
  // through to Xenia's legacy always-connected stub.
  *state = manager.Matches(config.own_network, config.peer_network)
               ? manager.state()
               : Sa2State::kPending;
  return true;
}

void AotRuntimeSa2Unregister(KernelState* kernel_state,
                             uint32_t guest_peer_address) {
  RuntimeConfig config;
  if (ResolveRuntimeConfig(kernel_state, &config) &&
      AddressMatchesPeer(guest_peer_address, config)) {
    RuntimeSa2Manager().Stop();
  }
}

bool AotRuntimeSa2InterceptionEnabled(KernelState* kernel_state) {
  RuntimeConfig config;
  return ResolveRuntimeConfig(kernel_state, &config) &&
         RuntimeSa2Manager().Matches(config.own_network, config.peer_network);
}

void AotRuntimeSa2ObservePreconnectPrepared(KernelState* kernel_state,
                                            const uint8_t* bytes, size_t size,
                                            uint32_t source_ipv4_network) {
  RuntimeConfig config;
  if (!ResolveRuntimeConfig(kernel_state, &config)) {
    return;
  }
  RuntimeSa2Manager().ObservePreconnectFrame(bytes, size, source_ipv4_network,
                                             config.own_network,
                                             config.peer_network);
}

bool AotRuntimeSa2HandleRequest(KernelState* kernel_state, const uint8_t* bytes,
                                size_t size, uint32_t source_ipv4_network,
                                Sa2Manager::AckSender ack_sender,
                                aot_runtime::Sa2ConsumeToken* consume_token) {
  RuntimeConfig config;
  if (!ResolveRuntimeConfig(kernel_state, &config) ||
      !RuntimeSa2Manager().Matches(config.own_network, config.peer_network)) {
    return false;
  }
  return RuntimeSa2Manager().HandleRequest(
      bytes, size, source_ipv4_network, std::move(ack_sender), consume_token);
}

void AotRuntimeSa2RecordConsumedAcked(
    KernelState* kernel_state,
    const aot_runtime::Sa2ConsumeToken& consume_token) {
  RuntimeConfig config;
  if (!ResolveRuntimeConfig(kernel_state, &config) ||
      consume_token.own_ipv4_network != config.own_network ||
      consume_token.peer_ipv4_network != config.peer_network) {
    return;
  }
  RuntimeSa2Manager().RecordConsumedAcked(consume_token);
}

void AotRuntimeSa2Shutdown() { RuntimeSa2Manager().Stop(); }

}  // namespace xe::kernel
