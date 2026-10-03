<!-- LOCAL REVIEW ONLY — iteration 1, head cb58109, not posted to GitHub -->

# maintainability-review

Lens: `maintainability-review` (review_label `Maintainability`), review_group `maintainability`, iteration 1 (run 1).
Change: lestarch-autobot/fprime-zephyr `b76db2b5...cb58109b` (LOCAL review, nothing posted).
Metadata: `<!-- ledger_rows: 24 -->` `<!-- unexplored_below_must_fix: 0 -->`
Totals: must fix 0, suggestion 3, could fix 4, future work 0.

Reading order (contract §13f): docs/sdd.md -> FatFilenameAllocatorCfg.hpp, FatFilenameAllocator.hpp, FprimeZephyrFatFilenameAllocator.hpp -> FprimeZephyrFatFilenameAllocator.cpp -> test/ut/FatFilenameAllocatorTestMain.cpp -> CMakeLists.txt files, fprime-zephyr.cmake.

---

### MAINT-1 — `maint-unclear-parameters` — **suggestion** — confidence: high
Location: `fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:106-108`

[Maintainability] **suggestion** `maint-unclear-parameters`: six same-typed `FwSizeType` fields filled by position.

`FatFilenameAllocatorStats` is initialized positionally with six `FwSizeType` values (clang-format already
mis-columns them). Reordering or inserting a struct field silently swaps counters (e.g. `exhaustedCount` <->
`rejectedCount`) with no compile error. C++14 has no designated initializers; assign by name.

```suggestion
        FatFilenameAllocatorStats stats = {};
        stats.slotCount = SLOT_COUNT;
        stats.slotBytes = SLOT_BYTES;
        stats.inUse = this->m_inUseCount;
        stats.highWaterMark = this->m_highWaterMark;
        stats.exhaustedCount = this->m_exhaustedCount;
        stats.rejectedCount = this->m_rejectedCount;
```

<!-- fprime-agent: maintainability-review; finding-key: maint-unclear-parameters:FatFilenameAllocator.hpp:getStats; v1 -->

---

### MAINT-2 — `maint-structural-hazard` — **suggestion** — confidence: high
Location: `fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:113` (with line 38)

[Maintainability] **suggestion** `maint-structural-hazard`: slot alignment is stated twice.

`SLOT_STRIDE` is computed from `SLOT_ALIGNMENT` (line 38), but the storage is aligned with a separate
`alignas(std::max_align_t)`. Editing `SLOT_ALIGNMENT` (e.g. to a cache-line size) changes the stride but not the
array's base alignment, so slots silently stop being aligned to the stated value. Use the named constant.

```suggestion
    alignas(SLOT_ALIGNMENT) U8 m_slots[SLOT_COUNT][SLOT_STRIDE] = {};  //!< Slot storage
```

<!-- fprime-agent: maintainability-review; finding-key: maint-structural-hazard:FatFilenameAllocator.hpp:m_slots-alignas; v1 -->

---

### MAINT-3 — `maint-structural-hazard` — **could fix** — confidence: medium
Location: `fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:18-24`

[Maintainability] **could fix** `maint-structural-hazard`: slot size copies private FatFs internals, unpinned.

`EXFAT_DIR_BLOCK_BYTES` copies `MAXDIRB()`/`SZDIRE` from `ff.c` (private, not in `<ff.h>`), and the total must equal
`INIT_NAMBUF`'s request byte-for-byte. If a Zephyr upgrade changes either, the build still passes and every
path-taking FatFs call fails with `FR_NOT_ENOUGH_CORE` (all requests counted as `rejected`). Pin the FatFs revision
that was checked, so an upgrade forces someone to check again. Below must-fix: nothing is wrong today. (Best-effort
fix; check the revision value before applying.)

```suggestion
// Slot size mirrors INIT_NAMBUF / MAXDIRB in ff.c; re-verify both when FatFs is upgraded
static_assert(FF_DEFINED == <revision verified against>, "FatFs revision changed: re-verify LFN slot size");
```

<!-- fprime-agent: maintainability-review; finding-key: maint-structural-hazard:FprimeZephyrFatFilenameAllocator.cpp:EXFAT_DIR_BLOCK_BYTES; v1 -->

---

### MAINT-4 — `maint-structural-hazard` — **could fix** — confidence: medium
Location: `fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:46-47` (relies on `FatFilenameAllocator.hpp:42,113-119`)

[Maintainability] **could fix** `maint-structural-hazard`: "constant-initialized" is not enforced by anything.

The comment (and SDD requirement FZFA-007) depends on every member initializer staying constant-evaluable. Under
C++14 there is no `constinit`. If someone later adds a member whose initializer is not constexpr, the compiler
silently switches to dynamic initialization and adds a startup constructor. No diagnostic, and the comment is then
wrong. Add a compile-time check, e.g. in the UT with a literal (no-op) lock policy:

```suggestion
struct NoLockPolicy { struct Lock {}; struct Guard { explicit Guard(Lock&) {} }; };
// Fails to compile if FatFilenameAllocator's default construction stops being constant-evaluable
constexpr Zephyr::FatFilenameAllocator<TEST_SLOT_BYTES, TEST_SLOT_COUNT, NoLockPolicy> CONSTANT_INIT_PROBE{};
```

(Best-effort fix; verify before applying. cc @LeStarch @thomas-bc — low-confidence on the mechanism, please confirm.)

<!-- fprime-agent: maintainability-review; finding-key: maint-structural-hazard:FprimeZephyrFatFilenameAllocator.cpp:s_pool-constinit; v1 -->

---

### MAINT-5 — `maint-structural-hazard` — **suggestion** — confidence: high
Location: `fprime-zephyr/Fs/FatFilenameAllocator/CMakeLists.txt:10-12` (ladder 7-46)

[Maintainability] **suggestion** `maint-structural-hazard`: early `return()` inside one branch of the platform ladder.

The Zephyr branch can `return()` from the middle of the file, but the `elseif (BUILD_TESTING)` branch falls through
to `endif()`. Anything added after line 46 later would run for native builds and for Zephyr builds that use the
allocator, and would be silently skipped for Zephyr builds that opt out or do not use HEAP mode. Nest the condition
so every path leaves through `endif()`.

```suggestion
    if (CONFIG_FS_FATFS_LFN_MODE_HEAP AND FPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR)
        # ... existing LTO check, register_fprime_module, skip_on_sub_build, app link, zephyr_ld_options ...
    endif()
```

Note: `skip_on_sub_build()` (line 29) also returns early by design and has the same property. Its scope should be
stated in a one-line comment if it stays.

<!-- fprime-agent: maintainability-review; finding-key: maint-structural-hazard:FatFilenameAllocator/CMakeLists.txt:early-return; v1 -->

---

### MAINT-6 — `maint-unclear-naming` — **could fix** — confidence: medium
Location: `fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.hpp:13`

[Maintainability] **could fix** `maint-unclear-naming`: two names one prefix apart, and the "Allocator" one does not allocate.

`Zephyr::FatFilenameAllocator` is the pool. `Zephyr::FprimeZephyrFatFilenameAllocator` is a non-instantiable class
whose only member is `getStats()`. A reader will expect the second name to allocate (or to be the instance), and the
`FprimeZephyr` prefix repeats the enclosing `Zephyr` namespace. A name such as `FatFilenamePool` (instance) /
`FatFilenamePoolStats::get()`, or a free function `Zephyr::getFatFilenameAllocatorStats()`, says what the type
does. Could fix rather than suggestion: the rename touches the header, .cpp, CMake, and SDD.

<!-- fprime-agent: maintainability-review; finding-key: maint-unclear-naming:FprimeZephyrFatFilenameAllocator.hpp:class-name; v1 -->

---

### MAINT-7 — `maint-inconsistent-local-convention` — **could fix** — confidence: medium
Location: `default/zephyr-config/FatFilenameAllocatorCfg.hpp:11`

[Maintainability] **could fix** `maint-inconsistent-local-convention`: config constant repeats its namespace.

The one sibling config, `LoRaCfg.hpp`, uses short names scoped by the namespace (`LoRaConfig::FREQUENCY`,
`LoRaConfig::TX_POWER`). `FatFilenameAllocatorConfig::FPRIME_ZEPHYR_FAT_FILENAME_SLOTS` repeats the namespace in a
macro-style name, so the qualified name says "fat filename" twice. Consider `FatFilenameAllocatorConfig::SLOT_COUNT`
(also update the `.cpp` and the SDD). Could fix: the rename touches 3 files. The precedent is a single file, so
confidence is medium.

<!-- fprime-agent: maintainability-review; finding-key: maint-inconsistent-local-convention:FatFilenameAllocatorCfg.hpp:FPRIME_ZEPHYR_FAT_FILENAME_SLOTS; v1 -->

---

## Ledger (contract §13e)

| # | Unit | Disposition |
|---|---|---|
| 1 | docs/sdd.md (all hunks) | out of scope (stale-documentation-review) |
| 2 | FatFilenameAllocatorCfg.hpp: header guard / namespace | clean (matches file-local style) |
| 3 | FatFilenameAllocatorCfg.hpp:11 constant name | finding (MAINT-7, could fix) |
| 4 | FatFilenameAllocatorCfg.hpp:9-10 comment | clean (accurate, 2 lines) |
| 5 | FatFilenameAllocator.hpp:15-22 Stats struct names | clean (names match meaning) |
| 6 | FatFilenameAllocator.hpp:28-30 Doxygen wrap artifact | out of scope (clang-format) |
| 7 | FatFilenameAllocator.hpp:38-40 SLOT_ALIGNMENT/STRIDE | clean in itself; see row 13 |
| 8 | FatFilenameAllocator.hpp:49-71 allocate() size/nesting/exit | clean (single exit, depth 4, braces) |
| 9 | FatFilenameAllocator.hpp:47-48 allocate return (nullptr sentinel) | clean (contract of ff_memalloc) |
| 10 | FatFilenameAllocator.hpp:76-101 release() control flow | clean (null guard; asserts outside lock, commented) |
| 11 | FatFilenameAllocator.hpp:98 comment | clean (accurate rationale) |
| 12 | FatFilenameAllocator.hpp:104-110 getStats positional init | finding (MAINT-1, suggestion) |
| 13 | FatFilenameAllocator.hpp:113 alignas duplicate source | finding (MAINT-2, suggestion) |
| 14 | FatFilenameAllocator.hpp:113-119 member naming (m_ prefix) | clean |
| 15 | FatFilenameAllocator.hpp:100 FW_ASSERT on foreign/double free | out of scope (security/correctness; user-approved design) |
| 16 | FprimeZephyrFatFilenameAllocator.hpp:13 class name | finding (MAINT-6, could fix) |
| 17 | .cpp:12 static_assert FF_USE_LFN==3 | clean (clear message) |
| 18 | .cpp:18-24 LFN/exFAT size formula | finding (MAINT-3, could fix) |
| 19 | .cpp:26-40 ZephyrSpinLockPolicy | clean (RAII, non-copyable, comment accurate) |
| 20 | .cpp:46-47 s_pool constant-init claim | finding (MAINT-4, could fix, low-confidence ping) |
| 21 | .cpp:57-69 __wrap_* shims + reserved-identifier comment | clean |
| 22 | test/ut/FatFilenameAllocatorTestMain.cpp | out of scope (test-quality-review) |
| 23 | Fs/FatFilenameAllocator/CMakeLists.txt early return() | finding (MAINT-5, suggestion) |
| 24 | default/zephyr-config/CMakeLists.txt, Fs/CMakeLists.txt, fprime-zephyr.cmake | clean (one-line additions following existing pattern) |

Dead code / commented-out code / debug leftovers: none introduced. Duplication between production blocks: none
introduced (the test's hard-coded 1120 B mirrors the .cpp formula, which is test-quality scope).

    findings: {'key': 'maint-unclear-parameters:FatFilenameAllocator.hpp:getStats', 'lens': 'maintainability-review', 'class': 'maint-unclear-parameters', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:106-108', 'severity': 'suggestion', 'confidence': 'high', 'description': 'getStats() fills FatFilenameAllocatorStats with six FwSizeType values by position. If a struct field is reordered or inserted, the counters (e.g. exhaustedCount/rejectedCount) silently swap with no compile error.', 'suggested_fix': 'Value-initialize stats = {} and set each field by name (C++14 has no designated initializers).'}, {'key': 'maint-structural-hazard:FatFilenameAllocator.hpp:m_slots-alignas', 'lens': 'maintainability-review', 'class': 'maint-structural-hazard', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:113', 'severity': 'suggestion', 'confidence': 'high', 'description': 'SLOT_STRIDE comes from SLOT_ALIGNMENT (line 38), but m_slots uses a separate alignas(std::max_align_t). If SLOT_ALIGNMENT is edited, the stride changes but the base alignment does not, so slots silently lose the stated alignment.', 'suggested_fix': 'Use alignas(SLOT_ALIGNMENT) on m_slots.'}, {'key': 'maint-structural-hazard:FprimeZephyrFatFilenameAllocator.cpp:EXFAT_DIR_BLOCK_BYTES', 'lens': 'maintainability-review', 'class': 'maint-structural-hazard', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:18-24', 'severity': 'could fix', 'confidence': 'medium', 'description': 'The slot size copies private ff.c internals (MAXDIRB/SZDIRE, INIT_NAMBUF) that are not in <ff.h>. If a FatFs/Zephyr upgrade changes them, the build still passes and every path-taking FatFs call fails with FR_NOT_ENOUGH_CORE (every request counted as rejected).', 'suggested_fix': 'Add a static_assert on FF_DEFINED for the verified FatFs revision, plus a one-line note to re-check INIT_NAMBUF/MAXDIRB on upgrade.'}, {'key': 'maint-structural-hazard:FprimeZephyrFatFilenameAllocator.cpp:s_pool-constinit', 'lens': 'maintainability-review', 'class': 'maint-structural-hazard', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:46-47', 'severity': 'could fix', 'confidence': 'medium (low on mechanism; maintainer ping)', 'description': "The 'Constant-initialized, no startup constructor' comment (and SDD FZFA-007) is not enforced. Under C++14 (no constinit), a future member initializer that is not constexpr silently switches s_pool to dynamic initialization and makes the comment wrong.", 'suggested_fix': 'Add a compile-time probe, e.g. in the UT a constexpr FatFilenameAllocator<..., NoLockPolicy> instance with a literal no-op lock policy, so the build fails if default construction stops being constant-evaluable.'}, {'key': 'maint-structural-hazard:FatFilenameAllocator/CMakeLists.txt:early-return', 'lens': 'maintainability-review', 'class': 'maint-structural-hazard', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/CMakeLists.txt:10-12', 'severity': 'suggestion', 'confidence': 'high', 'description': 'The Zephyr branch can return() from the middle of the file, while the elseif(BUILD_TESTING) branch falls through to endif(). Code added after line 46 later would be silently skipped for Zephyr builds that opt out or do not use HEAP mode. skip_on_sub_build() (line 29) has the same property.', 'suggested_fix': 'Nest the rest of the Zephyr branch under if(CONFIG_FS_FATFS_LFN_MODE_HEAP AND FPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR) ... endif() instead of return().'}, {'key': 'maint-unclear-naming:FprimeZephyrFatFilenameAllocator.hpp:class-name', 'lens': 'maintainability-review', 'class': 'maint-unclear-naming', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.hpp:13', 'severity': 'could fix', 'confidence': 'medium', 'description': 'Zephyr::FprimeZephyrFatFilenameAllocator is a non-instantiable class holding only getStats(), and its name differs from the pool template Zephyr::FatFilenameAllocator only by a prefix that repeats the namespace. Readers will expect it to allocate.', 'suggested_fix': 'Rename to describe the role, e.g. a free function Zephyr::getFatFilenameAllocatorStats() or a FatFilenamePoolStats accessor. The rename touches the header, .cpp, and SDD.'}, {'key': 'maint-inconsistent-local-convention:FatFilenameAllocatorCfg.hpp:FPRIME_ZEPHYR_FAT_FILENAME_SLOTS', 'lens': 'maintainability-review', 'class': 'maint-inconsistent-local-convention', 'location': 'default/zephyr-config/FatFilenameAllocatorCfg.hpp:11', 'severity': 'could fix', 'confidence': 'medium', 'description': 'The sibling LoRaCfg.hpp uses short names scoped by the namespace (LoRaConfig::FREQUENCY). FatFilenameAllocatorConfig::FPRIME_ZEPHYR_FAT_FILENAME_SLOTS repeats the namespace in a macro-style name.', 'suggested_fix': 'Rename to FatFilenameAllocatorConfig::SLOT_COUNT and update the .cpp and SDD.'}
