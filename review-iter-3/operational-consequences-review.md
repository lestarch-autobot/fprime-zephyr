<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# operational-consequences-review — iteration 3 (head 0fb227e)

Combined report for: design-review, operational-consequences-review

0fb227e fixes both iteration-2 findings (DR-5 and OC-4), and the re-review found nothing new. Every finding from iterations 1–3 is now closed and nothing in the delta regressed. Nothing was posted or pushed.

- **DR-5:** resolved. `FPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR_INSTALLED` is now set only when the pool is actually built, and the SDD shows how a caller should guard and link `getStats()`. That also closes the rest of DR-4.
- **OC-4:** resolved. The SDD now explains what `FF_VOLUMES` counts, mounted or not, and drops the thread-count wording for the REENTRANT case.

One note I did not file: callers should check `if (TARGET fprime-zephyr_Fs_FatFilenameAllocator)` before linking the allocator. That check only sees the target in code processed after `Fs`. Deployments are fine, but a future caller inside fprime-zephyr's own `Os`, `Drv` or `Svc` would not.

The iteration-2 note on garbage collection of Zephyr's own allocator is unchanged. It still rests on your PROVES link result.

Files are in `~/review-iter-3/`: `design-review.md`, `operational-consequences-review.md`, `structured.json`.
