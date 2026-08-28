$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$base = 'b5f6f6ed618210ecfbbcb228994418f734cdd850'

$paths = [ordered]@{
    sdl = 'src\xenia\hid\sdl\sdl_input_driver.cc'
    xlive = 'src\xenia\kernel\XLiveAPI.cpp'
    xsocket = 'src\xenia\kernel\xsocket.cc'
    cpuContract = 'src\xenia\cpu\aot_runtime_core.h'
    cpuFlags = 'src\xenia\cpu\cpu_flags.cc'
    hir = 'src\xenia\cpu\ppc\ppc_hir_builder.cc'
    emitter = 'src\xenia\cpu\backend\x64\x64_emitter.cc'
    runtime = 'src\xenia\kernel\aot_runtime_core.cc'
    sa2Header = 'src\xenia\kernel\aot_runtime_sa2.h'
    sa2 = 'src\xenia\kernel\aot_runtime_sa2.cc'
    xamNet = 'src\xenia\kernel\xam\xam_net.cc'
    tests = 'src\xenia\kernel\testing\aot_runtime_sa2_test.cc'
    boundary = 'AOT_RUNTIME_CORE.md'
    workflow = '.github\workflows\public-core-contract.yml'
}

$source = @{}
foreach ($entry in $paths.GetEnumerator()) {
    $path = Join-Path $repo $entry.Value
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "missing required source file: $path"
    }
    $source[$entry.Key] = Get-Content -LiteralPath $path -Raw
}

$required = [ordered]@{
    hid_allowlist_definition = $source.sdl.Contains('hid_sdl_allowed_devices, ""')
    hid_allowlist_filter = $source.sdl.Contains('IsControllerVidPidAllowed(vendor, product)')
    right_x_default_off = $source.sdl.Contains('DEFINE_bool(hid_sdl_invert_right_x, false,')
    loopback_default_off = $source.xlive.Contains('network_synthetic_loopback, false,')
    loopback_lookup_gated = $source.xlive.Contains('? FindPlayerByXuid(registered_xuid)')
    loopback_requires_whoami = $source.xlive -match 'network_synthetic_loopback &&\s*online_ip_\.sin_addr\.s_addr != 0'
    title_id = $source.cpuContract.Contains('0x454108D8u')
    module_hash = $source.cpuContract.Contains('0x7C5F016EA6A81E95ull')
    strict_peer_parser = $source.cpuContract.Contains(
        'IsSyntheticLoopbackGuestIpv4(parsed)') -and
        $source.cpuContract.Contains('address != 0x7F000001u')
    mutation_rejects_own_peer = $source.emitter.Contains(
        'IsDistinctSyntheticPeer(peer, own_guest)') -and
        $source.tests.Contains('must differ from the local synthetic identity')
    mutation_helpers_tested = $source.emitter.Contains(
        'TryRepairLegDestination(') -and
        $source.emitter.Contains('TryRepairXportControlLoad(') -and
        $source.tests.Contains('leg destination repair is exact') -and
        $source.tests.Contains('preserves full-width rejects')
    sa2_default_off = $source.cpuFlags.Contains('aot_runtime_sa2, false,')
    leg_default_off = $source.cpuFlags.Contains('aot_runtime_leg_destination_repair, false,')
    xport_default_off = $source.cpuFlags.Contains('aot_runtime_xport_control_load_repair, false,')
    exact_mutation_addresses = $source.cpuContract.Contains('0x823A0CC8u') -and
        $source.cpuContract.Contains('0x8239D6C4u')
    runtime_hash_guard = $source.runtime.Contains('IsSupportedBuild(executable->title_id()')
    trap_hash_guard = $source.emitter.Contains('IsSupportedBuild(executable->title_id(), *module_hash)')
    executing_module_guard = $source.emitter.Contains('executable->xex_module() != executing_module')
    full_leg_sentinel = $source.emitter.Contains('read32(0x823A0CF8u) == 0x8164000Cu')
    full_xport_sentinel = $source.emitter.Contains('read32(0x8239D6B8u) == 0x895F0030u') -and
        $source.emitter.Contains('read32(0x8239D6CCu) == 0x7D6A5A14u')
    xport_b19_only = $source.emitter.Contains('read32(0x8239D6C0u) == 0x39600000u') -and
        $source.emitter.Contains('(897F0680)')
    xport_full_register = $source.cpuContract.Contains('*r11 != 0u') -and
        $source.emitter.Contains('uint64_t repaired_r11 = context->r[11]')
    owned_worker = $source.sa2.Contains('worker.join();') -and
        $source.sa2.Contains('lifecycle_lock(lifecycle_mutex_)')
    bounded_worker = $source.sa2Header.Contains('maximum_receive_calls = 640u') -and
        $source.sa2.Contains('kMaximumAttemptsLimit') -and
        $source.sa2.Contains('kMaximumReceiveCallsLimit')
    connect_gate = $source.sa2.Contains('connect_armed_') -and
        -not $source.sa2.Contains('ConfigurePassive') -and
        -not $source.runtime.Contains('ConfigurePassive')
    query_side_effect_free = $source.runtime.Contains('Query is deliberately side-effect free')
    ack_generation_commit = $source.sa2.Contains('generation_ != generation') -and
        $source.sa2.Contains('if (!ack_sender(ack))')
    async_only_intercept = $source.xsocket.Contains('if (!intercept_sa2)') -and
        $source.xsocket.Contains('AotRuntimeSa2ShouldPollAgain(disposition)') -and
        $source.xsocket.Contains('goto poll_again;')
    socket_disposition_tested = $source.tests.Contains(
        'socket disposition passes rejects and polls after consumption') -and
        $source.xsocket.Contains('AotRuntimeSa2Disposition(')
    focused_tests = $source.tests.Contains('prior local connect') -and
        $source.tests.Contains('concurrent same-peer starts') -and
        $source.tests.Contains('unregister during ACK') -and
        $source.tests.Contains('malformed flood')
    focused_test_tag = ([regex]::Matches($source.tests,
        '\[aot-runtime-core\]')).Count -eq 11
    ci_filtered_tests = $source.workflow.Contains(
        "xenia-kernel-tests.exe '[aot-runtime-core]'")
    ci_app_build = $source.workflow.Contains(
        'build --config=release --target=xenia-app --build-tests --no_premake')
    mutation_disclosure = $source.boundary.Contains('guest-memory mutation') -and
        $source.boundary.Contains('Guest memory is not modified')
    packet_policy = $source.boundary.Contains('rejected XSA1 datagram remains visible to the guest unchanged')
}

$failed = @($required.GetEnumerator() | Where-Object { -not $_.Value })
if ($failed.Count -ne 0) {
    throw ('runtime-core contract failed: ' + (($failed.Name | Sort-Object) -join ', '))
}

# The synchronous receive path is outside the exercised SA2 seam and must stay
# byte-for-byte equivalent (after line-ending normalization) to the base.
$baseXsocket = (& git -C $repo show "$base`:src/xenia/kernel/xsocket.cc") -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw 'unable to read base xsocket source'
}
$recvPattern = '(?s)int XSocket::RecvFrom\(.*?\n\}\n\nstruct WSARecvFromData'
$baseRecv = [regex]::Match($baseXsocket, $recvPattern).Value
$candidateRecv = [regex]::Match(($source.xsocket -replace "`r`n", "`n"), $recvPattern).Value
if (-not $baseRecv -or -not $candidateRecv -or $baseRecv -ne $candidateRecv) {
    throw 'synchronous RecvFrom changed outside the accepted async SA2 seam'
}

$changed = @(& git -C $repo diff --name-only --ignore-submodules=all $base --)
if ($LASTEXITCODE -ne 0) {
    throw 'unable to enumerate candidate changes'
}

$committedGitlinks = @(& git -C $repo diff --name-only $base HEAD -- third_party)
$stagedGitlinks = @(& git -C $repo diff --cached --name-only -- third_party)
$workingGitlinks = @(& git -C $repo diff --name-only --ignore-submodules=dirty -- third_party)
if ($LASTEXITCODE -ne 0) {
    throw 'unable to inspect third-party gitlinks'
}
$gitlinkChanges = @($committedGitlinks + $stagedGitlinks + $workingGitlinks |
    Where-Object { $_ } | Sort-Object -Unique)
if ($gitlinkChanges.Count -ne 0) {
    throw ('third-party gitlink changed: ' + ($gitlinkChanges -join ', '))
}

$expectedChanged = @(
    '.github/workflows/public-core-contract.yml'
    'AOT_RUNTIME_CORE.md'
    'PUBLIC_ALPHA_BOUNDARY.md'
    'README.md'
    'src/xenia/cpu/aot_runtime_core.h'
    'src/xenia/cpu/backend/x64/x64_emitter.cc'
    'src/xenia/cpu/backend/x64/x64_emitter.h'
    'src/xenia/cpu/cpu_flags.cc'
    'src/xenia/cpu/cpu_flags.h'
    'src/xenia/cpu/ppc/ppc_hir_builder.cc'
    'src/xenia/hid/sdl/sdl_input_driver.cc'
    'src/xenia/kernel/XLiveAPI.cpp'
    'src/xenia/kernel/XLiveAPI.h'
    'src/xenia/kernel/aot_runtime_core.cc'
    'src/xenia/kernel/aot_runtime_core.h'
    'src/xenia/kernel/aot_runtime_sa2.cc'
    'src/xenia/kernel/aot_runtime_sa2.h'
    'src/xenia/kernel/json/player_object_json.cc'
    'src/xenia/kernel/kernel_state.cc'
    'src/xenia/kernel/testing/CMakeLists.txt'
    'src/xenia/kernel/testing/aot_runtime_sa2_test.cc'
    'src/xenia/kernel/xam/xam_net.cc'
    'src/xenia/kernel/xsocket.cc'
    'tools/public-alpha/test_same_pc_public_core.ps1'
)
$changed = @($changed | ForEach-Object { $_.Replace('\', '/') } | Sort-Object)
$expectedChanged = @($expectedChanged | Sort-Object)
$boundaryDelta = @(Compare-Object -ReferenceObject $expectedChanged `
    -DifferenceObject $changed)
if ($boundaryDelta.Count -ne 0) {
    $details = @($boundaryDelta | ForEach-Object {
        '{0} ({1})' -f $_.InputObject, $_.SideIndicator
    })
    throw ('changed-file boundary mismatch: ' + ($details -join ', '))
}

$forbiddenExtensions = '\.(exe|dll|pdb|xex|iso|sav|dmp|zip|7z|rar|png|jpg)$'
$forbiddenFiles = @($changed | Where-Object { $_ -match $forbiddenExtensions })
if ($forbiddenFiles.Count -ne 0) {
    throw ('binary, game, or evidence artifact entered candidate: ' +
           (($forbiddenFiles | Sort-Object) -join ', '))
}

$diff = (& git -C $repo diff --no-ext-diff --unified=0 `
    --ignore-submodules=all $base -- .) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw 'unable to inspect candidate diff'
}

$forbiddenPatterns = [ordered]@{
    private_user_path = '(?i)' + 'C:' + '[\\/]' + 'Users' + '[\\/]'
    private_workspace_path = '(?i)' + 'C:' + '[\\/]' + 'xenia' + '-coop'
    pass_probe = '(?i)' + ('PA' + 'SS') + '\d{2,}'
    private_key = ('BEGIN ' + 'PRIVATE KEY')
    github_token = ('gh' + 'p_') + '[A-Za-z0-9]{20,}'
    legacy_sa = '(?i)\b' + ('aot_sec_' + 'assoc') + '\b'
    native_fallback = '(?i)\b' + ('aot_native_' + 'postjoin') + '\b'
    broad_probe = '(?i)\b' + ('aot_p2_' + 'breaks') + '\b'
    personal_name = '(?i)\b' + ('ta' + 'pin') + '\b'
}
foreach ($entry in $forbiddenPatterns.GetEnumerator()) {
    if ($diff -match $entry.Value) {
        throw "forbidden runtime-candidate content: $($entry.Key)"
    }
}

Write-Output ("AOT_RUNTIME_CORE_SOURCE_CONTRACT_PASS checks={0} changed_files={1}" -f
    $required.Count, $changed.Count)
