# Chibi-Robo! Zip Lash Recompiled

An experimental Windows x64 and Linux x86-64 port of the European Nintendo 3DS release. The build statically translates the original ARM and Thumb program and loads assets from a local copy of the cartridge.

## Play from a release

Download the Windows x64 or Linux x86-64 RAR from the [releases page](https://github.com/Okxmir/chibi-robo-zip-lash-recompiled/releases) and extract the whole archive.

The prebuilt Linux release was tested on Ubuntu 24.04 and requires glibc 2.38 or newer, libstdc++, and OpenSSL 3.

## Build from source

On Windows x64, install Visual Studio with the **Desktop development with C++** workload and CMake tools, and install **Python 3.11 or newer** with pip. Put your decrypted European cartridge image (`CTR-P-BXLP`) in the project folder.

Open the project folder in File Explorer, type `cmd` in the address bar, and press Enter. Run this command with your cartridge filename.

```cmd
build.cmd -Cartridge "YourRom.3ds" -Language en
```

Open `dist\ChibiRoboZipLash.exe` when the build finishes.

On Linux x86-64, install the prerequisites in the [build guide](BUILD.md#linux-x86-64) and open a terminal in the project folder.

```bash
bash build.sh --cartridge "YourRom.3ds" --language en
./dist/linux-x64/ChibiRoboZipLash
```

Choose `en` for English, `fr` for French, `de` for German, `es` for Spanish, or `it` for Italian. The first build downloads its dependencies and prepares the game data locally. The repository includes the port source and build scripts. [Full build guide](BUILD.md)

## Controls

| 3DS input | Default PC binding |
| --- | --- |
| Circle Pad | WASD positions (ZQSD on AZERTY) or left stick |
| D-Pad | Arrow keys or controller D-pad |
| A / B / X / Y | K / J / I / U or the east / south / north / west controller buttons |
| L / R | Q / E positions or controller shoulder buttons |
| Start / Select | Enter / Backspace or controller Start / Back |
| Touch screen | Click the lower screen |
| Rotate the system | Hold T or move the right stick right to turn clockwise. Hold G or move the right stick left to turn counterclockwise. |

The Vulkan renderer enables frame interpolation for new profiles. It produces intermediate images between the game's own frames while keeping the original simulation timing. Press F4 to toggle interpolation and save the setting for the current profile.
