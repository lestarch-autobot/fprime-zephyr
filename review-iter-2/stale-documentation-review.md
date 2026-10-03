<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# stale-documentation-review — iteration 2 (head 165ccf3)

Iteration-2 review of 165ccf3: all 5 iteration-1 findings are resolved and the fixes didn't break anything. There are 6 new findings, none must-fix: 2 suggestions and 4 could-fix. Nothing was posted or pushed.

I checked FatFs claims against R0.16 at the revision Zephyr `main` pins (`4f60a12`).

**Suggestions**
- **DOC-6:** The SDD's example guard for calling `getStats()`, `#if defined(CONFIG_FS_FATFS_LFN_MODE_HEAP)`, isn't enough. With `-DFPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR=OFF` the macro is still defined but the module isn't built, so a caller written that way fails at link time. The module exports no compile definition a caller could check instead.
- **DOC-7:** The SDD says the slot count needed is the smaller of mounted volumes and FatFs threads. The new build check instead requires at least `FF_VOLUMES`, which counts every devicetree disk (or the custom mount-point count), mounted or not. A deployment that follows the SDD's rule can fail the build.

**Could fix**
- **DOC-8:** The SDD, the cpp comments and the new `FF_DEFINED` `static_assert` message all say `INIT_NAMBUF`. In this FatFs revision the macro is `INIT_NAMEBUFF`, so anyone re-checking the slot size after a FatFs upgrade will grep for a name that doesn't exist.
- **DOC-9:** The SDD's Requirements table has no rows for the new build checks (slot count vs `FF_VOLUMES`, the FatFs revision pin). FZFA-007 still lists only inspection as verification, though it is now checked at build time.
- **DOC-10:** The comments in `FatFilenameAllocatorCfg.hpp` and the cpp say "at most one slot per volume" / "exhaustion impossible", without the `f_fdisk(..., NULL)` exception the SDD itself documents.
- **DOC-11:** The `dir_clear` warning's write counts (64 and 256) assume 512 B sectors without saying so, and the warning splits the bullet list it sits in.

The findings file has the suggested text for each item.
