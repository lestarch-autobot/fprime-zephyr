<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# correctness-review — iteration 2 (head 165ccf3)

Combined report for: security-review, correctness-review

All 5 iteration-1 findings are fixed in 165ccf3, including the must fix. Both lenses are now Go. Nothing was posted to GitHub.

**security-review:** no findings, and CI safety is clean.

**correctness-review:** all five iteration-1 findings are resolved:
- The design doc now says that on exFAT, `f_sync` and `f_close` of a modified file also take a slot. It also warns about the data-loss case.
- The build now fails if the slot count is below the number of FatFs volumes.
- A compile-time check now confirms the pool needs no startup constructor. I tested it with GCC (C++11, 14 and 17): it compiles, and it correctly breaks the build when I gave it a lock type that can't be built at compile time. Clang wasn't installed, so I couldn't check it there.
- The `--wrap` linker flags are now added unconditionally, so a linker that doesn't support them fails the build instead of silently falling back to `k_malloc`.
- The design doc now lists `f_fdisk`.

**One new could-fix (a doc inconsistency):** `fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:60-63` says the count needed is the smaller of the volume count and the thread count. The new build check rejects any count below `FF_VOLUMES`. So a deployment with 2 disks and 1 thread that follows the doc and sets 1 slot will fail to build. The code is correct; the doc needs rewording.

The fixes introduced no regressions.
