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

The SDL allowlist and synthetic loopback mechanisms were exercised by the
backend-assisted Army of Two B19 same-PC run. The right-X correction is only
source- and build-validated; it was prepared but not exercised in that accepted
run. All three mechanisms are title-independent and remain opt-in. With
synthetic loopback disabled, the legacy player lookup and port behavior are
preserved. Synthetic identity substitution also requires a successful,
non-zero backend `whoami` result.

## Deliberately excluded

- Guest-address traps, title-specific field writes, and gameplay steering.
- PASS-series probes, diagnostic logging, screenshots, logs, saves, game
  images, and reverse-engineering artifacts.
- Machine-specific paths, controller IDs, account identifiers, and backend
  deployment material.
- The historical USER0 record/replay/UDP input bridge.

## Current status

This commit is the portable same-PC foundation, not a complete Army of Two
release. The demonstrated B19 run also used a title-specific secure-association
and native post-join compatibility layer. That layer must be independently
reduced, reviewed, and validated before this branch can be called an AoT
co-op alpha.

The public-core guard is a source contract only. Static checks and successful
compilation do not establish runtime behavior.
Any completed compatibility candidate still requires a clean two-instance,
physical-controller acceptance run.
