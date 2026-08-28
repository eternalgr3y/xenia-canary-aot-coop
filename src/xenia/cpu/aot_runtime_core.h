/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_AOT_RUNTIME_CORE_H_
#define XENIA_CPU_AOT_RUNTIME_CORE_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace xe::cpu::aot_runtime {

constexpr uint32_t kTitleId = 0x454108D8u;
constexpr uint64_t kSupportedModuleHash = 0x7C5F016EA6A81E95ull;
constexpr uint32_t kLegDestinationPc = 0x823A0CC8u;
constexpr uint32_t kXportControlLoadPc = 0x8239D6C4u;

constexpr bool IsSupportedBuild(uint32_t title_id, uint64_t module_hash) {
  return title_id == kTitleId && module_hash == kSupportedModuleHash;
}

constexpr bool IsMutationAddress(uint32_t address) {
  return address == kLegDestinationPc || address == kXportControlLoadPc;
}

// Strict dotted-quad parser for the same-PC transport. The result is in the
// title's conventional big-endian integer form (127.a.b.c => 0x7FaaBBcc).
// Only distinct synthetic loopback peers are accepted; backend/self loopback
// 127.0.0.1 and the unspecified 127.0.0.0 address are deliberately excluded.
inline bool ParseSyntheticPeerIpv4(std::string_view text,
                                   uint32_t* guest_address) {
  if (!guest_address || text.empty()) {
    return false;
  }

  uint32_t octets[4] = {};
  std::size_t octet_index = 0;
  std::size_t octet_digits = 0;
  uint32_t value = 0;
  bool have_digit = false;
  for (char ch : text) {
    if (ch >= '0' && ch <= '9') {
      if (++octet_digits > 3u) {
        return false;
      }
      have_digit = true;
      value = value * 10u + static_cast<uint32_t>(ch - '0');
      if (value > 255u) {
        return false;
      }
    } else if (ch == '.' && have_digit && octet_index < 3u) {
      octets[octet_index++] = value;
      value = 0;
      octet_digits = 0;
      have_digit = false;
    } else {
      return false;
    }
  }
  if (!have_digit || octet_index != 3u) {
    return false;
  }
  octets[3] = value;

  const uint32_t parsed =
      (octets[0] << 24u) | (octets[1] << 16u) | (octets[2] << 8u) | octets[3];
  if (octets[0] != 127u || parsed == 0x7F000000u || parsed == 0x7F000001u) {
    return false;
  }
  *guest_address = parsed;
  return true;
}

}  // namespace xe::cpu::aot_runtime

#endif  // XENIA_CPU_AOT_RUNTIME_CORE_H_
