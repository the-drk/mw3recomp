# Running the MW3 smoke test on your own machine

The local runner builds and runs a bounded diagnostic smoke test on x86-64
Linux. Automatic PR CI does not receive game files.

```sh
bash tools/smoke_mw3.sh
```

It verifies the supported XEX before generation, builds and links the runner,
and checks guest entry and completion of its report. Disc/zone failures make
the command fail. Logs stay in private temporary files; no uploads occur.
Use `JOBS=2` to limit build parallelism and `MW2_RUN_SECONDS=60` for a longer
run. This is a diagnostic check, not proof of menu boot or gameplay.

Your game files never leave your machine and are never committed: `mw3/` is
git-ignored.

## What you need

- Ubuntu 24.04, or Windows with WSL running Ubuntu 24.04. WSL 1 has the
  diagnostic limitations and experimental software-rendering workaround below.
  On WSL, keep the
  repository and game files inside the Linux filesystem (`~/`), not under
  `/mnt/c`, which is many times slower.
- Plenty of free disk space and RAM: the game files are about 5 GB with all four
  image packs, and the recompile builds about 36,000 functions into a large
  library. I have not measured the build's own footprint; 20 GB free and 8 GB of
  RAM are guesses, and the compile is the step that fails first if they are short.
- Your own game files, extracted from your disc.

```sh
sudo apt-get update
sudo apt-get install -y clang-18 lld-18 ninja-build python3 cmake git binutils openssl ca-certificates libvulkan-dev
```

## 1. Get the code

```sh
git clone https://github.com/the-drk/mw3recomp.git
cd mw3recomp
git checkout mw3-bringup
```

## 2. Stage the game files

Put your files in `mw3/tu0/` keeping the disc's names and folders. The title
reads them as `D:\<name>`, so `default.xex` and the fastfiles and packs all sit
directly in that folder:

```
mw3/tu0/default.xex
mw3/tu0/code_post_gfx.ff
mw3/tu0/ui.ff
mw3/tu0/common.ff
mw3/tu0/imagefile1.pak
mw3/tu0/imagefile3.pak
mw3/tu0/imagefile4.pak     (0.95 GB, requested next)
mw3/tu0/imagefile2.pak     (1.89 GB, not requested yet)
```

Check that `default.xex` is the supported executable before anything else; the
build refuses any other:

```sh
sha256sum mw3/tu0/default.xex
# d90bc8d93b7b8f6001ead8330271196a6ef7a5c2e4a9e9b63118f47d58a05912
```

Do not create placeholders for files your dump lacks (`patch.ff`, `ui.ffm`,
`.pakm` files). The title asks for them, does not get them and carries on.

## 3. Recompile and build (about 10 minutes on a modern desktop)

```sh
bash tools/build_mw3.sh --compile-only
cmake -S . -B build-mw3 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
  -DMW2_TITLE=mw3 -DMW3_RECOMP_ONLY=ON -DMW3_LINK_PROBE=ON \
  -DMW2_USE_SDL=OFF -DMW2_LTO=OFF
cmake --build build-mw3 --target mw2 --parallel
```

The first command regenerates `ppc_mw3/` from your executable and compiles it.
The second and third link the diagnostic runtime to `build-mw3/mw3`.

## 4. Run it

```sh
MW2_RUN_SECONDS=12 MW2_LOG_FILE=/tmp/mw3.log \
  ./build-mw3/mw3 mw3/tu0/default.xex mw3/tu0
tail -n 120 /tmp/mw3.log
```

`MW2_RUN_SECONDS` ends the run and prints the end-of-run report after that many
seconds. Raise it (try 60) once the title stops failing early. Without SDL there
is no window or audible output, but Vulkan can render private frame captures
when a usable driver is available. The
renderer reports `no Vulkan instance` or `no device came up` when the machine
has no usable Vulkan driver, which does not stop the run.

## 5. Reading the log

Look for these, in this order:

1. `XamLoaderSetLaunchData: ... text "eERROR: Could not find zone 'X.ff'"`: the
   title's fatal-error handler naming a zone it could not open. Add that file.
2. `file not found: D:\<name>` followed by `the title reported a dirty disc`:
   a required file is missing. The last `file not found` before it is the one.
   `patch.ff`, `patch.ffm`, `ui.ffm` and `update:\patch.ff` are optional and
   always appear.
3. The end-of-run report: `gpu: N frames`, `N draws`, `audio: N callbacks`,
   `unimplemented imports actually reached`, and `files the guest asked for and
    did not get`. A normal bounded run reports
    `exiting: the run deadline was reached`; distinguish that from an early
    fatal-error exit and inspect the process exit status as well.

Logs can contain private paths and guest diagnostics. Review and redact them
before sharing; do not publish raw logs automatically.

## Where the run stands

### WSL 1 headless validation (2026-11-14)

On `bringup/sync-validation`, the supported campaign executable with the full
local disc extraction completed bounded 12-second and 60-second runs with exit
code 0. The 60-second run recorded 3,160 swap commands, 166,629 draw commands,
11 shaders, and no reached unimplemented imports. Only the optional patch files
were reported missing. These are command-processing counts, not rendered frames
or proof of menu boot, gameplay, audible sound, or PS4 readiness.

The MW3 heap now begins at `0x85000000`: the old `0x84600000` start overlapped
the generated function lookup table and corrupted an existing BINK function's
entry. A compile-time assertion rejects overlapping layouts.

This machine uses WSL 1 and an older AMD CPU. Configure with
`-DMW2_X86_LEVEL=x86-64 -DMW2_LTO=OFF -DMW2_PGO=OFF -DMW2_USE_SDL=OFF`
and build with `--parallel 1`. The memory backend falls back to an immediately
unlinked temporary file only when `memfd_create` returns `ENOSYS`.

The installed Mesa software Vulkan driver crashed because its `memfd_create`
call returned `ENOSYS`, followed by attempts to resize/map descriptor `-1`.
For command-processing-only diagnostics, the following bypasses driver discovery:

```sh
VK_DRIVER_FILES=/nonexistent/mw3-no-vulkan.json \
MW2_RUN_SECONDS=60 MW2_LOG_FILE=/root/mw3-headless.log \
  ./build-mw3/mw3 mw3/tu0/default.xex mw3/tu0
```

### Experimental WSL 1 software-rendered startup

`tools/wsl1_memfd_probe_shim.c` is an opt-in diagnostic `LD_PRELOAD` shim that
substitutes an unlinked temporary file only on `ENOSYS`. It is not a complete
memfd implementation, does not emulate sealing, and must not be installed as a
system-library replacement. The asset-free Vulkan layout probe passed with it.

With this shim and `LP_NUM_THREADS=1`, a bounded 60-second run produced inspected,
non-black 1280×720 startup captures. Subsequent no-SDL scripted-input runs showed
the Japanese brightness screen and then a screen-adjustment step with large
horizontal arrows and an A-to-continue prompt. Earlier brightness text remained
visible underneath the later UI: transition/clearing correctness is unresolved.
Main-menu arrival is not verified.

The slider test exited 0 at its deadline, recording 326 renderer frames, 19,468
draws, 16 texture uploads with no failures, and 329 decoded XMA frames on three
contexts with zero errors. No unimplemented imports were reached. These counts
are not playable-performance evidence; audible output remains unverified.

`MW2_INPUT_SCRIPT` works without SDL on this branch. For example,
`5:start:1,12:lx+:2,16:a:1` schedules Start, left-stick movement, and A. Script
times begin at the first input initialization/query, not necessarily process
start. Shader compilation and slow software rendering mean scheduled times must
not be assumed to match the displayed screen. Script-configuration log lines
alone do not prove that a press was consumed by the title.

A subsequent 90-second test with later A confirmations exited normally but all
six sampled captures showed brightness setup, not the screen-adjustment view.
Progression therefore remains inconsistent. It recorded 584 renderer frames,
34,174 draws and 309 decoded XMA frames, with no upload/decoding errors or reached
unimplemented imports.

For the no-SDL scripted controller, `MW2_INPUT_TRACE=1` logs changed states as
they are returned by `XamInputGetState`, including script-relative time, packet,
buttons, triggers, and axes. This distinguishes configured events from delivered
states (including releases); it does not prove that UI code accepted the input.
An event whose entire interval elapses between polls is not queued or replayed.
Keep these diagnostic logs private as well.

The traced 90-second run confirmed all nine scheduled input events and their
releases were returned to the guest (18 state changes). Left-stick X reached
32767 and -32768, D-pad Right reached button mask `0008`, and A reached `1000`.
Captures showed the brightness slider changing and then the screen-adjustment
view, still with overlapping brightness UI. Later A states were delivered, but
progression beyond that view was not demonstrated. This rules out missed event
intervals for this run, not incorrect UI/input semantics elsewhere.
The run exited 0 at its deadline with 458 renderer frames, 26,586 recorded draws,
16 texture uploads with no failures, 759 decoded XMA frames with zero errors,
and no reached unimplemented imports. Main menu and audible output remain
unverified.

### Profile ABI correction awaiting game-dependent validation

Source review found that the profile reader omitted the `XUSER_DATA` type byte
at entry offset +24 for stored settings. The public layout in Xenia's
`src/xenia/kernel/xam/user_profile.h` confirms that the type is a byte (binary
is 6); the value remains at +32. The reader now populates this field. An
asset-free test covers binary and integer round trips, payload pointers,
reserved bytes, missing settings, and buffer sizing through the real imports.
This is an ABI correction, not a demonstrated fix for setup confirmation.
The overlapping UI remains unexplained; blindly changing attachment load
operations to clear would discard legitimate contents between rendering passes.

`MW2_DUMP_FRAMES` selects a private capture directory and
`MW2_DUMP_FRAME_EVERY` sets the sampling interval in frames. Keep captures,
generated code, assets, and raw logs local. Public builds and synthetic tests
run in GitHub Actions; game-dependent builds and live runs stay local.

### Earlier incomplete-data run

See [MW3-BRINGUP.md](MW3-BRINGUP.md): with `ui.ff`, `common.ff`,
`imagefile1.pak` and `imagefile3.pak` supplied the title reaches
`imagefile4.pak`, submitting 17 GPU swaps before the dirty-disc error.
