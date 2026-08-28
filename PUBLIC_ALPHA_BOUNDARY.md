# Same-PC source candidate boundary

This branch is a clean source candidate based on AdrianCassar's Xenia Canary
Netplay commit `b5f6f6ed618210ecfbbcb228994418f734cdd850`.

## Included production core

- Per-instance SDL controller allowlists through
  `--hid_sdl_allowed_devices`.
- Optional, default-off right-stick horizontal inversion through
  `--hid_sdl_invert_right_x`.
- Synthetic loopback identities through
  `--network_synthetic_loopback`, allowing two local emulator instances to
  bind the same guest ports while retaining distinct online addresses.
- A default-off, title/build-scoped XSA1 secure-association handshake for one
  explicitly configured synthetic-loopback peer. The worker is attempt-,
  receive-, and timeout-bounded and is synchronously stopped and joined.
- A default-off leg-destination repair that fills only an empty type-1 endpoint
  at one exact instruction seam. This writes the configured peer IPv4 address
  and port `0x1771` into guest memory.
- A separate default-off B19 XPort control-load repair at one exact instruction
  seam. It changes only guest register `r11` from zero to one when the guarded
  row byte is already one; it does not write guest memory.

The SDL allowlist and synthetic loopback mechanisms were exercised by the
backend-assisted Army of Two B19 same-PC run. The right-X correction is only
source- and build-validated; it was prepared but not exercised in that accepted
run. The newly reduced runtime core is also source- and build-validated only;
the historical run exercised predecessor behavior, not this clean extraction.
The foundation mechanisms remain title-independent and opt-in. With
synthetic loopback disabled, the legacy player lookup and port behavior are
preserved. Synthetic identity substitution also requires a successful,
non-zero backend `whoami` result.

Every runtime-core entry point requires title ID `454108D8` and executable
module hash `7C5F016EA6A81E95`. Each mutation additionally requires the exact
live instruction fingerprint. The XPort repair accepts only the documented B19
patched predecessor, not unpatched retail code. The leg repair also fails
closed when the configured peer is missing, non-synthetic, or equal to the
instance's own initialized synthetic identity.

## Deliberately excluded

- Broad guest-address trap lists, gameplay-state forcing, and unguarded writes.
- PASS-series probes, diagnostic logging, screenshots, logs, saves, game
  images, and reverse-engineering artifacts.
- Machine-specific paths, controller IDs, account identifiers, and backend
  deployment material.
- The historical USER0 record/replay/UDP input bridge.
- Legacy secure-association shims, native post-join fallbacks, and guest-write
  recovery paths outside the single guarded leg endpoint.

## Current status

This commit is a reviewable runtime-core source candidate, not a player kit or
runtime-proven release. Invalid, malformed, spoofed, and non-XSA1 datagrams are
passed to the guest unchanged. Only an exact validated 13-byte request is ACKed
and consumed, and only after a prior local connect and successful ACK send.
XSA1 is a local reachability/state shim, not cryptographic security.

The runtime-core guard is a source contract only. Static checks and successful
compilation do not establish runtime behavior.
Any completed compatibility candidate still requires a clean two-instance,
physical-controller acceptance run.
