# Army of Two B19 runtime-core contract

This source candidate reduces the title-specific behavior used by the proven
same-PC co-op path to three opt-in mechanisms. It is not runtime acceptance
evidence for this extraction.

## Build and peer gates

All three mechanisms are disabled by default. They require title ID `454108D8`
and executable module hash `7C5F016EA6A81E95`. The network mechanism also
requires synthetic loopback, a successful non-zero online identity, and one
explicit peer supplied by `--aot_runtime_peer_ipv4`. Only distinct `127.x`
addresses are accepted; `127.0.0.0`, `127.0.0.1`, and the instance's own
address are rejected. The leg-destination repair independently rechecks that
the configured peer differs from the initialized local synthetic identity
before it can write an endpoint.

Enable the bounded association worker with `--aot_runtime_sa2=true`. A local
`XNetConnect` for the configured peer is the only operation that arms it.
Status queries are side-effect free, and unregister/title shutdown stops and
joins the owned worker. The default production limits are 40 sends, 640 receive
calls, a 250 ms retry interval, and a 25 ms receive slice.

The prober sends an exact 13-byte XSA1 request to the configured peer's UDP
port 1000. An ACK establishes the association only when its native source,
payload sender, payload target, type, size, and source port all match. The game
socket accepts an inbound request only after local connect and validates the
same source/sender/target agreement. It consumes the request only if the exact
ACK is sent and the active connection generation is still current. Every
non-XSA1 or rejected XSA1 datagram remains visible to the guest unchanged.
XSA1 is a local reachability and state shim; it does not provide cryptographic
authentication, confidentiality, or integrity.

## Mutation seams

`--aot_runtime_leg_destination_repair=true` enables the guarded seam at
`0x823A0CC8`. The runtime rechecks the title, module hash, full instruction
sentinel through `0x823A0CF8`, object ranges, event type, and empty destination.
Only then it writes the configured peer IPv4 value at `leg+0x78` and port
`0x1771` at `leg+0x7E`. This is a guest-memory mutation.

`--aot_runtime_xport_control_load_repair=true` enables the separate guarded
seam at `0x8239D6C4`. The sentinel spans `0x8239D6B8` through `0x8239D6CC` and
requires B19's patched predecessor `0x39600000`; the retail predecessor
`0x897F0680` is intentionally rejected. When guest `r11` is exactly zero and
the guarded row byte at `r31+0x680` is one, the hook changes only guest `r11`
to one for the untouched compare. Guest memory is not modified.

## Evidence boundary

The accepted historical B19 run exercised predecessor behavior from the
private research fork. This clean extraction has focused source guards, fake
transport unit tests, and build validation, but still requires a cold,
two-instance physical-controller acceptance run before release claims can be
made. It contains no game image, save, backend secret, machine-specific path,
personal identifier, diagnostic probe, or runtime artifact.
