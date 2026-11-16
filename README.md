# Containerize

Containerize lets you use Skyrim items and furniture as storage. Download it and check the game requirements on the [Nexus mod page](https://www.nexusmods.com/skyrimspecialedition/mods/110297). Set up your first container with the [preset guide](https://github.com/QTR-Modding/Containerize/wiki).

## Use Containerize from another SKSE plugin

The [Containerize API guide](https://github.com/QTR-Modding/Containerize/wiki/API) shows how to rent chests and use storage linked to items.

## Build from source

Use CMake 3.21 or later, Ninja, and an x64 MSVC toolchain with C++23 support. Set `COMMONLIB_SSE_FOLDER` to the root of your compatible CommonLibSSE checkout and `VCPKG_ROOT` to the root of your vcpkg checkout. Run these commands from the repository root in an x64 Visual Studio developer shell.

```powershell
cmake --preset release
cmake --build build/release
```

The plugin is built in `build/release`. To also copy it to your mod manager, set `SKYRIM_MODS_FOLDER` to the directory that contains your installed mods.

The [wiki source](docs/wiki/Home.md) syncs to the published wiki when changes to it reach `main`.
