<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# fprime-code-review — iteration 2 (head 165ccf3)

The iteration-2 local re-review of 165ccf3 (fprime-code-review) is done. Both iteration-1 findings are resolved and there are no new findings. Nothing was posted to GitHub or pushed.

- **CPP-19 (was must fix): resolved.** Every field of `FatFilenameAllocatorStats` now starts at `0U`, and `getStats()` sets every field.
- **CPP-11 (was could fix): resolved.** `getStats()` is now `const` and the lock member is `mutable`. Both lock types (the Zephyr spinlock and the host test mutex) still work with this.

I also checked the new FatFs version check (`FF_DEFINED == 80386`) against the FatFs version Zephyr v4.4.2 actually uses. That version has `80386` and the same buffer-size formulas, so the PROVES build is not broken by it. I did not compile anything.

The attached report also has two notes for other review lenses. They are not findings of this lens:
- **Unused test variable:** the new `CONSTANT_INIT_PROBE` in the test is never referenced. clang's `-Wall` warns about that, so a clang host test build with `-Werror` would fail; GCC's `-Wall` does not warn.
- **Overstated comment:** the `.cpp:16` comment says `FF_VOLUMES` slots "make exhaustion impossible". An application `f_fdisk(..., NULL)` call runs outside the volume lock and can take one more slot. The design doc already describes this case.
