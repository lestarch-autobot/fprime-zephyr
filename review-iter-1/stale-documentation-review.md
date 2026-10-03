<!-- LOCAL REVIEW ONLY — iteration 1, head cb58109, not posted to GitHub -->

# stale-documentation-review

<!-- fprime-agent: stale-documentation-review v1 -->
<!-- reviewed_head: cb58109bfa914f11c6338a291048c1e3e4d5aae6 -->
<!-- counts: {"must_fix": 0, "suggestion": 2, "could_fix": 3, "future_work": 0, "outstanding": 5} -->
<!-- verdict: Go -->
<!-- run: 1 -->
<!-- ledger_rows: 24 -->
<!-- unexplored_below_must_fix: 1 -->

Lens: Documentation Currency (`stale-documentation-review`). Local review only, nothing posted.
Diff: `b76db2b5...cb58109b` in lestarch-autobot/fprime-zephyr. Reading order (contract §13f): docs/sdd.md, headers
(FatFilenameAllocatorCfg.hpp, FatFilenameAllocator.hpp, FprimeZephyrFatFilenameAllocator.hpp), source
(FprimeZephyrFatFilenameAllocator.cpp), test (FatFilenameAllocatorTestMain.cpp), build files (CMakeLists.txt x3,
fprime-zephyr.cmake). I checked the SDD's claims against Zephyr `main` sources (`subsys/fs/Kconfig.fatfs`,
`subsys/fs/fat_fs.c`, `modules/fatfs/zfs_ffsystem.c`) and `zephyrproject-rtos/fatfs` `ff.c`, not against a v4.4.2 checkout.

No must-fix findings. Every SDD claim I checked matches the code and Zephyr/FatFs, except the items below.

---

### DOC-1: SDD's list of "other `ff_memalloc` callers" leaves out `f_fdisk()`

- **Tag:** `**suggestion**`
- **Class:** `stale-sdd`
- **Location:** `fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:86-94` ("Other `ff_memalloc` callers")
- **Confidence:** medium
- **Description:** The section says the other FatFs callers of `ff_memalloc` "are" `dir_clear()` and `f_mkfs()`. `ff.c` has a third one: `f_fdisk()` (compiled with `FF_MULTI_PARTITION` and mkfs support, i.e. `CONFIG_FS_FATFS_MULTI_PARTITION=y`) calls `if (!buf) buf = ff_memalloc(FF_MAX_SS);`. With exFAT off, `MAX_LFN=255` and `FF_MAX_SS=512`, that request is exactly the 512 B slot size, so it would be **served** from the pool rather than rejected. That is the opposite of what the section implies for non-LFN callers.
- **Why below must-fix:** Zephyr's `fat_fs.c` never calls `f_fdisk`, so only an application that calls it directly with `buf == NULL` can reach it. It also holds no LFN buffer at the same time, so the "at most one slot per thread" sentence still holds.
- **Suggested fix:**
```suggestion
- `f_mkfs()` and `f_fdisk()` (`CONFIG_FS_FATFS_MULTI_PARTITION`) use `ff_memalloc` only when given no work buffer.
  Zephyr's `fs_mkfs` and mount-time format always pass one, and Zephyr never calls `f_fdisk`. An application that calls
  `f_fdisk(..., NULL)` directly asks for `FF_MAX_SS` bytes; when that equals the slot size (exFAT off, `MAX_LFN=255`,
  512 B sectors), the request takes a pool slot.
```

### DOC-2: Top-level README doesn't mention the automatic FatFs allocator hook

- **Tag:** `**suggestion**`
- **Class:** `stale-top-level-doc` / `missing-doc-for-new-surface`
- **Location:** `README.md` ("Adding Zephyr Deployments" / "Build"); triggered by `fprime-zephyr.cmake:5` and `fprime-zephyr/Fs/FatFilenameAllocator/CMakeLists.txt:59-88`
- **Confidence:** low (I inferred the doc surface; no current README text covers FatFs). Per the low-confidence rubric, this needs a maintainer ping.
- **Description:** The README is the deployment-integration guide for this package. With this change, any deployment that already sets `CONFIG_FS_FATFS_LFN_MODE_HEAP=y` gets these automatically, with no deployment-side change:
  - a new `-Wl,--wrap` link option;
  - a static `.bss` pool;
  - new failure behavior: exhaustion returns `-ENOMEM`, and `dir_clear` falls back to sector-at-a-time clearing.

  Two new configure-time errors can also stop the build: `CONFIG_LTO`, and the `app` target missing. The README says none of this and does not point to the new SDD or the `FPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR=OFF` opt-out. Users of downstream `fprime-zephyr-reference` deployments would only find out from the SDD. This is a possible external-doc follow-up (`stale-external-doc`); I did not check its current `prj.conf`.
- **Suggested fix:** Add a short section to `README.md`, e.g. after "Adding Zephyr Deployments":
```suggestion
## FatFs Long File Names

When a deployment sets `CONFIG_FS_FATFS_LFN_MODE_HEAP=y` (required for `CONFIG_FS_FATFS_REENTRANT=y` without
`LFN_MODE_STACK`), fprime-zephyr serves FatFs long-filename buffers from a static pool instead of `k_malloc`.
See [FatFilenameAllocator SDD](./fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md) for sizing, `CONFIG_LTO`
incompatibility, and the `-DFPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR=OFF` opt-out.
```

### DOC-3: SDD says Zephyr's `ff_memalloc` "stays in the image"

- **Tag:** `**could fix**`
- **Class:** `stale-sdd`
- **Location:** `fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:124-127`
- **Confidence:** low
- **Description:** The SDD says Zephyr's `ff_memalloc`/`ff_memfree` in `zfs_ffsystem.c` "stays in the image, unreferenced". Zephyr links with `--gc-sections` and `-ffunction-sections`, so an unreferenced `ff_memalloc` (and its `k_malloc` call) is normally discarded. The author's own link check ("no k_malloc reference") is more consistent with it being discarded than kept. The sentence may mislead someone who checks the map file.
- **Suggested fix:**
```suggestion
`__wrap_ff_memalloc`. Zephyr's own definition in `zfs_ffsystem.c` is still compiled (it shares the file with the
`ff_mutex_*` functions that `FS_FATFS_REENTRANT` needs) but is no longer referenced, so section garbage collection
normally drops it from the image. Zephyr has no Kconfig or CMake option to leave out only its allocator.
```

### DOC-4: SDD's `dir_clear()` fallback description is slightly wrong

- **Tag:** `**could fix**`
- **Class:** `stale-sdd`
- **Location:** `fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:89-91`
- **Confidence:** high
- **Description:** "halves the size down to one sector until a request succeeds" doesn't match the code. `ff.c` `dir_clear()` loops `szb > SS(fs) && (ibuf = ff_memalloc(szb)) == 0; szb /= 2`, so it never requests a single-sector buffer. It stops at two sectors and then uses `fs->win`. The conclusion (rejected, falls back to the window) is right.
- **Suggested fix:**
```suggestion
- `dir_clear()` (from `f_mkdir` and directory growth) asks for a cluster-sized buffer (capped at 32 KiB) and halves the
  size while it is larger than one sector. These requests are rejected (counted in `rejectedCount`) and FatFs falls
  back to clearing the cluster one sector at a time from the volume window. This is slower but correct.
```

### DOC-5: Badly wrapped Doxygen `\tparam LockPolicy` comment

- **Tag:** `**could fix**`
- **Class:** `stale-public-api-comment` (cosmetic)
- **Location:** `fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:28-30`
- **Confidence:** high
- **Description:** The formatter split the `LockPolicy` doc so the word "from" sits alone on line 29 and the continuation is indented. The rendered Doxygen description reads oddly.
- **Suggested fix:**
```suggestion
//! \tparam LockPolicy type providing `Lock` (default-constructible lock object) and `Guard` (RAII guard
//!         constructed from `Lock&`) used to serialize access to the pool
```

---

## Ledger dispositions (session working state)

| # | Unit | Disposition |
|---|---|---|
| 1 | sdd.md "Why" mode table: BSS forbidden with REENTRANT | clean: `Kconfig.fatfs` `FS_FATFS_REENTRANT depends on !FS_FATFS_LFN_MODE_BSS` |
| 2 | sdd.md STACK 512-1120 B | clean: matches Kconfig help text ((N+1)*2, +608 with exFAT) |
| 3 | sdd.md HEAP requires `HEAP_MEM_POOL_SIZE > 0` | clean: `depends on HEAP_MEM_POOL_SIZE > 0` |
| 4 | sdd.md LTO error | clean: CMakeLists.txt:65-68 |
| 5 | sdd.md auto-hook steps 1-2 and opt-out | clean: CMakeLists.txt:60-88 |
| 6 | sdd.md slot count / config path / default 4 | clean: FatFilenameAllocatorCfg.hpp:28 |
| 7 | sdd.md exhaustion yields `FR_NOT_ENOUGH_CORE` / `-ENOMEM` | clean: ff.c `INIT_NAMBUF`, fat_fs.c `translate_error` |
| 8 | sdd.md slot size table 512/1120 and `MAXDIRB` formula | clean: ff.c:521,548,552; cpp:252-258 |
| 9 | sdd.md alignment / `.bss` footprint | clean: hpp `SLOT_STRIDE`; 1120 is a multiple of 16 |
| 10 | sdd.md behavior table (7 rows) | clean: `allocate()`/`release()` match |
| 11 | sdd.md spinlock / critical section | clean: cpp `ZephyrSpinLockPolicy` |
| 12 | sdd.md `dir_clear()` description | finding DOC-4 |
| 13 | sdd.md `f_mkfs` and the completeness of the caller list | finding DOC-1 (f_mkfs part clean: fat_fs.c:472-481, 532-539 pass `work`) |
| 14 | sdd.md stats accessor snippet and field table | clean: matches `FatFilenameAllocatorStats` and `getStats()` |
| 15 | sdd.md design file table | clean |
| 16 | sdd.md "stays in the image" | finding DOC-3 |
| 17 | sdd.md "no heap, no new, no ctor" | clean: constexpr ctor, default member init |
| 18 | sdd.md requirements FZFA-001..008 | clean: consistent with code; test-side verification is out of scope (test-quality-review) |
| 19 | sdd.md UT run instructions / ctest `-R` | clean: ut.cmake `add_test(NAME ${UT_EXECUTABLE_TARGET})` contains the module name |
| 20 | FatFilenameAllocatorCfg.hpp doc comment | clean |
| 21 | FatFilenameAllocator.hpp Doxygen (`allocate`/`release`/`getStats`/struct) | clean except finding DOC-5 |
| 22 | FprimeZephyrFatFilenameAllocator.hpp/.cpp comments | clean |
| 23 | fprime-zephyr.cmake / Fs/CMakeLists.txt / config CMakeLists: top-level doc surfaces | finding DOC-2 |
| 24 | fprime docs `supported-platforms.md` (Zephyr row) | clean: no change to the platform list |

Below-must-fix item not explored: whether `fprime-zephyr-reference` documentation or its `prj.conf` already enables
`LFN_MODE_HEAP` (external-doc follow-up for DOC-2).

    findings: {'key': 'DOC-1', 'lens': 'stale-documentation-review', 'class': 'stale-sdd', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:86-94', 'severity': 'suggestion', 'confidence': 'medium', 'description': "The SDD's list of other ff_memalloc callers names only dir_clear() and f_mkfs(). It leaves out f_fdisk() (CONFIG_FS_FATFS_MULTI_PARTITION), which calls ff_memalloc(FF_MAX_SS) when buf is NULL. With exFAT off, MAX_LFN=255 and 512 B sectors, that request equals the 512 B slot size and would be served from the pool, not rejected. This is below must-fix because Zephyr never calls f_fdisk, so only a direct application call can reach it.", 'suggested_fix': 'Extend the f_mkfs bullet to cover f_fdisk: Zephyr never calls it, and a direct f_fdisk(...,NULL) call can take a pool slot when FF_MAX_SS equals the slot size.'}, {'key': 'DOC-2', 'lens': 'stale-documentation-review', 'class': 'stale-top-level-doc', 'location': 'README.md (Adding Zephyr Deployments / Build); triggered by fprime-zephyr.cmake:5 and Fs/FatFilenameAllocator/CMakeLists.txt:59-88', 'severity': 'suggestion', 'confidence': 'low', 'description': "The README is this package's deployment-integration guide, but it doesn't mention the new hook. Any deployment with LFN_MODE_HEAP=y now silently gets an ld --wrap, a .bss pool, new -ENOMEM and dir_clear fallback behavior, and two new configure-time fatal errors (CONFIG_LTO, missing app target). The README doesn't point to the SDD or the FPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR=OFF opt-out. fprime-zephyr-reference may also need an external-doc follow-up.", 'suggested_fix': "Add a short 'FatFs Long File Names' section to README.md that links the SDD and names the LTO limitation and the opt-out option."}, {'key': 'DOC-3', 'lens': 'stale-documentation-review', 'class': 'stale-sdd', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:124-127', 'severity': 'could fix', 'confidence': 'low', 'description': "The SDD says Zephyr's ff_memalloc in zfs_ffsystem.c 'stays in the image, unreferenced'. With Zephyr's --gc-sections, an unreferenced ff_memalloc (and its k_malloc call) is normally discarded, which matches the author's 'no k_malloc reference' link check.", 'suggested_fix': 'Reword: the definition is still compiled (it shares the file with ff_mutex_*), but it is unreferenced, so section garbage collection normally drops it.'}, {'key': 'DOC-4', 'lens': 'stale-documentation-review', 'class': 'stale-sdd', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:89-91', 'severity': 'could fix', 'confidence': 'high', 'description': "The SDD says dir_clear() 'halves the size down to one sector until a request succeeds'. ff.c only loops while szb > SS(fs), so it never requests a one-sector buffer. The request is capped at MAX_MALLOC (32 KiB) and then FatFs falls back to fs->win.", 'suggested_fix': 'Reword to: asks for a cluster-sized buffer (capped at 32 KiB) and halves the size while it is larger than one sector, then falls back to the volume window.'}, {'key': 'DOC-5', 'lens': 'stale-documentation-review', 'class': 'stale-public-api-comment', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:28-30', 'severity': 'could fix', 'confidence': 'high', 'description': "The \\tparam LockPolicy Doxygen comment is badly wrapped: 'from' sits alone on its own line and the continuation is indented, so the rendered docs read oddly.", 'suggested_fix': "Re-wrap to two lines: '... and `Guard` (RAII guard' / 'constructed from `Lock&`) used to serialize access to the pool'."}
