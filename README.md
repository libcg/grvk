# GRVK

GRVK is a Mantle to Vulkan translation layer.

[Mantle](https://en.wikipedia.org/wiki/Mantle_(API)) was originally developed by AMD and DICE starting in 2013, in an effort to produce a low-overhead graphics API as an alternative to [DirectX 11](https://en.wikipedia.org/wiki/DirectX#DirectX_11) and [OpenGL 4](https://en.wikipedia.org/wiki/OpenGL#OpenGL_4.4). Mantle was discontinued in 2015, with only [a few games](https://en.wikipedia.org/wiki/Category:Video_games_that_support_Mantle_(API)) ever supporting it. Support was dropped from the AMD drivers in 2019, and it was never compatible with Nvidia cards. Despite this short life, it spawned a new generation of graphics API, including [Metal](https://en.wikipedia.org/wiki/Metal_(API)), [DirectX 12](https://en.wikipedia.org/wiki/DirectX#DirectX_12) and [Vulkan](https://en.wikipedia.org/wiki/Vulkan_(API)).

This project is an attempt to revive Mantle and make it run everywhere.

[Current game support](https://github.com/libcg/grvk/wiki/Game-Support)

## Getting the latest build

Head over to the [Releases page](https://github.com/libcg/grvk/releases) for the latest stable build.
Development builds are available [here](https://github.com/libcg/grvk/actions?query=branch%3Amaster).

## Building manually

### Requirements

- [mingw-w64](https://www.mingw-w64.org) compiler
- [Meson](https://mesonbuild.com/) build system
- Vulkan 1.3 compatible GPU and recent drivers ([see wiki](https://github.com/libcg/grvk/wiki/Driver-Support))

NOTE: `binutils` 2.34 has [known issues](https://github.com/doitsujin/dxvk/issues/1625) and should be avoided.

### Compiling

```
# 32-bit
meson --cross-file build-win32.txt --prefix $(pwd) build.w32
cd build.w32
ninja

# 64-bit
meson --cross-file build-win64.txt --prefix $(pwd) build.w64
cd build.w64
ninja
```

`mantle32.dll`/`mantle64.dll`/`mantleaxl32.dll`/`mantleaxl64.dll` will be generated.

## Usage

After dropping the DLLs in the game directory, GRVK will get loaded by the game at launch. By default, GRVK will create log files named `grvk.log`/`grvk_axl.log` in the same directory.

### Environment variables

- `GRVK_LOG_LEVEL` controls the log level. Acceptable values are `trace`, `verbose`, `debug`, `info`, `warning`, `error` or `none`.
- `GRVK_LOG_PATH` controls the log file path. An empty string will disable logging to the file entirely.
- `GRVK_AXL_LOG_PATH` similar to `GRVK_LOG_PATH`, but for the extension library (mantleaxl).
- `GRVK_DUMP_SHADERS` controls whether to dump shaders (IL input, IL disassembly, and SPIR-V output). Pass `1` to enable.

## Credits

- [Philip Rebohle](https://github.com/doitsujin/) and [Joshua Ashton](https://github.com/Joshua-Ashton) for [DXVK](https://github.com/doitsujin/dxvk), a DirectX to Vulkan translation layer
- [Alexander Overvoorde](https://github.com/Overv) for the [MantleHelloTriangle](https://github.com/Overv/MantleHelloTriangle) example used to bring up GRVK
- AMD for making the API and publicly releasing the [Mantle specification](https://drive.google.com/file/d/13AbMuQltP8t-XabtmnTATlDGj5aGHRyq/view)
