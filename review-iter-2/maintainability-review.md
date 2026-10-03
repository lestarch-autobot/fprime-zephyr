<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# maintainability-review — iteration 2 (head 165ccf3)

Iteration-2 maintainability re-review at 165ccf3 is done: 5 of the 7 iteration-1 findings are fixed and nothing must be fixed. Nothing was posted to GitHub.

**Iteration-1 findings:**
- **Fixed (MAINT-1 to MAINT-5):** stats filled by name, single alignment constant, FatFs revision pinned (80386, which matches R0.16 upstream), constant-initialization checks in the `.cpp` and the unit test, and the CMake gating no longer returns early.
- **Not fixed (MAINT-6, MAINT-7, both could fix):**
  - The stats-only class `FprimeZephyrFatFilenameAllocator` still has a name that suggests it allocates.
  - The config constant is still called `FPRIME_ZEPHYR_FAT_FILENAME_SLOTS` rather than a short name like `SLOT_COUNT`.
  - The commit doesn't say why either was kept.

**New findings, both suggestions:**
- **MAINT-8** (`FprimeZephyrFatFilenameAllocator.cpp:16`): the new comment says `FF_VOLUMES` slots "make exhaustion impossible". The SDD only claims that for FatFs's own calls, because an application calling `f_fdisk(..., NULL)` can still take a slot. Reword the comment to match.
- **MAINT-9** (`FprimeZephyrFatFilenameAllocator.cpp:53-55`): the new `static_assert` now sits between the `s_pool` doc comment and `s_pool` itself, so the comment documents the assert instead of the object. Move the assert below `Pool s_pool;`.

The findings are in `review-iter-2/maintainability-review.md`.
