$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sdl = Join-Path $repo 'src\xenia\hid\sdl\sdl_input_driver.cc'
$xlive = Join-Path $repo 'src\xenia\kernel\XLiveAPI.cpp'
$xsocket = Join-Path $repo 'src\xenia\kernel\xsocket.cc'

foreach ($path in @($sdl, $xlive, $xsocket)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "missing required source file: $path"
    }
}

$sdlSource = Get-Content -LiteralPath $sdl -Raw
$xliveSource = Get-Content -LiteralPath $xlive -Raw
$xsocketSource = Get-Content -LiteralPath $xsocket -Raw

$required = [ordered]@{
    hid_allowlist_definition = $sdlSource.Contains('hid_sdl_allowed_devices, ""')
    hid_allowlist_filter = $sdlSource.Contains('IsControllerVidPidAllowed(vendor, product)')
    right_x_default_off = $sdlSource.Contains('DEFINE_bool(hid_sdl_invert_right_x, false,')
    right_x_application = $sdlSource.Contains('cvars::hid_sdl_invert_right_x ? ~event.caxis.value')
    loopback_default_off = $xliveSource.Contains('network_synthetic_loopback, false,')
    loopback_identity = $xliveSource.Contains('same-PC synthetic online IP')
    loopback_legacy_port = $xliveSource -match 'if \(!cvars::network_synthetic_loopback\)\s*\{\s*return 36000;'
    loopback_lookup_gated = $xliveSource.Contains('? FindPlayerByXuid(registered_xuid)')
    loopback_requires_whoami = $xliveSource -match 'network_synthetic_loopback &&\s*online_ip_\.sin_addr\.s_addr != 0'
    socket_bind_isolation = $xsocketSource.Contains('network_synthetic_loopback')
}

$failed = @($required.GetEnumerator() | Where-Object { -not $_.Value })
if ($failed.Count -ne 0) {
    throw ('public-core contract failed: ' + (($failed.Name | Sort-Object) -join ', '))
}

$changed = @(& git -C $repo diff --name-only `
    b5f6f6ed618210ecfbbcb228994418f734cdd850 --)
if ($LASTEXITCODE -ne 0) {
    throw 'unable to enumerate candidate changes'
}

$expectedChanged = @(
    '.github/workflows/public-core-contract.yml'
    'PUBLIC_ALPHA_BOUNDARY.md'
    'README.md'
    'src/xenia/hid/sdl/sdl_input_driver.cc'
    'src/xenia/kernel/XLiveAPI.cpp'
    'src/xenia/kernel/XLiveAPI.h'
    'src/xenia/kernel/json/player_object_json.cc'
    'src/xenia/kernel/xam/xam_net.cc'
    'src/xenia/kernel/xsocket.cc'
    'tools/public-alpha/test_same_pc_public_core.ps1'
)
$changed = @($changed | ForEach-Object { $_.Replace('\\', '/') } | Sort-Object)
$expectedChanged = @($expectedChanged | Sort-Object)
$boundaryDelta = @(Compare-Object -ReferenceObject $expectedChanged `
    -DifferenceObject $changed)
if ($boundaryDelta.Count -ne 0) {
    $details = @($boundaryDelta | ForEach-Object {
        '{0} ({1})' -f $_.InputObject, $_.SideIndicator
    })
    throw ('changed-file boundary mismatch: ' + ($details -join ', '))
}

$forbiddenExtensions = '\.(exe|dll|pdb|xex|iso|sav|dmp|zip|7z|rar)$'
$forbiddenFiles = @($changed | Where-Object { $_ -match $forbiddenExtensions })
if ($forbiddenFiles.Count -ne 0) {
    throw ('binary or game artifact entered candidate: ' +
           (($forbiddenFiles | Sort-Object) -join ', '))
}

$diff = (& git -C $repo diff --no-ext-diff --unified=0 `
    b5f6f6ed618210ecfbbcb228994418f734cdd850 -- .) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw 'unable to inspect candidate diff'
}

$forbiddenPatterns = [ordered]@{
    private_user_path = '(?i)' + 'C:' + '[\\/]' + 'Users' + '[\\/]'
    private_workspace_path = '(?i)' + 'C:' + '[\\/]' + 'xenia' + '-coop'
    pass_probe = '(?i)' + ('PA' + 'SS') + '\d{2,}'
    guest_address_probe = '(?i)' + ('ao' + 't_p2_breaks') + '|' +
        ('0x' + '82') + '[0-9a-f]{6}'
    title_specific_cvar = '(?i)\b' + ('ao' + 't_') + '[a-z0-9_]+'
    private_key = ('BEGIN ' + 'PRIVATE KEY')
    github_token = ('gh' + 'p_') + '[A-Za-z0-9]{20,}'
}
foreach ($entry in $forbiddenPatterns.GetEnumerator()) {
    if ($diff -match $entry.Value) {
        throw "forbidden public-candidate content: $($entry.Key)"
    }
}

Write-Output ("PUBLIC_CORE_SOURCE_CONTRACT_PASS checks={0} changed_files={1}" -f
    $required.Count, $changed.Count)
