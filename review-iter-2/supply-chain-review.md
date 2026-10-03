<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# supply-chain-review — iteration 2 (head 165ccf3)

Iteration-2 supply-chain review is done: SC-1 is resolved, with one new could-fix and no must-fix items, so CI safety is still Go. Nothing was posted to GitHub.

**SC-1 (resolved):** the new `static_assert(FF_DEFINED == 80386)` pins the FatFs version. I checked it upstream: Zephyr v4.4.2 pins FatFs `f4ead3bf`, which defines 80386. The size formula in that version's `ff.c` matches the pool's slot size.

**SC-2 (new, could fix):** the comments and assert message at `FprimeZephyrFatFilenameAllocator.cpp:13,14,24,27` and `docs/sdd.md:167` name `INIT_NAMBUF`. That macro doesn't exist in the pinned `ff.c`; the real name is `INIT_NAMEBUFF`. Someone who hits the version assert on a Zephyr upgrade and searches for the named macro will find nothing. Fix: rename it to `INIT_NAMEBUFF` in those places.

Nothing else in the fix commit concerns this lens:
- **Link flags:** switching the `--wrap` flags from `zephyr_ld_options` to `zephyr_link_libraries` adds no fetch or toolchain change.
- **No sensitive files touched:** no dependency manifests, workflows, actions, Dockerfiles or review-system files.
- **Prompt injection:** the README, SDD and commit-message changes contain none.

Commit c4b1020 holds the iteration-1 review files, which contain hidden HTML comments. If that commit isn't dropped before upstreaming, a real run of this lens will flag those comments.

Files are in /home/ubuntu/review/review-iter-2/:
- supply-chain-review.md
