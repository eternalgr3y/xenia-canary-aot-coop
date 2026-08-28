/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_AOT_RUNTIME_CORE_H_
#define XENIA_KERNEL_AOT_RUNTIME_CORE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

#include "xenia/kernel/aot_runtime_sa2.h"

namespace xe::kernel {

class KernelState;

enum class AotRuntimeSa2DatagramDisposition {
  kPassThrough,
  kConsumeAndPollAgain,
};

constexpr AotRuntimeSa2DatagramDisposition AotRuntimeSa2Disposition(
    bool request_consumed) {
  return request_consumed
             ? AotRuntimeSa2DatagramDisposition::kConsumeAndPollAgain
             : AotRuntimeSa2DatagramDisposition::kPassThrough;
}

constexpr bool AotRuntimeSa2ShouldPollAgain(
    AotRuntimeSa2DatagramDisposition disposition) {
  return disposition == AotRuntimeSa2DatagramDisposition::kConsumeAndPollAgain;
}

bool AotRuntimeSa2Connect(KernelState* kernel_state,
                          uint32_t guest_peer_address);
bool AotRuntimeSa2Query(KernelState* kernel_state, uint32_t guest_peer_address,
                        aot_runtime::Sa2State* state);
void AotRuntimeSa2Unregister(KernelState* kernel_state,
                             uint32_t guest_peer_address);
bool AotRuntimeSa2InterceptionEnabled(KernelState* kernel_state);
void AotRuntimeSa2ObservePreconnectPrepared(KernelState* kernel_state,
                                            const uint8_t* bytes, size_t size,
                                            uint32_t source_ipv4_network);
bool AotRuntimeSa2HandleRequest(KernelState* kernel_state, const uint8_t* bytes,
                                size_t size, uint32_t source_ipv4_network,
                                aot_runtime::Sa2Manager::AckSender ack_sender,
                                aot_runtime::Sa2ConsumeToken* consume_token);
void AotRuntimeSa2RecordConsumedAcked(
    KernelState* kernel_state,
    const aot_runtime::Sa2ConsumeToken& consume_token);
void AotRuntimeSa2Shutdown();

}  // namespace xe::kernel

#endif  // XENIA_KERNEL_AOT_RUNTIME_CORE_H_
