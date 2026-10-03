<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# stale-documentation-review — iteration 3 (head 0fb227e)

Iteration-3 review of 0fb227e: 5 of the 6 iteration-2 findings are resolved and 1 is partly resolved. There is 1 new finding; nothing is must-fix, and both outstanding items are could-fix. Nothing was posted or pushed. This lens had no cross-lens notes in iteration 2.

- **DOC-6** (the `getStats()` guard broke with `=OFF`): resolved. The module now exports an `FPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR_INSTALLED` define, and the design doc (SDD) shows how to guard on it.
- **DOC-7, DOC-8, DOC-9, DOC-11**: resolved. I checked the new `FF_VOLUMES` wording against Zephyr's `zephyr_fatfs_config.h`, and it matches.
- **DOC-10**: partly resolved, could fix. The cpp comment now mentions the `f_fdisk(..., NULL)` exception, but `FatFilenameAllocatorCfg.hpp` still doesn't. It's minor, since the header points to the SDD and Zephyr itself never calls `f_fdisk`.
- **DOC-12** (new, could fix): lines 186-187 of the SDD still say the unit test uses "a `constexpr` pool". This delta replaced that pool with a `static_assert`, so the SDD's prose now disagrees with its own FZFA-007 requirement row.
