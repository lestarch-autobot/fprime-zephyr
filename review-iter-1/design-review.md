<!-- LOCAL REVIEW ONLY — iteration 1, head cb58109, not posted to GitHub -->

# design-review

Lens: F Prime Design Reviewer (`[Design]`), review_group `system`, iteration 1 (local diff only, nothing posted).
Change: lestarch-autobot/fprime-zephyr `devin/1791051832-fat-filename-allocator`, base `b76db2b5` .. head `cb58109b`.
Evidence sources read: full content of all 10 changed files; Zephyr FatFs `ff.c` (zephyrproject-rtos/fatfs master), Zephyr `modules/fatfs/{zfs_ffsystem.c,zephyr_fatfs_config.h,CMakeLists.txt}`, `subsys/fs/{Kconfig.fatfs,fat_fs.c,fs.c}` (zephyr main), fprime-zephyr `Os/File.cpp`, `cmake/toolchain/zephyr.cmake`.

<!-- ledger_rows: 16 --> <!-- unexplored_below_must_fix: 0 -->

## Findings

### DR-1 — design-margin-erosion — **suggestion**
- **Location:** `default/zephyr-config/FatFilenameAllocatorCfg.hpp:9-11` (also `fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:42-43`)
- **Confidence:** high
- **Description:** The default of 4 slots and the comment "one per thread that may be inside a FatFs path call at the same time" over-size the pool for the configuration the SDD targets (`CONFIG_FS_FATFS_REENTRANT=y`). In `ff.c`, every `INIT_NAMEBUFF` runs after `mount_volume()`/`validate()` has taken the per-volume mutex (`lock_volume`, ff.c:899-921, e.g. f_open 3820→3825) and every `FREE_NAMEBUFF` runs before `LEAVE_FF` releases it (e.g. f_open 3986→3991). Zephyr sets `FF_FS_TIMEOUT` to `K_FOREVER`. So at most one slot per FAT volume can be in use: concurrent demand is `min(threads, FF_VOLUMES)`, not the thread count. For a single-volume deployment (one SD card, e.g. PROVES) 3 of the 4 default slots (3 × 1120 B = 3360 B of .bss at exFAT/MAX_LFN=255) can never be used.
- **Suggested fix:** When `FF_FS_REENTRANT` is set, derive the default from `FF_VOLUMES` (for example `constexpr FwSizeType SLOTS = FF_FS_REENTRANT ? FF_VOLUMES : FatFilenameAllocatorConfig::FPRIME_ZEPHYR_FAT_FILENAME_SLOTS;` in the .cpp), or keep the config constant and change its comment and the SDD to "number of mounted FAT volumes (FF_VOLUMES) when CONFIG_FS_FATFS_REENTRANT=y; otherwise the number of threads that may call FatFs at the same time".

### DR-2 — design-missed-reuse — **suggestion** (low confidence)
- **Location:** `fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:31-121`
- **Confidence:** low
- **Description:** The new template is a fixed-block, statically allocated, lock-protected pool. Zephyr already provides one in the kernel this module depends on: `k_mem_slab` (`K_MEM_SLAB_DEFINE_STATIC`, `k_mem_slab_alloc(..., K_NO_WAIT)`, `k_mem_slab_free`, `k_mem_slab_num_used_get`, and `k_mem_slab_max_used_get` with `CONFIG_MEM_SLAB_TRACE_MAX_UTILIZATION`). Zephyr's own FatFs glue uses it for `FIL` objects (`subsys/fs/fat_fs.c`, `fatfs_filep_pool`). The overlap is partial: a slab has no size-rejection counter, and it detects foreign or double frees only through `__ASSERT` under `CONFIG_ASSERT`. A thin shim (size check plus counters plus `k_mem_slab`) would keep the approved behaviour without a separately verified allocator. The SDD "Design" section does not say why the kernel facility was not used.
- **Suggested fix:** Either build the shim on a `K_MEM_SLAB_DEFINE_STATIC` slab, keeping the size check, the rejected/exhausted counters and an explicit ownership check before `k_mem_slab_free`, or add one sentence to `docs/sdd.md` §Design explaining why `k_mem_slab` was rejected (e.g. no FW_ASSERT on foreign or double free without `CONFIG_ASSERT`, and the template can be unit tested on the host).
- cc @LeStarch @thomas-bc — low-confidence finding, please confirm.

### DR-3 — design-behavioral-regression — **could fix** (low confidence)
- **Location:** `fprime-zephyr/Fs/FatFilenameAllocator/CMakeLists.txt:7-12`; `fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:91-93`
- **Confidence:** low
- **Description:** The automatic hook and the exact-size-only policy are approved design decisions (1) and (2). This is not a defect claim, only a migration risk. A downstream deployment that already sets `CONFIG_FS_FATFS_LFN_MODE_HEAP=y` and updates the fprime-zephyr submodule changes behaviour with no configuration change: (a) LFN buffers move from the kernel heap to +4480 B of .bss, capped at 4 concurrent buffers; (b) `dir_clear()` loses its multi-sector heap buffer, which it got before whenever `k_malloc` had room, and now always clears clusters one sector at a time; (c) heap bytes that were reserved for FatFs are no longer used. Category 6 asks for (a) acknowledgement, (b) justification and (c) migration guidance. The SDD gives the opt-out (`-DFPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR=OFF`) but has no "existing HEAP-mode deployments" note. No in-repo sample config (`sample-config/`, `ci/sample-configs/`) selects HEAP mode, so in-repo impact is nil.
- **Suggested fix:** Add a short "Upgrading existing HEAP-mode deployments" paragraph to `docs/sdd.md` (and the PR description) listing (a)–(c) and saying that `CONFIG_HEAP_MEM_POOL_SIZE` can be reduced by the amount that was reserved for FatFs.
- cc @LeStarch @thomas-bc — low-confidence finding, please confirm.

### DR-4 — design-code-mismatch — **could fix**
- **Location:** `fprime-zephyr/Fs/FatFilenameAllocator/CMakeLists.txt:10-12` vs `fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:96-114` and `FprimeZephyrFatFilenameAllocator.hpp:18`
- **Confidence:** medium
- **Description:** The SDD documents `FprimeZephyrFatFilenameAllocator::getStats()` as the way to inspect the pool. Under approved decision (3) it is the only observability interface, and it is what a later telemetry component would call. The module that defines it is registered only when `CONFIG_FS_FATFS_LFN_MODE_HEAP=y` and the CMake option is ON. In every other configuration, a consumer that lists the module in `DEPENDS` fails at configure time, or fails at link time with an undefined `getStats` if it only includes the header. So any consumer has to copy the Kconfig/option condition. That condition is not documented.
- **Suggested fix:** Either always register a module that provides `getStats()` (returning `slotCount = 0` when the pool is not installed), or state in the SDD "Inspecting the pool" section that the accessor and module exist only when the pool is installed, and name the condition a consumer must test.

## Considered, not filed
- **design-needs-human-adjudication** (trigger: a new build-system mechanism, i.e. global GNU ld `--wrap` symbol interposition added through `zephyr_ld_options` from a library module, the first use of `--wrap` in fprime-zephyr or fprime). Not filed: the design owner approved the automatic `--wrap` hook with a CMake opt-out (approved decision 1), which is the adjudication this class asks for. The implementation does what was approved: Kconfig gate, opt-out option, LTO guard, explicit `target_link_libraries(app ...)`.

## Ledger
| # | Unit | Disposition |
|---|---|---|
| 1 | sdd.md: intent ("REENTRANT without dynamic memory, without stack growth") vs design | clean: the pool removes k_malloc from FatFs; the remaining `HEAP_MEM_POOL_SIZE>0` is a Kconfig requirement and is documented |
| 2 | sdd.md: Slot count guidance | finding DR-1 |
| 3 | sdd.md: Behavior table vs `allocate`/`release` | clean: matches the code |
| 4 | sdd.md: Other ff_memalloc callers (dir_clear, f_mkfs) | clean as design: verified against ff.c:1694, 6179, 6642 and fat_fs.c:472/532 (work buffers passed); f_fdisk not called by Zephyr; no slot-size collision with dir_clear sizes for MAX_LFN 12..255 (dir_clear never requests ≤ SS) |
| 5 | sdd.md: Inspecting the pool / accessor availability | finding DR-4 |
| 6 | Cfg.hpp: default 4 / comment | finding DR-1 |
| 7 | FatFilenameAllocator.hpp: fixed pool template vs existing facilities | finding DR-2 |
| 8 | FatFilenameAllocator.hpp: fault handling (exhaustion → nullptr, wrong size → nullptr, foreign/double free → FW_ASSERT outside lock) | clean: approved decision 3; FatFs maps nullptr to FR_NOT_ENOUGH_CORE → -ENOMEM (fat_fs.c:65-66) |
| 9 | FprimeZephyrFatFilenameAllocator.cpp: static_assert(FF_USE_LFN==3), size derivation from ff.h | clean: matches ff.c:548/552 and MAXDIRB at ff.c:521 |
| 10 | .cpp: k_spinlock policy vs Os::Mutex | clean: constant-init requirement and non-F Prime caller context justify a kernel primitive |
| 11 | .cpp: `__wrap_*` C shims | clean |
| 12 | Static accessor instead of telemetry | clean: approved decision 3 |
| 13 | CMakeLists: Kconfig gating, opt-out, LTO guard, `target_link_libraries(app)` + `zephyr_ld_options` | clean / considered for adjudication (see above) |
| 14 | CMakeLists: behaviour change for existing HEAP deployments | finding DR-3 |
| 15 | default/zephyr-config/CMakeLists.txt, fprime-zephyr.cmake, Fs/CMakeLists.txt registration | clean |
| 16 | test/ut: host UT with std::mutex policy | out of scope (test-quality-review) |
