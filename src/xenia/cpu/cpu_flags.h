/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_CPU_CPU_FLAGS_H_
#define XENIA_CPU_CPU_FLAGS_H_
#include "xenia/base/cvar.h"

DECLARE_string(cpu);

DECLARE_string(load_module_map);

DECLARE_bool(disassemble_functions);

DECLARE_bool(trace_functions);
DECLARE_bool(trace_function_coverage);
DECLARE_bool(trace_function_references);
DECLARE_bool(trace_function_data);

DECLARE_bool(validate_hir);

DECLARE_uint64(pvr);

// Breakpoints:
DECLARE_uint64(break_on_instruction);
DECLARE_int32(break_condition_gpr);
DECLARE_uint64(break_condition_value);
DECLARE_string(break_condition_op);
DECLARE_bool(break_condition_truncate);

DECLARE_bool(break_on_debugbreak);

// Army of Two: The 40th Day (454108D8) same-PC runtime core. Every option is
// default-off and is additionally fenced to the supported retail build at the
// behavior site.
DECLARE_string(aot_runtime_peer_ipv4);
DECLARE_bool(aot_runtime_sa2);
DECLARE_bool(aot_runtime_leg_destination_repair);
DECLARE_bool(aot_runtime_xport_control_load_repair);

#endif  // XENIA_CPU_CPU_FLAGS_H_
