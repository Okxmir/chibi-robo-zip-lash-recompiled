# Building Chibi-Robo! Zip Lash

Use your own decrypted European cartridge image for Chibi-Robo! Zip Lash (`CTR-P-BXLP`). The first build downloads pinned dependencies and prepares the game data on your PC.

## Windows x64

- Windows x64 and Visual Studio with the **Desktop development with C++** workload, including **C++ CMake tools for Windows** and a Windows SDK.
- Python 3.11 or newer with pip. The `python` command must work in Command Prompt.

Open the source folder in File Explorer, type `cmd` in the address bar, and press Enter. Run the command using your cartridge filename or its full path.

```cmd
build.cmd -Cartridge "YourRom.3ds" -Language en
```

The build verifies the cartridge, recompiles its code, runs the tests, and prepares `dist\ChibiRoboZipLash.exe`. The first build can take several minutes. Once it finishes, open the executable in File Explorer. Keep the project folder intact so the executable can find `extracted\CHIBI-EUR.cxi` alongside the `dist` directory. The executable also accepts the path to that local partition as an argument.

After a successful build, you can change the language by running `.\scripts\package.ps1 -Language fr` in PowerShell from the project folder.

Saves are under `dist\userdata`. Keep that folder to preserve your progress when rebuilding.

For a faster development build, add `-Fast`. To reuse an existing compatible dependency cache, pass its absolute path through `-Upstream`.

### Individual steps

```powershell
.\scripts\prepare.ps1 -Cartridge "YourCartridge.3ds"
.\scripts\prepare_font.ps1
.\scripts\build_release.ps1
.\scripts\run_tests.ps1
.\scripts\package.ps1 -Language en
```

Use `scripts/build_debug.ps1` and `scripts/run_tests.ps1 -Configuration Debug` for a Debug build.

## Linux x86-64

Use a C++20 compiler, CMake 3.24 or newer, Ninja, Python 3.11 or newer with Pillow and PyPNG, and Rust 1.90 or newer with Cargo. Install the packages below on Ubuntu 24.04. Use your distribution's equivalents elsewhere.

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config ca-certificates curl \
  python3 python3-pil python3-png libssl-dev libx11-dev libxext-dev libxrandr-dev \
  libxi-dev libxcursor-dev libxfixes-dev libxss-dev libxtst-dev libxkbcommon-dev \
  libwayland-dev libegl-dev libdecor-0-dev libasound2-dev libpulse-dev libvulkan-dev
```

If needed, install Rust from the [official Rust installer](https://rustup.rs/) and open a new terminal so that `cargo` is available.

From the project folder, run the build with your cartridge filename or its full path.

```bash
bash build.sh --cartridge "YourRom.3ds" --language en
```

Launch the game after the build passes its tests.

```bash
./dist/linux-x64/ChibiRoboZipLash
```

Keep the project folder intact so the game can find `extracted/CHIBI-EUR.cxi`. Keep `dist/linux-x64/userdata` to preserve saves. Install a Vulkan driver for your GPU to use hardware rendering and frame interpolation. On systems with limited build memory, run `CMAKE_BUILD_PARALLEL_LEVEL=2 bash build.sh --cartridge "YourRom.3ds" --language en` to limit compilation to two jobs.

## Game languages

| Code | Game language |
| --- | --- |
| `en` | English |
| `fr` | French |
| `de` | German |
| `es` | Spanish |
| `it` | Italian |

Windows uses `-Language` and Linux uses `--language`. Omit the option to keep an existing profile's language or use English for a new profile. Close the game before changing the language.

## Diagnostics

Set `CHIBI_DATA_DIR` to a separate folder when testing.

Set `CHIBI_FPS=1` before launching to log original top-screen framebuffer updates and the frames delivered by Vulkan's interpolation queue. The GPU transfer counter can include multiple transfers within one displayed frame.

Pass `--verify-gamepad` to the executable to test the default SDL mapping with a virtual controller. Pass `--poll-gamepad` to check a connected controller. During the physical check, move the left stick and press the east and north face buttons.
