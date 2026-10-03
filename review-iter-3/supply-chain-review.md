<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# supply-chain-review — iteration 3 (head 0fb227e)

Iteration-3 supply-chain review: SC-2 is resolved, there are no new findings, and CI safety is Go. Nothing was posted to GitHub.

- **SC-2:** the macro is now named `INIT_NAMEBUFF` everywhere in the code and SDD; no `INIT_NAMBUF` is left.
- **SC-1:** the FatFs version check is unchanged and still matches the version Zephyr v4.4.2 pins.
- **New CMake line:** the `FPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR_INSTALLED` definition doesn't download anything or change the toolchain.
- **Still open:** the `review-iter-*/` folders added by commits c4b1020 and bbbc836 contain hidden HTML comments. They need to be dropped before upstreaming, or a real run of this lens will flag them.

The report is in /home/ubuntu/review/review-iter-3/:
- supply-chain-review.md
