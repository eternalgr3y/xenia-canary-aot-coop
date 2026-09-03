# Xenia Same-PC Netplay Foundation

I built this because I wanted to play co-op without split-screen with my family; the community offers really no path to enabling networking so you're forced to play split screen in a sub 720p game - which is otherwise a fantastic game to play with a friend. This one PC is really the only testing environment that makes sense for me. I'm sure you could get this running so you can play on two different PC's or however you want, but a powerful PC with two monitors makes for a really run couch/nostalgia day and you can play in the same room with a friend! 

This branch is a source candidate for running *Army of Two: The 40th Day* in
two Xenia instances on one Windows PC. It combines opt-in synthetic loopback
identities and per-instance SDL controller selection with a narrowly fenced
title-specific runtime core for the supported B19 retail build.

The runtime core contains a bounded secure-association handshake and two
default-off compatibility repairs. It deliberately excludes the historical
probe harness, broad guest traps, diagnostic fallbacks, and private runtime
artifacts. This reduced candidate is source- and build-tested; it has not yet
passed a clean two-controller runtime acceptance run. See
[`AOT_RUNTIME_CORE.md`](AOT_RUNTIME_CORE.md) and
[`PUBLIC_ALPHA_BOUNDARY.md`](PUBLIC_ALPHA_BOUNDARY.md) for the exact boundary.

The source contract runs in CI, and focused transport unit tests are included.
A local release build may be produced with
`xb setup` followed by `xb build --config=release --target=xenia-app`.
Successful compilation does not establish gameplay compatibility.

## Upstream lineage and resources

This work is based on AdrianCassar's Xenia Canary Netplay commit
`b5f6f6ed618210ecfbbcb228994418f734cdd850`, which is itself derived from Xenia
Canary. All release, buildbot, hosted-session, wiki, compatibility, and support
links in the preserved upstream README below belong to those upstream projects.
They do not distribute, host, or support this branch.

---

## Preserved upstream Xenia Canary Netplay README

This is a fork of [Xenia Canary](https://github.com/xenia-canary/xenia-canary) which implements online multiplayer features. The REST API powering this fork can be found [here](https://github.com/AdrianCassar/Xenia-WebServices#xenia-web-services).

Current online sessions are displayed at [https://xenia-netplay-2a0298c0e3f4.herokuapp.com/](https://xenia-netplay-2a0298c0e3f4.herokuapp.com/).

---

## Netplay Wiki

The wiki contains a compatibility list and general information about the current state netplay.

To get started with netplay read [config setup](https://github.com/AdrianCassar/xenia-canary/wiki/Config-Setup).

### Wiki Pages:
* [Home](https://github.com/AdrianCassar/xenia-canary/wiki)
* [Netplay Compatibility](https://github.com/AdrianCassar/xenia-canary/wiki/Netplay-Compatibility)
* [Testing Procedure](https://github.com/AdrianCassar/xenia-canary/wiki/Testing-Procedure)
* [Config Setup](https://github.com/AdrianCassar/xenia-canary/wiki/Config-Setup)
* [Systemlink](https://github.com/AdrianCassar/xenia-canary/wiki/Systemlink)
* [FAQ](https://github.com/AdrianCassar/xenia-canary/wiki/FAQ)

### Netplay Builds:
* [Latest stable release](https://github.com/AdrianCassar/xenia-canary/releases/latest)
* [Github Actions](https://github.com/AdrianCassar/xenia-canary/actions?query=actor%3AAdrianCassar+branch%3Anetplay_canary_experimental)
    * These builds are latest work in progress and require a github account to download.
* [Mousehook](https://github.com/marinesciencedude/xenia-canary-mousehook/releases?q=Netplay)
    * A limited number of games have netplay and mousehook support.

---

<p align="center">
    <a href="https://github.com/xenia-canary/xenia-canary/tree/canary_experimental/assets/icon">
        <img height="256px" src="https://raw.githubusercontent.com/xenia-canary/xenia/master/assets/icon/256.png" />
    </a>
</p>

<h1 align="center">Xenia Canary - Xbox 360 Emulator</h1>

Xenia Canary is an experimental fork of the Xenia emulator. For more information, see the
[Xenia Canary wiki](https://github.com/xenia-canary/xenia-canary/wiki).

Come chat with us about **emulator-related topics** on [Discord](https://discord.gg/Q9mxZf9).
For developer chat join `#dev` but stay on topic. Lurking is not only fine, but encouraged!
Please check the [FAQ](https://github.com/xenia-canary/xenia-canary/wiki/FAQ) page before asking questions.
We've got jobs/lives/etc, so don't expect instant answers.

Discussing illegal activities will get you banned.

## Status

Buildbot | Status | Releases
-------- | ------ | --------
Canary (🪟, 🐧) | [![CI](https://github.com/xenia-canary/xenia-canary/actions/workflows/Orchestrator.yml/badge.svg?branch=canary_experimental)](https://github.com/xenia-canary/xenia-canary/actions/workflows/Orchestrator.yml/badge.svg?branch=canary_experimental) [![Codacy Badge](https://app.codacy.com/project/badge/Grade/cd506034fd8148309a45034925648499)](https://app.codacy.com/gh/xenia-canary/xenia-canary/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade) | [Latest](https://github.com/xenia-canary/xenia-canary/releases/latest) ◦ [All](https://github.com/xenia-canary/xenia-canary/releases) ◦ [Old](https://github.com/xenia-canary/xenia-canary-releases/releases)

### Experimental Netplay

Buildbot | Status | Releases
-------- | ------ | --------
Windows | [![Codacy Badge](https://app.codacy.com/project/badge/Grade/d814c4b6aa444dcc9c1631e0224b2739)](https://app.codacy.com/gh/AdrianCassar/xenia-canary/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade) | [Latest](https://github.com/AdrianCassar/xenia-canary/releases/latest)

## Quickstart

See the [Quickstart](https://github.com/xenia-canary/xenia-canary/wiki/Quickstart) page.

## FAQ

See the [frequently asked questions](https://github.com/xenia-canary/xenia-canary/wiki/FAQ) page.

## Game Compatibility

See the [Game compatibility list](https://github.com/xenia-canary/game-compatibility/issues)
for currently tracked games, and feel free to contribute your own updates,
screenshots, and information there following the [existing conventions](https://github.com/xenia-canary/game-compatibility/blob/canary/README.md).

## Building

See [building.md](docs/building.md) for setup and information about the
`xb` script. When writing code, check the [style guide](docs/style_guide.md)
and be sure to run clang-format!

## Contributors Wanted!

Have some spare time, know advanced C++, and want to write an emulator?
Contribute! There's a ton of work that needs to be done, a lot of which
is wide open greenfield fun.

**For general rules and guidelines please see [CONTRIBUTING.md](.github/CONTRIBUTING.md).**

Fixes and optimizations are always welcome (please!), but in addition to
that there are some major work areas still untouched:

* Help work through [missing functionality/bugs in games](https://github.com/xenia-canary/xenia-canary/labels/compat)
* Reduce the size of Xenia's [huge log files](https://github.com/xenia-canary/xenia-canary/issues/1526)
* Skilled with Linux? A strong contributor is needed to [help with porting](https://github.com/xenia-canary/xenia-canary/labels/platform-linux)

See more projects [good for contributors](https://github.com/xenia-canary/xenia-canary/labels/good%20first%20issue). It's a good idea to ask on Discord and check the issues page before beginning work on
something.

## Disclaimer

The goal of this project is to experiment, research, and educate on the topic
of emulation of modern devices and operating systems. **It is not for enabling
illegal activity**. All information is obtained via reverse engineering of
legally purchased devices and games and information made public on the internet
(you'd be surprised what's indexed on Google...).
