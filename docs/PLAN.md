# Plan and design log - Retro Engine v3 core

Dated entries; the newest decisions are at the end of each section.

## The request (2026-10-17)

Chimera issue 295 asked for three cores: the Retro Engine v3, v4 and v5
decompilations (Sonic CD; Sonic 1 and 2; Sonic Mania). The owner asked for
each to be analysed and built if it is technically and legally sound, and
for the work to stay local: nothing pushed, no comment on the issue. The
owner chose three cores, one per engine (the three code bases share global
names, so one sandbox cannot hold more than one), and supplied the games'
data files for testing: Sonic CD's Android APK, Sonic 1 and 2's Android
APKs, and Sonic Mania's Android files.

## Feasibility

- **Licence.** The decompilation's licence (LICENSE-RSDK.txt) allows copying,
  changing and distributing, for public or private use, as long as: a build
  given to anyone has the downloadable content off by default; nothing is
  used for profit; the decompilation and the original are credited; no game
  asset is distributed; the licence travels with modified source. Chimera is
  free and its packages carry no game, so all five hold: the core defines
  `RSDK_AUTOBUILD`, the package declares `commercialUse: no`, the credits are
  in the README, LICENSE and the package, and LICENSE-RSDK.txt is in the
  repository and the package. The repository's own files are MIT, which
  adds no condition of its own to the decompilation's.
- **One thread on one clock.** The engine is single-threaded but for SDL's
  sound callback and the video decoder's thread. Its main loop is one frame
  a pass (RetroEngine::Run); the core runs that pass (RunFrame, patch 0001)
  and calls the mixer itself. Its only clocks are SDL's performance counter
  (pacing the loop, which the core does not run) and tick count (videos),
  and time() for the random seed: the frame count and a setting.
- **The picture.** A software renderer (the default upstream) into a 16-bit
  screen buffer; the hardware renderer is not built.
- **What the user brings.** The game's data file. Never committed or
  uploaded; the tests on it are skipped where it is absent.
- **What a runner can test.** A game written for the tests from scratch
  (waterbox/tests/make-test-game.py): a data file in the engine's own
  format, with a stage whose object is a script the engine compiles at load.
  Everything the core does is tested on it, in CI.

## How the core is built (2026-10-17)

- **SDL 2 for real**, built from source in its minimal configuration (dummy
  video, no threads, no timers), as the SDLPoP core builds it. The engine uses
  SDL's software renderer, its sound conversion (SDL_AudioStream resamples
  the music), its WAV reader and its file reader. What the core answers
  itself is wrapped at link time (seams.c): the clocks, the sound device,
  the keyboard state, presenting, and the renderer (the engine asks for an
  accelerated one; the dummy driver has only software, so it gets that).
- **Files** (rsdk-files.c). The engine opens everything through SDL's file
  reader, which opens with fopen. Both are wrapped. Data.rsdk is copied at
  Init into sealed memory and opened with fmemopen: the stream's position
  and buffer are guest memory, which a savestate carries. settings.ini is
  written from the project's settings. The saves (SData.bin) are memory
  streams, empty at start, carried by savestates. SDL_RWFromFile refuses a
  stream with no file descriptor, so it is answered directly
  (SDL_RWFromFP over the same stream).
- **Math.** libvorbis and the engine call sin, cos, tan, acos, atan2, exp,
  log and pow, whose last bit differs between glibc (the native reference)
  and musl (the sandbox). The link wraps them all into detmath.c (series
  from + - * / only); the compiler is told they are not builtins.
- **rand.** musl's generator in both builds.
- **Videos.** Not played: theoraplay decodes on a thread. A decoder that
  never starts (theora-stubs.c) makes the engine skip the video, as when
  the file is missing; no video file is ever given in any case.
- **Lag.** A frame reads the controls unless it is the one that loads a
  stage (ProcessStage's STAGEMODE_LOAD: no object runs).

## Measured (2026-10-17)

- Sonic CD, Android data (SHA-1 520B6982...): title, menu (a direction then
  A), and Palmtree Panic Zone act 1 played with the pad, native.
- 3600 frames (title to Palmtree Panic, running and jumping): native =
  sandbox (picture, sound, frame lengths, lag, memory); rerecord (a save
  and a load before every frame) = straight; a session (a state saved at
  frame 3240, loaded into a new process) = straight. State: 6.5 MB.
- Speed: 3600 frames in 2.8 s in the sandbox, 0.5 s per 600 natively.
- The test game: 100 frames, the same three tests; input, saves (written
  and read back, also across a session), pokes, settings (the random seed,
  the screen width), refusals.
- Frontend (chimera-run and Chimera headless): 9 checks. Contract tests 20.
- Each new test was broken on purpose and seen to fail (the input and lag,
  the saves, the pokes, the seed, the width, the equivalence, the session,
  turbo, the clock, the refusals, the firmware name, the frontend's game
  time and refusal). Rerecord and teeth were not broken this round.

## Not done

- The Steam release's data and videos: not tried (no copy here).
- Sonic Origins' data: not tried.
- Mods: the mod loader is off.
- Windows: the package has not been run on Windows yet.
- The Original Controls setting's two values were not checked in play.
