<!-- LOCAL REVIEW ONLY — iteration 1, head cb58109, not posted to GitHub -->

# correctness-review

Review of lestarch-autobot/fprime-zephyr `devin/1791051832-fat-filename-allocator`, base b76db2b58025ea80efbe9999a4f1a15e95201775, head cb58109bfa914f11c6338a291048c1e3e4d5aae6 (run 1, local only).

Counts: must_fix=1, suggestion=3, could_fix=1, future_work=0. Verdict: No-Go.

Ledger (13e), rows dispositioned:
- FatFilenameAllocator.hpp allocate(): size predicate, first-free scan, counters, lock scope: clean.
- release(): nullptr no-op; asserts on a foreign or double-freed pointer are fired outside the lock; m_inUseCount only decremented when the slot was in use: clean.
- getStats(): snapshot taken under the lock: clean.
- SLOT_STRIDE rounding and alignment; the exFAT dirbuf (lfn + FF_MAX_LFN+1 WCHAR) stays inside SLOT_BYTES: clean.
- LFN_BUFFER_BYTES / EXFAT_DIR_BLOCK_BYTES compared with ff.c:521/548/552 (WCHAR is WORD, MAXDIRB evaluated left to right): clean.
- dir_clear request sizes (powers of two > SS >= 512) against the slot size for every FF_MAX_LFN in 12..255, exFAT on and off: never equal (max slot 1120): clean.
- f_mkfs: Zephyr always passes a work buffer (fat_fs.c:470, 528): clean.
- Every INIT_NAMEBUFF..FREE_NAMEBUFF region in ff.c (f_open, f_sync, f_chdir, f_getcwd, f_opendir, f_readdir, f_stat, f_unlink, f_mkdir, f_rename, f_chmod, f_utime) has no early exit, so no slot leaks: clean.
- SDD claims on handle calls and slot lifetime: finding 1.
- Slot-count sizing against the FatFs volume lock: finding 2.
- s_pool constant initialization: finding 3.
- zephyr_ld_options drop behavior: finding 4.
- SDD list of other ff_memalloc callers: finding 5.
- CMake gating (CONFIG_LTO fatal, app target check, opt-out option): clean apart from finding 4.
- UT death-test and recording-hook logic: clean (test-substance questions are test-quality-review's scope).
ledger_rows: 15; unexplored_below_must_fix: 1 (FW_ASSERT index argument in release() always equals SLOT_COUNT on failure; diagnostic value only, not filed).

## 1. [Correctness] **must fix** SDD says handle calls never take a slot and slots are held only by path-taking calls; on exFAT, f_sync and f_close do take one

- Finding key: `4e7b8021201d6286` (site-key `32665c4a4e4c3c73`)
- Class: `correctness-other`
- Severity: must fix
- Location: `fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:9-11, 54-56`
- Confidence: high

**Description.** sdd.md:10-11 says calls on open handles do not use the LFN buffer, and :54 says a slot is held only during a path-taking call. In the pinned FatFs (zephyrproject-rtos/fatfs@4f60a12, ff.c:4249-4270), f_sync runs DEF_NAMEBUFF/INIT_NAMEBUFF for any modified file when fs_type == FS_EXFAT, and f_close calls f_sync first (ff.c:4313). This PR is what makes that buffer a fixed-pool slot. When the pool is empty, closing a modified exFAT file fails with FR_NOT_ENOUGH_CORE before its directory entry (size, start cluster) is written. Zephyr's fatfs_close (subsys/fs/fat_fs.c:147-151) frees the FIL anyway, so the data written to that file is lost on the volume. The PR adds a false claim to its own SDD (§1a must-fix). It also understates which operations are affected by exhaustion.

**Suggested fix.** Correct the SDD (best-effort fix; verify before applying):
```suggestion
FatFs needs a temporary LFN buffer inside every path-taking call (`f_open`, `f_stat`, `f_unlink`, `f_rename`,
`f_mkdir`, `f_opendir`, `f_readdir`, ...). On exFAT volumes `f_sync` and `f_close` of a modified file also take one to
rewrite the directory entry. The buffer is released before the call returns. `f_read`, `f_write` and `f_lseek` do not use it.
```
Also update :54 to say "one FatFs call", not "one path-taking FatFs call". State that on exFAT, exhaustion can make `fs_close`/`fs_sync` return `-ENOMEM` after Zephyr has already released the file object.

## 2. [Correctness] **suggestion** Slot demand is bounded by FF_VOLUMES under FF_FS_REENTRANT, not by thread count; the bound is neither stated nor enforced

- Finding key: `70b51bfa2f7d6d07` (site-key `cde968d48c3f2658`)
- Class: `correctness-other`
- Severity: suggestion
- Location: `default/zephyr-config/FatFilenameAllocatorCfg.hpp:9-11 (and fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:42-44)`
- Confidence: medium

**Description.** With FF_FS_REENTRANT, every INIT_NAMEBUFF call runs after mount_volume()/validate() has taken that volume's mutex (ff.c lock_volume:899). The slot is released before LEAVE_FF unlocks it. So at most one slot is in use per mounted volume: concurrent holders <= FF_VOLUMES (Zephyr derives this from the devicetree disk list, zephyr_fatfs_config.h:112-121). The SDD sizing rule, "number of threads that may be inside such a call", over-provisions. More importantly, nothing ties the slot count to the actual bound. If a deployment sets SLOTS < FF_VOLUMES, exhaustion becomes possible, including the exFAT f_close data-loss path above. If SLOTS >= FF_VOLUMES, exhaustion is provably impossible.

**Suggested fix.** Add a compile-time guard in FprimeZephyrFatFilenameAllocator.cpp next to the FF_USE_LFN static_assert (best-effort; verify before applying):
```suggestion
#if FF_FS_REENTRANT
static_assert(FatFilenameAllocatorConfig::FPRIME_ZEPHYR_FAT_FILENAME_SLOTS >= FF_VOLUMES,
              "FatFs serializes each volume, so FF_VOLUMES slots make exhaustion impossible");
#endif
```
Alternatively, document SLOTS = number of mounted FatFs volumes as the sufficient value. cc @LeStarch @thomas-bc: confirm the preferred default.

## 3. [Correctness] **suggestion** Constant initialization of s_pool is relied on (FZFA-007, SDD:129) but nothing enforces it

- Finding key: `f901c60961fe8866` (site-key `970d4518c1f8a4c3`)
- Class: `correctness-initialization`
- Severity: suggestion
- Location: `fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:46-47`
- Confidence: medium

**Description.** `Pool s_pool;` is constant-initialized only while every member of FatFilenameAllocator, including Zephyr's `struct k_spinlock`, stays a literal type and the defaulted constexpr constructor remains valid. For a class template, an invalid constexpr is silently dropped (no diagnostic required). The compiler would then emit a dynamic initializer that runs from Zephyr's z_init_static(). That happens after POST_KERNEL, where FatFs automount can already be running, so the constructor would overwrite live pool state, breaking FZFA-007. The author checked a single configuration (rp2350, v4.4.2). Other configurations (CONFIG_SPIN_VALIDATE, SMP, ticket spinlocks) change the layout of k_spinlock and have not been checked.

**Suggested fix.** Make the build fail if the pool stops being constant-initializable (best-effort; verify before applying):
```suggestion
//! Constant-initialized: lives in .bss and needs no startup constructor
static_assert((Pool(), true), "Pool must be constant-initializable (no startup constructor)");
Pool s_pool;
```

## 4. [Correctness] **suggestion** zephyr_ld_options silently drops a flag that fails its check, so the pool can be skipped without any build error

- Finding key: `d7e18e92615ca362` (site-key `c7a42414837f63ae`)
- Class: `correctness-contract-violation`
- Severity: suggestion
- Location: `fprime-zephyr/Fs/FatFilenameAllocator/CMakeLists.txt:36`
- Confidence: low

**Description.** zephyr_ld_options goes through target_ld_options (zephyr cmake/modules/extensions.cmake:2469-2491). That function tests each option with zephyr_check_compiler_flag and adds it only if the check passes (target_link_libraries_ifdef). On a toolchain or linker that rejects `--wrap` (e.g. arcmwdt or IAR), or when the check misfires, FatFs silently keeps using k_malloc. The SDD tells users to set CONFIG_HEAP_MEM_POOL_SIZE=256, and k_malloc(1120) from a 256 B heap always fails. Every path-taking FatFs call would then return -ENOMEM at runtime, and getStats() would read all zeros. The CONFIG_LTO case is already turned into a hard error; this case is not. GNU ld, the toolchain that was verified, is unaffected.

**Suggested fix.** Add the flags unconditionally so an unsupported linker fails the link loudly (best-effort; verify before applying):
```suggestion
    zephyr_link_libraries(-Wl,--wrap=ff_memalloc -Wl,--wrap=ff_memfree)
```
Alternatively, check the result and stop with FATAL_ERROR, as the CONFIG_LTO branch does. cc @LeStarch @thomas-bc: low-confidence finding, please confirm.

## 5. [Correctness] **could fix** The list of 'other ff_memalloc callers' leaves out f_fdisk, and an f_fdisk request can match the slot size

- Finding key: `a5fc75bc3c339357` (site-key `7c7de68acc66160e`)
- Class: `correctness-other`
- Severity: could fix
- Location: `fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:86-95`
- Confidence: medium

**Description.** With CONFIG_FS_FATFS_MULTI_PARTITION=y, FatFs compiles f_fdisk (ff.c:6620-6647). Called with work == NULL, it calls ff_memalloc(FF_MAX_SS). Without exFAT and with MAX_LFN=255, the slot size is 512 B, the same as FF_MAX_SS=512. The request is therefore served from the pool. That is memory-safe (the slot is >= 512 B), but it contradicts the SDD's statement that only the LFN buffer is served. Zephyr itself never calls f_fdisk; only application code could.

**Suggested fix.** Add `f_fdisk()` (only with CONFIG_FS_FATFS_MULTI_PARTITION, only if called with no work buffer; its FF_MAX_SS request can match the slot size) to the list at sdd.md:89-95.


    findings: {'key': '4e7b8021201d6286', 'lens': 'correctness-review', 'class': 'correctness-other', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:9-11, 54-56', 'severity': 'must fix', 'confidence': 'high', 'description': "SDD says handle calls never take a slot and slots are held only by path-taking calls; on exFAT, f_sync and f_close do take one. sdd.md:10-11 says calls on open handles do not use the LFN buffer, and :54 says a slot is held only during a path-taking call. In the pinned FatFs (zephyrproject-rtos/fatfs@4f60a12, ff.c:4249-4270), f_sync runs DEF_NAMEBUFF/INIT_NAMEBUFF for any modified file when fs_type == FS_EXFAT, and f_close calls f_sync first (ff.c:4313). This PR is what makes that buffer a fixed-pool slot. When the pool is empty, closing a modified exFAT file fails with FR_NOT_ENOUGH_CORE before its directory entry (size, start cluster) is written. Zephyr's fatfs_close (subsys/fs/fat_fs.c:147-151) frees the FIL anyway, so the data written to that file is lost on the volume. The PR adds a false claim to its own SDD (§1a must-fix). It also understates which operations are affected by exhaustion.", 'suggested_fix': 'Correct the SDD (best-effort fix; verify before applying):
```suggestion
FatFs needs a temporary LFN buffer inside every path-taking call (`f_open`, `f_stat`, `f_unlink`, `f_rename`,
`f_mkdir`, `f_opendir`, `f_readdir`, ...). On exFAT volumes `f_sync` and `f_close` of a modified file also take one to
rewrite the directory entry. The buffer is released before the call returns. `f_read`, `f_write` and `f_lseek` do not use it.
```
Also update :54 to say "one FatFs call", not "one path-taking FatFs call". State that on exFAT, exhaustion can make `fs_close`/`fs_sync` return `-ENOMEM` after Zephyr has already released the file object.'}, {'key': '70b51bfa2f7d6d07', 'lens': 'correctness-review', 'class': 'correctness-other', 'location': 'default/zephyr-config/FatFilenameAllocatorCfg.hpp:9-11 (and fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:42-44)', 'severity': 'suggestion', 'confidence': 'medium', 'description': 'Slot demand is bounded by FF_VOLUMES under FF_FS_REENTRANT, not by thread count; the bound is neither stated nor enforced. With FF_FS_REENTRANT, every INIT_NAMEBUFF call runs after mount_volume()/validate() has taken that volume\'s mutex (ff.c lock_volume:899). The slot is released before LEAVE_FF unlocks it. So at most one slot is in use per mounted volume: concurrent holders <= FF_VOLUMES (Zephyr derives this from the devicetree disk list, zephyr_fatfs_config.h:112-121). The SDD sizing rule, "number of threads that may be inside such a call", over-provisions. More importantly, nothing ties the slot count to the actual bound. If a deployment sets SLOTS < FF_VOLUMES, exhaustion becomes possible, including the exFAT f_close data-loss path above. If SLOTS >= FF_VOLUMES, exhaustion is provably impossible.', 'suggested_fix': 'Add a compile-time guard in FprimeZephyrFatFilenameAllocator.cpp next to the FF_USE_LFN static_assert (best-effort; verify before applying):
```suggestion
#if FF_FS_REENTRANT
static_assert(FatFilenameAllocatorConfig::FPRIME_ZEPHYR_FAT_FILENAME_SLOTS >= FF_VOLUMES,
              "FatFs serializes each volume, so FF_VOLUMES slots make exhaustion impossible");
#endif
```
Alternatively, document SLOTS = number of mounted FatFs volumes as the sufficient value. cc @LeStarch @thomas-bc: confirm the preferred default.'}, {'key': 'f901c60961fe8866', 'lens': 'correctness-review', 'class': 'correctness-initialization', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:46-47', 'severity': 'suggestion', 'confidence': 'medium', 'description': "Constant initialization of s_pool is relied on (FZFA-007, SDD:129) but nothing enforces it. `Pool s_pool;` is constant-initialized only while every member of FatFilenameAllocator, including Zephyr's `struct k_spinlock`, stays a literal type and the defaulted constexpr constructor remains valid. For a class template, an invalid constexpr is silently dropped (no diagnostic required). The compiler would then emit a dynamic initializer that runs from Zephyr's z_init_static(). That happens after POST_KERNEL, where FatFs automount can already be running, so the constructor would overwrite live pool state, breaking FZFA-007. The author checked a single configuration (rp2350, v4.4.2). Other configurations (CONFIG_SPIN_VALIDATE, SMP, ticket spinlocks) change the layout of k_spinlock and have not been checked.", 'suggested_fix': 'Make the build fail if the pool stops being constant-initializable (best-effort; verify before applying):
```suggestion
//! Constant-initialized: lives in .bss and needs no startup constructor
static_assert((Pool(), true), "Pool must be constant-initializable (no startup constructor)");
Pool s_pool;
```'}, {'key': 'd7e18e92615ca362', 'lens': 'correctness-review', 'class': 'correctness-contract-violation', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/CMakeLists.txt:36', 'severity': 'suggestion', 'confidence': 'low', 'description': 'zephyr_ld_options silently drops a flag that fails its check, so the pool can be skipped without any build error. zephyr_ld_options goes through target_ld_options (zephyr cmake/modules/extensions.cmake:2469-2491). That function tests each option with zephyr_check_compiler_flag and adds it only if the check passes (target_link_libraries_ifdef). On a toolchain or linker that rejects `--wrap` (e.g. arcmwdt or IAR), or when the check misfires, FatFs silently keeps using k_malloc. The SDD tells users to set CONFIG_HEAP_MEM_POOL_SIZE=256, and k_malloc(1120) from a 256 B heap always fails. Every path-taking FatFs call would then return -ENOMEM at runtime, and getStats() would read all zeros. The CONFIG_LTO case is already turned into a hard error; this case is not. GNU ld, the toolchain that was verified, is unaffected.', 'suggested_fix': 'Add the flags unconditionally so an unsupported linker fails the link loudly (best-effort; verify before applying):
```suggestion
    zephyr_link_libraries(-Wl,--wrap=ff_memalloc -Wl,--wrap=ff_memfree)
```
Alternatively, check the result and stop with FATAL_ERROR, as the CONFIG_LTO branch does. cc @LeStarch @thomas-bc: low-confidence finding, please confirm.'}, {'key': 'a5fc75bc3c339357', 'lens': 'correctness-review', 'class': 'correctness-other', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:86-95', 'severity': 'could fix', 'confidence': 'medium', 'description': "The list of 'other ff_memalloc callers' leaves out f_fdisk, and an f_fdisk request can match the slot size. With CONFIG_FS_FATFS_MULTI_PARTITION=y, FatFs compiles f_fdisk (ff.c:6620-6647). Called with work == NULL, it calls ff_memalloc(FF_MAX_SS). Without exFAT and with MAX_LFN=255, the slot size is 512 B, the same as FF_MAX_SS=512. The request is therefore served from the pool. That is memory-safe (the slot is >= 512 B), but it contradicts the SDD's statement that only the LFN buffer is served. Zephyr itself never calls f_fdisk; only application code could.", 'suggested_fix': 'Add `f_fdisk()` (only with CONFIG_FS_FATFS_MULTI_PARTITION, only if called with no work buffer; its FF_MAX_SS request can match the slot size) to the list at sdd.md:89-95.'}
