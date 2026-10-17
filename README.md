# Retro Engine v3 (Sonic CD) for Chimera

This repository builds the RSDKv3 decompilation, a decompilation of
Christian Whitehead's Retro Engine v3, as a game core for
[Chimera](https://github.com/ToolAssisted-run/chimera), a frontend for
tool-assisted speedruns. Retro Engine v3 is the engine of Sonic CD's 2011
remake. The core plays that game frame by frame, the same every time, with
savestates, rewinding and movies.

The build makes one file, `rsdkv3.chimeraCore`. Put it in Chimera's `Cores`
folder.

## What you bring

The game's data file, `Data.rsdk`, as the project's firmware. This package
contains none of the game.

- The Android release (checked): in its APK the file is `assets/Data.xmf`.
  Rename it to `Data.rsdk`. SHA-1 `520B69826B5FEA7AFA1C49B6AD5E5091D44C9601`.
- The Steam release's `Data.rsdk` is the same kind of file. It was not
  tried. A file of your own can be used in the original's place, and the
  project then records its hash.

## Settings

Language, screen width (320 to 424 pixels, 424 by default), the special
stages' high-resolution floor, the game's Original Controls option, and the random
seed. Each is recorded in the project.

## What it does not do yet

- The Steam release's videos (the opening and the endings, `.ogv` files) are
  not played; the engine skips them, as it does when the files are missing.
  The Android release's APK holds no .ogv file.
- Mods (the decompilation's mod loader) are off.
- The downloadable content is off in every build, as the decompilation's
  licence requires.

[docs/PLAN.md](docs/PLAN.md) says what was decided and measured,
[docs/BUILDING.md](docs/BUILDING.md) how to build and test it.

## Licence

This repository's own files are MIT. The engine is the RSDKv3 decompilation
(by Rubberduckycooly and st&times;tic, of the Retro Engine by Christian
"Taxman" Whitehead), under its own licence, in
[LICENSE-RSDK.txt](LICENSE-RSDK.txt): no use for profit, credit to its
authors, downloadable content off, no game assets distributed. A package
built from here is under those terms. [LICENSE](LICENSE) says what each part
is under.
