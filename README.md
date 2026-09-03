# Xenia — Army of Two: The 40th Day co-op

I built this because I wanted to play co-op without split-screen with my family; the community offers really no path to enabling networking so you're forced to play split screen in a sub 720p game - which is otherwise a fantastic game to play with a friend. This one PC is really the only testing environment that makes sense for me. I'm sure you could get this running so you can play on two different PC's or however you want, but a powerful PC with two monitors makes for a really run couch/nostalgia day and you can play in the same room with a friend! 

## Same-PC co-op

This is the custom Xenia emulator component for **Army of Two: The 40th Day co-op on one Windows PC**. Each player runs a separate game instance with their own controller, profile, and screen. The setup also uses local companion services.

Start with the [AoT same-PC co-op project](https://github.com/eternalgr3y/aot-40th-day-same-pc-coop) for the project overview, configuration, companion components, and current release information.

**Status: experimental source alpha.** A ready-to-play package is still being prepared. The published emulator candidate has not completed the project's full two-controller gameplay and checkpoint-reload acceptance tests. The [curated source release](https://github.com/eternalgr3y/aot-40th-day-same-pc-coop/releases/tag/v0.1.0-source-alpha.2) is available for review; it does not include an emulator executable.

## What this fork adds

- Distinct local network identities so both instances can run on the same PC.
- A separate SDL controller selection for each player, with optional right-stick X inversion.
- A bounded connection handshake and two guarded compatibility repairs for the supported AoT game build.

See [AOT_RUNTIME_CORE.md](AOT_RUNTIME_CORE.md) for the implementation and [PUBLIC_ALPHA_BOUNDARY.md](PUBLIC_ALPHA_BOUNDARY.md) for the supported scope and validation limits.

## Building this emulator

Follow [the build instructions](docs/building.md) to install the development dependencies, then run:

```text
xb setup
xb build --config=release --target=xenia-app
```

The emulator build is one component of the co-op setup. Use the [main project](https://github.com/eternalgr3y/aot-40th-day-same-pc-coop) for the launcher and local services.

## Credits

Built on [Xenia](https://github.com/xenia-project/xenia), [Xenia Canary](https://github.com/xenia-canary/xenia-canary), and [AdrianCassar's Netplay fork](https://github.com/AdrianCassar/xenia-canary). The Netplay source base is `b5f6f6ed618210ecfbbcb228994418f734cdd850`.

Thanks to the upstream contributors whose work makes this project possible. See [LICENSE](LICENSE) and the notices included with third-party components.
