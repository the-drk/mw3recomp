# mw3recomp

### Modern Warfare 3 · Xbox 360 → native runtime · eventual PS4 port

Static recompilation research for **Call of Duty: Modern Warfare 3 (2011)**,
focused on the offline single-player campaign. [XenonRecomp](https://github.com/hedge-dev/XenonRecomp)
translates the PowerPC executable into C++, which is compiled with a native host
runtime derived from [mw2-recompiled](https://github.com/PaulCombal/mw2-recompiled).
This is not a conventional Xbox 360 emulator.

[Local diagnostics](docs/MW3-LOCAL-RUN.md) · [Build guide](docs/MW3-BUILD.md) ·
[Validation workflow](.github/workflows/mw3-validation.yml) · [Bring-up history](docs/MW3-BRINGUP.md)

> [!IMPORTANT]
> **Research build — status: 14 November 2026.** MW3 completes a bounded
> **60-second headless startup run** with normal exit. A separate software-Vulkan
> run produced verified **1280×720 startup and first-run brightness-screen captures**. Main-menu boot,
> audible audio, and gameplay are **not verified**.
> There is **no playable release, PS4 executable, or FPKG**.

## Current milestone

The supported campaign executable, with a complete local disc extraction,
completed both 12-second and 60-second diagnostic runs under Ubuntu 24.04 on
WSL 1. The longer run ended at its deadline with **exit code 0**:

| Diagnostic | 60-second result |
|---|---:|
| GPU swap commands | 3,160 |
| Draw commands | 166,629 |
| GPU packets processed | 994,587 |
| Distinct shaders loaded | 11 |
| Unimplemented imports reached | 0 |
| Unknown opcodes / abandoned ring batches | 0 / 0 |

These are **headless command-processing counts**, not rendered frames or a
playable frame-rate measurement. Audio callbacks ran, but output was discarded
and no XMA frames were decoded. Only known optional patch files were reported
missing; no dirty-disc failure appeared in these runs.

### What changed

- **Memory-layout corruption fixed:** MW3's function lookup table overlapped
  the old general heap. Moving the heap to `0x85000000` stopped the observed
  failed indirect call; a compile-time guard now rejects overlapping layouts.
- **WSL 1 memory backing:** an immediately unlinked temporary file is used only
  when `memfd_create` returns `ENOSYS`. Normal Linux keeps the memfd path.
- **Older CPU diagnostics:** explicit baseline x86-64 builds can run without
  the default x86-64-v2 instruction requirements.
- **Asset-free validation:** runtime tests, static checks, relay tests, and public
  decoder/recompiler builds run in GitHub Actions.

### Rendering milestone and remaining blocker

The installed software Vulkan driver calls `memfd_create`, which returns
`ENOSYS` on WSL 1, then attempts to map the invalid descriptor and crashes.
An asset-free probe reproduces this independently of MW3. An opt-in diagnostic
shim, `tools/wsl1_memfd_probe_shim.c`, substitutes an unlinked temporary file;
it is not a complete memfd implementation and does not emulate sealing.

With that shim, a separate 60-second software-rendering run exited normally and
produced three non-black 1280×720 captures. An inspected capture shows a green
waveform-style startup visual, **not a verified menu**. The renderer recorded
201 frames and 11,302 draws, with nine textures uploaded and no upload failures.
Software rendering is slow; these results are not a playable performance claim.
Captures and raw logs remain local. A subsequent 60-second test with the no-SDL
scripted controller and Start/A presses reached the Japanese first-run brightness
screen, verified from captures. It exited normally and decoded 365 XMA frames on
three contexts with zero decoding errors; audible output is still unverified.
The next 60-second slider/confirmation test showed brightness followed by a
screen-adjustment step with large horizontal arrows and an A-to-continue prompt.
Earlier brightness text remained visible underneath, so transition/clearing
correctness is still unresolved. That run exited normally with 326 renderer
frames, 19,468 recorded draws, 16 texture uploads with no failures, and 329 XMA
frames decoded with no errors. No unimplemented imports were reached.
Next: confirm the screen-adjustment step and inspect the following screen.
This is not main-menu validation.

A later 90-second run with additional A confirmations exited normally, but all
six sampled captures still showed brightness setup. It did not reproduce the
screen-adjustment view: progression is not reliable yet. That run recorded 584
renderer frames and 34,174 draws, with no texture upload or XMA decoding errors
and no reached unimplemented imports. Longer button-only confirmation is not a
demonstrated fix; input-delivery timing needs to be checked next.

The subsequent traced 90-second test returned every scheduled press and release
through the input import (18 state changes). Captures showed slider movement
and the screen-adjustment view, but no verified progression beyond it despite
later A presses. Missed script intervals do not explain this particular run.
It exited 0 with 458 renderer frames, 26,586 recorded draws, no texture upload
failures, 759 decoded XMA frames with zero errors, and no reached unimplemented
imports. Next: investigate setup confirmation/UI state handling and the
overlapping rendering, rather than assuming more button presses will fix it.

A profile-read ABI correction (the missing stored-setting data-type byte) passed
synthetic tests and was rebuilt locally. The same 90-second input schedule still
ended at screen adjustment with overlapping UI, despite all 18 input state
changes being delivered. No profile read/write messages appeared, so exercising
the corrected stored-setting path is not established. This did not demonstrate
a fix for setup progression; main-menu arrival remains unverified.

## Progress at a glance

| Area | Evidence / status |
|---|---|
| Supported executable | Campaign `iw5sp_ship.exe`, title ID `0x415608CB`, XEX version `0x16` |
| Static recompilation | Generated code compiles; diagnostic desktop runner links |
| Guest execution | Sustained 60-second headless run, normal deadline exit |
| Kernel / XAM | 258 imports inventoried: 224 implemented, 34 logging stubs; none of those stubs reached in the validated runs |
| Graphics command processing | Swaps, draws, shaders, interrupts and ring progress observed |
| Actual rendering | Non-black 1280×720 startup visual captured using software Vulkan and a diagnostic WSL 1 shim |
| Audio | 365 XMA frames decoded with zero errors in the input run; audible output unverified |
| Startup UI / input | Brightness followed by screen adjustment captured with scripted input; overlapping UI remains unresolved |
| Main menu / campaign / saves | Not demonstrated |
| PS4 port and packaging | Not started |

Import counts are name-level inventory, **not proof of correct semantics**.
Historical investigation documents may describe older failures and milestones;
the current bounded-run procedure and evidence are in
[MW3-LOCAL-RUN.md](docs/MW3-LOCAL-RUN.md).

## Game files stay on your machine

This repository does not distribute game content.

- Supply your own legally obtained disc dump under the ignored `mw3/` directory.
- Never upload game assets, extracted images, generated PPC code/headers, or
  game-derived build artifacts to GitHub, Actions, Git LFS, or an asset repository.
- CI uses public source and synthetic test inputs only. Public dependency
  artifacts contain no game code.
- Keep raw runtime logs private and review/redact diagnostics before sharing.
- Do not create placeholder game files to hide missing assets.

[`tools/check_no_game_files.py`](tools/check_no_game_files.py) checks tracked
files and LFS rules in CI; its `--staged` mode is available for pre-commit checks.

## Development workflow

### Public checks → GitHub Actions

The [validation workflow](.github/workflows/mw3-validation.yml) runs static
checks, asset-free runtime tests, relay tests, and public XMA decoder and pinned
XenonRecomp builds. Use Actions for independent public work rather than making a
low-powered development machine repeat every build.

For contributors who need to reproduce the asset-free tests locally:

```sh
cmake -S . -B build-tests -G Ninja \
  -DMW2_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tests --parallel 2
ctest --test-dir build-tests --output-on-failure
```

Tests use synthetic inputs and stub guest headers, not extracted game headers.
Prerequisites: CMake 3.20+, Ninja, a C++20 compiler, and Python 3.11+ for tooling.

### Private game-dependent work → local machine

1. Stage the supported campaign XEX at `mw3/tu0/default.xex` and retain the full
   disc directory structure for live runs.
2. Install Clang 18, CMake, Ninja, Python, Git, binutils and Vulkan development
   headers as described in the [local guide](docs/MW3-LOCAL-RUN.md).
3. Verify and recompile your executable:

   ```sh
   bash tools/build_mw3.sh
   ```

This generates and builds game-derived code and audit reports; it does **not**
produce a playable game. Follow [MW3-LOCAL-RUN.md](docs/MW3-LOCAL-RUN.md) to
configure/link the diagnostic runner and perform a bounded run.

Linux x86-64 is the development target. WSL 1 has been validated for the
documented **headless** path, not accelerated rendering. On older CPUs select
`MW2_X86_LEVEL=x86-64`; use a single build job on constrained machines. Keep WSL
builds in the Linux filesystem rather than `/mnt/c`.

The supported XEX SHA-256 is:

```text
d90bc8d93b7b8f6001ead8330271196a6ef7a5c2e4a9e9b63118f47d58a05912
```

## Roadmap

1. **Startup progression — next:** investigate progression beyond the captured
   startup visual, validate input, and check rendering correctness.
2. **Desktop menu:** establish visual correctness, input, audio output, and
   reliable startup. Fix runtime semantics based on observed execution.
3. **Campaign:** load the first mission; validate gameplay, saves, and performance.
4. **PS4 feasibility and port:** only after desktop evidence, investigate
   OpenOrbis integration, memory mapping, graphics backend compatibility,
   controller/audio support, profiling, and packaging.

Multiplayer services and Special Ops are outside the current scope. A passing
build or smoke test is not a promise of compatibility or a release date.

## Architecture and investigation notes

The pipeline verifies the XEX, extracts a flat image, analyses jump tables,
generates C++, and links the result with guest memory, kernel/XAM handlers,
threads, DPC delivery, GPU command processing and audio support. MW3-specific
addresses must be validated against MW3; MW2 hooks are not interchangeable.

- [Build and local workflow](docs/MW3-BUILD.md), [bounded startup tests](docs/MW3-LOCAL-RUN.md)
- [Runtime architecture](docs/runtime.md), [rendering](docs/rendering.md), [audio](docs/audio.md)
- [Bring-up history](docs/MW3-BRINGUP.md), [synchronization validation](docs/MW3-SYNC-VALIDATION.md)
- [DPC investigation](docs/MW3-DPC-INVESTIGATION.md), [startup-import plan](docs/MW3-STARTUP-IMPORT-PLAN.md)
- [D3D9 integration](docs/d3d9-seam.md)

## Credits

- [mw2-recompiled](https://github.com/PaulCombal/mw2-recompiled) — runtime and recompilation foundation
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) — PowerPC static recompiler
- [Xenia](https://github.com/xenia-project/xenia) — compatibility reference
- [LibertyRecomp](https://github.com/OZORDI/LibertyRecomp) and
  [gta4-ps4](https://github.com/plastikman/gta4-ps4) — related recompilation and porting work

## License

Code is licensed under [GPL-3.0-only](LICENSE). Game content belongs to its
respective owners and is not included.
