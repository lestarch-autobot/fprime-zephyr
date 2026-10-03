<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# operational-consequences-review — iteration 2 (head 165ccf3)

Combined report for: design-review, operational-consequences-review

The fixes in 165ccf3 resolve all iteration-1 findings except DR-4, which is only partly resolved. The re-review found no regressions and two new minor findings (one could fix, one suggestion). Nothing was posted or pushed.

**Iteration-1 status**
- **OC-1 (the must fix):** resolved. The SDD is corrected and has the close-time warning. The new build check that the slot count is at least `FF_VOLUMES` removes the data-loss risk for FatFs's own calls when `CONFIG_FS_FATFS_REENTRANT=y`.
- **OC-2, OC-3, DR-1, DR-2, DR-3:** resolved.
- **DR-4:** partly resolved. The SDD now says when `getStats()` exists, but the `#if` guard it recommends is wrong (DR-5).

**New**
- **DR-5 (could fix):** the SDD tells callers of `getStats()` to guard with `#if defined(CONFIG_FS_FATFS_LFN_MODE_HEAP)`.
  - With `-DFPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR=OFF`, that guard is still true but the module isn't built, so the caller compiles and then fails to link.
  - The CMake option isn't passed to C++, so no correct guard exists.
  - Fix: export a compile definition from the module, or always build a `getStats()` that returns `slotCount = 0` when the pool isn't installed.
- **OC-4 (suggestion):** the new slot-count text doesn't match the new build check.
  - The SDD says the count needed is the smaller of mounted volumes and threads, and that one SD card needs 1 slot. The build requires slots ≥ `FF_VOLUMES`.
  - In Zephyr, `FF_VOLUMES` counts every enabled disk node in the devicetree (flash, RAM, SDMMC, …), or `CONFIG_FS_FATFS_CUSTOM_MOUNT_POINT_COUNT` (up to 10). It is not the number of mounted volumes.
  - So a board that enables more disk nodes than it mounts can't go down to 1 slot. A board with more than 4 disk nodes fails to build with the default count.
  - The check is safe and conservative; only the SDD and config comment wording need fixing.

One claim rests on indirect evidence: the SDD says Zephyr's own allocator is dropped by section garbage collection. I confirmed Zephyr links with `--... [truncated]

_The message transcript was truncated on export; the full report is in reviewer session https://nasa-jpl-demo.devinenterprise.com/sessions/b79d2041a32c48f185c942d8f1ca8e6c._
