<!-- LOCAL REVIEW ONLY — iteration 1, head cb58109, not posted to GitHub -->

# fprime-code-review

Lens: F Prime C/C++ Design (`review_label: C++ Design`, review_group `fprime-code`), run 1, LOCAL ONLY (nothing posted).
Change: lestarch-autobot/fprime-zephyr `devin/1791051832-fat-filename-allocator`, base `b76db2b5` ... head `cb58109b`.
Rule source: nasa/fprime@devel `.github/skills/fprime-cpp-design/SKILL.md` (CPP-1..CPP-37).

Totals: must fix 1, suggestion 0, could fix 1, future work 0.
`<!-- ledger_rows: 34 -->` `<!-- unexplored_below_must_fix: 0 -->`

## Findings

### F1 - `cpp-uninitialized-variable` (CPP-19) - **must fix**
- finding-key: `fprime-code-review/cpp-uninitialized-variable/FatFilenameAllocator.hpp/FatFilenameAllocatorStats`
- site-key: `fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:15-22`
- confidence: medium

[C++ Design] **must fix** CPP-19 (initialize all variables): the public `FatFilenameAllocatorStats` members have no initializers.

Every instance in this diff is aggregate-initialized, but the struct is a public API type (returned by
`FprimeZephyrFatFilenameAllocator::getStats()`), so a caller writing `FatFilenameAllocatorStats s;` (e.g. a future
telemetry component member) gets six indeterminate `FwSizeType` fields. C++14 aggregates may carry default member
initializers, so the `getStats()` brace-init at line 106 keeps compiling unchanged. CPP-19 is memory/lifetime
(skill §3: default must fix, downgrade only for test-only code).

```suggestion
struct FatFilenameAllocatorStats {
    FwSizeType slotCount = 0U;       //!< Number of slots in the pool
    FwSizeType slotBytes = 0U;       //!< Exact request size served by each slot
    FwSizeType inUse = 0U;           //!< Slots currently allocated
    FwSizeType highWaterMark = 0U;   //!< Largest value inUse has reached
    FwSizeType exhaustedCount = 0U;  //!< Correctly sized requests refused because every slot was in use
    FwSizeType rejectedCount = 0U;   //!< Requests refused because their size was not slotBytes
};
```

### F2 - `cpp-missing-constexpr-or-const` (CPP-11) - **could fix**
- finding-key: `fprime-code-review/cpp-missing-constexpr-or-const/FatFilenameAllocator.hpp/getStats`
- site-key: `fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:104`
- confidence: low

[C++ Design] **could fix** CPP-11 (prefer const): `getStats()` is a read-only snapshot but is non-`const`, so it cannot be
called through a `const` pool reference.

It is non-const only because the guard locks `m_lock`. Declaring the lock `mutable` (the usual idiom for a
synchronization member) lets the accessor be `const`. Low confidence: CPP-11 is written about values, not member
functions, and the only caller today is the non-const `s_pool` (best-effort fix; verify before applying).

```suggestion
    FatFilenameAllocatorStats getStats() const {
```
(and at line 119: `mutable typename LockPolicy::Lock m_lock = {};`)

cc @LeStarch @thomas-bc - low-confidence finding, please confirm.

## Ledger (working state; dispositions)

Reading order (contract 13f): docs -> headers -> sources -> tests -> build.

| # | Site | Rule(s) | Disposition |
|---|---|---|---|
| 1 | `docs/sdd.md` (all hunks) | documentation currency | out of scope (stale-documentation-review) |
| 2 | `default/zephyr-config/FatFilenameAllocatorCfg.hpp:11` slot-count constant | CPP-3/8/11/28/30 | clean: `constexpr FwSizeType`, named, documented |
| 3 | `FatFilenameAllocator.hpp:15-22` stats struct members | CPP-19 | **finding F1 (must fix)** |
| 4 | `FatFilenameAllocator.hpp:15-22` stats field types | CPP-3/28 | clean: `FwSizeType` |
| 5 | `FatFilenameAllocator.hpp:31` template `<FwSizeType, FwSizeType, LockPolicy>` | CPP-7 (template complexity) | clean: one type param, no metaprogramming |
| 6 | `FatFilenameAllocator.hpp:33-34` static_asserts | CPP-5 | clean |
| 7 | `FatFilenameAllocator.hpp:38-40` SLOT_ALIGNMENT/SLOT_STRIDE | CPP-11/30/5 | clean: constexpr, derived; not odr-used, so no C++14 out-of-line definition needed |
| 8 | `FatFilenameAllocator.hpp:42-44` ctor / deleted copy ops | CPP-17/18 | clean: copy deleted, move not implicitly declared |
| 9 | `FatFilenameAllocator.hpp:49` `void* allocate(FwSizeType)` | CPP-21/13/35 | clean: mirrors the `ff_memalloc` external API; outcome split carried by counters |
| 10 | `FatFilenameAllocator.hpp:55,84` scan loops | CPP-34 | clean: bounded `for` over SLOT_COUNT |
| 11 | `FatFilenameAllocator.hpp:62` `&m_slots[i][0]` | CPP-9/10 | clean: implicit U8* -> void*, no cast |
| 12 | `FatFilenameAllocator.hpp:76` `release(void* const)` | CPP-13/21 | clean: nullptr is a valid input (FatFs contract) |
| 13 | `FatFilenameAllocator.hpp:85` `static_cast<void*>` compare | CPP-9/10 | clean |
| 14 | `FatFilenameAllocator.hpp:99` `FW_ASSERT(index < SLOT_COUNT, ...)` | CPP-4/29 | clean: operand is a pointer FatFs got from `ff_memalloc` (ff.c FREE_NAMEBUFF / dir_clear / LEAVE_MKFS only free what they allocated, or NULL); not ground- or hardware-settable; predicate side-effect-free |
| 15 | `FatFilenameAllocator.hpp:100` `FW_ASSERT(wasInUse, ...)` | CPP-4/29 | clean: same trace; under FW_NO_ASSERT expands to `((void)(cond))`, so no unused-variable warning and no state mutation lost |
| 16 | `FatFilenameAllocator.hpp:99-100` assert arg casts | CPP-9 | clean: `static_cast<FwAssertArgType>` |
| 17 | `FatFilenameAllocator.hpp:104` `getStats()` non-const | CPP-11 | **finding F2 (could fix)** |
| 18 | `FatFilenameAllocator.hpp:113-114` `m_slots` / `m_inUse` C arrays | CPP-22/21/1 | clean: private storage of a boot-time-sized pool (CPP-1 "object pools" allowed); Fw/DataStructures containers are not constant-initializable and would add the startup constructor FZFA-007 forbids |
| 19 | `FatFilenameAllocator.hpp:113-119` member initializers | CPP-19 | clean: all members have `= {}` / `= 0U` |
| 20 | `FatFilenameAllocator.hpp:28-30` Doxygen line wrap ("from\n//! `Lock&`") | comment formatting | out of scope (maintainability-review) |
| 21 | `FprimeZephyrFatFilenameAllocator.hpp:13-19` static-only class | CPP-17/18/20 | clean: ctor deleted, no state |
| 22 | `FprimeZephyrFatFilenameAllocator.cpp:12` `static_assert(FF_USE_LFN == 3)` | CPP-5/8 | clean |
| 23 | `.cpp:18` LFN_BUFFER_BYTES `(FF_MAX_LFN+1)*sizeof(WCHAR)` | CPP-30 | clean: matches ff.c `INIT_NAMEBUFF` `(FF_MAX_LFN+1)*2` (WCHAR is WORD) |
| 24 | `.cpp:21` EXFAT_DIR_BLOCK_BYTES literals 44/15/32 | CPP-30/8 | clean: named constant with derivation comment; matches ff.c `MAXDIRB` / `SZDIRE` (Zephyr fatfs ff.c:190,521). Drift risk vs. a private ff.c macro noted for correctness/maintainability lenses |
| 25 | `.cpp:27-40` ZephyrSpinLockPolicy / Guard | CPP-17/18/19/32 | clean: explicit ctor, members initialized in init list, copy deleted, `k_spin_lock` key retained |
| 26 | `.cpp:28` `using Lock = struct k_spinlock;` | CPP-12 | clean: type alias, not `using namespace` |
| 27 | `.cpp:47` `Pool s_pool;` anonymous-namespace global | CPP-19/1 | clean: all members default-initialized; constant-initialized |
| 28 | `.cpp:59-68` `__wrap_ff_memalloc(UINT)` / `__wrap_ff_memfree` | CPP-3/9/26 | clean: `UINT` is the FatFs external API type; `static_cast<FwSizeType>`; reserved names justified by inline comment |
| 29 | dir_clear() request sizes vs. slot size (ff.c:1694) | CPP-35 / design claim | clean for this lens: dir_clear only asks for sizes > SS(fs) (>= 1024), and no LFN slot size in the Kconfig range MAX_LFN 12..255 is a power of two above 512 (non-exFAT max 512 B; exFAT max 1120 B, only power of two is 128 B), so dir_clear can never be served a slot |
| 30 | test `FatFilenameAllocatorTestMain.cpp` STL/`std::thread`/`std::mutex`/`std::vector` | CPP-25 | clean: UT is registered only in the non-Zephyr `elseif (BUILD_TESTING)` branch, i.e. not built with the flight toolchain config |
| 31 | test line 272 lambda; line 103 `reinterpret_cast<std::uintptr_t>`; line 283 CAS `while` | CPP-7/10/34 | out of scope: agent file scopes test sources to CPP-25/CPP-19 only (test-quality-review for test substance) |
| 32 | test fixture / hook members (`m_count`, `m_firstLine`, `m_firstArg`, arrays `= {}`) | CPP-19 | clean: all initialized |
| 33 | test `override` on `reportAssert` / `doAssert` | CPP-15 | clean |
| 34 | CMake: `fprime-zephyr.cmake`, `Fs/CMakeLists.txt`, `FatFilenameAllocator/CMakeLists.txt` (`--wrap`, `app` link, LTO guard), `default/zephyr-config/CMakeLists.txt` | build system | out of scope (supply-chain-review / design-review) |

Rules with no candidate site in this diff: CPP-2 (no Fw::Buffer), CPP-6 (no NULL), CPP-14/16/20 (no inheritance or friend in production), CPP-23/24/31/36/37 (no ground interface, strings, events or commands), CPP-33 (pool logic is already a reusable module).

## Out-of-scope observations (for other lenses; not findings of this lens)
- None material. Row 24 drift risk (duplicated private `MAXDIRB` formula) would surface immediately as every path call failing with `FR_NOT_ENOUGH_CORE`; correctness/maintainability lenses may weigh it.

    findings: {'key': 'fprime-code-review/cpp-uninitialized-variable/FatFilenameAllocator.hpp/FatFilenameAllocatorStats', 'lens': 'fprime-code-review', 'class': 'cpp-uninitialized-variable', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:15-22', 'severity': 'must fix', 'confidence': 'medium', 'description': 'CPP-19: public API struct FatFilenameAllocatorStats declares six FwSizeType members with no initializers; any default-declared instance (e.g. a future telemetry component member) holds indeterminate values. All instances in this diff are aggregate-initialized, but CPP-19 requires explicit initializers on every member (memory/lifetime cluster: default must fix).', 'suggested_fix': 'Add default member initializers (`FwSizeType slotCount = 0U;` etc.) to every field; C++14 aggregates allow NSDMI, so the brace-init in getStats() still compiles.'}, {'key': 'fprime-code-review/cpp-missing-constexpr-or-const/FatFilenameAllocator.hpp/getStats', 'lens': 'fprime-code-review', 'class': 'cpp-missing-constexpr-or-const', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/FatFilenameAllocator.hpp:104', 'severity': 'could fix', 'confidence': 'low', 'description': 'CPP-11: getStats() is a read-only snapshot but is non-const (only because it locks m_lock), so it cannot be called through a const pool reference. Low confidence: CPP-11 targets values more than member functions; only caller today is the non-const s_pool.', 'suggested_fix': 'Declare `mutable typename LockPolicy::Lock m_lock = {};` and make `FatFilenameAllocatorStats getStats() const`.'}
