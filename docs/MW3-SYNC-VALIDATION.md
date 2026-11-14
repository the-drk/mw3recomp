# Synchronization validation follow-up

Branch: `bringup/sync-validation`, based on `73ab97a`.

The previous CI run failed before compilation because `build-tests/` contained
tracked CMake outputs from `/home/claude/mw3recomp`. Untrack those generated
files and ignore test/MW3 build directories; local copies are left intact.

The new asset-free `kernel_sync_test` compiles the real synchronization imports
and dispatcher implementation. It checks critical-section initialization,
recursive ownership, failed try-entry by another guest thread, final release,
spinlock ownership and exclusion across two host threads.

The initialization boundary test exposes unsigned overflow when rounding
`UINT32_MAX` spin counts: the old expression wraps to zero instead of saturating
to 255 units. Widen the addition before rounding. This is a runtime correctness
fix, not evidence that MW3 passes this value or that a live boot advances.

No game assets or generated PPC code are used. Live startup still requires the
user's supported executable and complete game root on a Linux/WSL host; menu,
renderer output and PS4 readiness remain unverified.
