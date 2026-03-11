# Discord Bot Plugin for RUNE

This plugin adds Discord bot nodes to RUNE so you can build flows that connect to Discord, listen for messages and reactions, send embeds, DMs, and all that. Its built on the [rune_plugin_sdk](https://github.com/blazium-games/rune_plugin_sdk) and serves as a more advanced example than the math or timer plugins. If youve already messed with those and want to see how to wire up external libs, async stuff, and event listeners, this is the one to look at.

You need RUNE installed first. Grab it from [bioblaze.blazium.games/rune-interface](https://bioblaze.blazium.games/rune-interface). Docs for the editor itself are at [blazium-games.github.io/rune_docs](https://blazium-games.github.io/rune_docs/).

## What You Get

**Event nodes** (Discord/Events): On Ready, On Message, On Reaction. These fire when stuff happens in Discord.

**Action nodes** (Discord/Actions): Connect Discord, Send Message, Send Embed, Reply to Message, Send Direct Message, Set Presence

**Data nodes** (Discord/Data): Get User, Get Channel, Build Embed. For pulling user/channel info and constructing embeds before you send them.

Token resolution: the bot token can come from plugin settings, a node property, flow env (DISCORD_TOKEN), or app env. Whichever it finds first.

Intent flags: per-intent toggles in the plugin settings so you can turn on message content or guild members or whatever you need without messing with bitmasks.

Auto-connect: when a flow loads that has an On Ready node, the plugin will connect automatically if youve got auto_connect enabled in settings.

## Prerequisites

- RUNE installed (from the itch link above)
- CMake 3.16 or newer
- C++17 compiler (MSVC, GCC, Clang all work)
- Git with submodules. Clone with `--recursive` or youll be missing DPP and the rest

## Build

Clone the repo with submodules:

```
git clone --recursive https://github.com/blazium-games/rune_discord_plugin.git
cd rune_discord_plugin
```

**Windows** (Visual Studio 2022):

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target rune_discord_plugin
```

**Linux**:

```
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target rune_discord_plugin
```

Output goes to the `dist/` folder. You get the plugin DLL (or .so on Linux), the DPP library, and plugin.json. Copy the whole dist folder into wherever RUNE keeps its plugins.

## Install

Copy everything in `dist/` into RUNEs plugins directory. On first launch RUNE will pick it up and you should see the Discord nodes under Discord/Events, Discord/Actions, Discord/Data.

## SDK Patterns Used (Advanced Stuff)

This plugin shows a few patterns that the simpler examples dont. Worth skimming the code if youre building something similar.

**External dependencies**: DPP (D++ Discord Library), OpenSSL, zlib, nlohmann/json. All vendored and built via CMake ExternalProject. The CMakeLists handles the whole thing so you dont need to install them manually.

**Event/listener nodes**: On Message and On Reaction use `start_listening` and `stop_listening` from the plugin API. The BotManager singleton handles DPP callbacks and queues events. When a node starts listening it registers a callback; when the event fires the callback runs and populates the node outputs, then triggers the execution pin. The event queue is teh important part because DPP runs on its own thread and RUNE expects node execution on the main thread.

**Async**: DPP callbacks run on a background thread. The BotManager queues events and they get processed on the main thread during `on_tick`. So the plugin never blocks RUNE while waiting for Discord.

**Plugin settings**: `get_settings_schema` returns a JSON schema for the settings UI. `on_settings_changed` gets called when the user saves settings. The plugin parses the JSON and updates its config (token, auto_connect, intent flags, etc).

**Token resolution**: `ResolveDiscordToken` checks the node property first, then flow env, then app env, then plugin settings. Lets you override per-flow or per-node without exposing secrets.

## Project Layout

```
rune_discord_plugin/
  include/       - discord_plugin.h, bot_manager.h
  src/           - discord_plugin.cpp, bot_manager.cpp
  src/nodes/      - events/, actions/, data/
  vendored/       - DPP, OpenSSL, zlib, json
  rune_plugin_sdk/  - submodule
  plugin.json
  CMakeLists.txt
```

Headers in include. Node implementations split by type: events (On Ready, On Message, On Reaction), actions (Connect, Send Message, etc), data (Get User, Get Channel, Build Embed).

## Going Further

For the basics of how to write a plugin from scratch check out the [rune_plugin_sdk](https://github.com/blazium-games/rune_plugin_sdk) and its examples. Math plugin is pure data, timer plugin has events and async. This one is a step up from those.

Rune docs for the editor: [blazium-games.github.io/rune_docs](https://blazium-games.github.io/rune_docs/).

If something breaks or acts wierd check the docs or dig into the code. The structure is pretty straightforward once you get past the CMake setup.
