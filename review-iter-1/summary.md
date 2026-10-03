<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# Local review summary — iteration 1

Change: `lestarch-autobot/fprime-zephyr` `devin/1791051832-fat-filename-allocator`, base `b76db2b` .. head `cb58109`.
Definitions: `nasa/fprime@devel` `.github/agents` (review-orchestrator, agent-registry, review-contract, 10 reviewer lenses in 8 groups).
All reviewers ran locally; nothing was posted to GitHub.

| Lens | must fix | suggestion | could fix | future work |
|---|---|---|---|---|
| security-review | 0 | 0 | 0 | 0 |
| correctness-review | 1 | 3 | 1 | 0 |
| supply-chain-review | 0 | 1 | 0 | 0 |
| design-review | 0 | 2 | 2 | 0 |
| operational-consequences-review | 1 | 2 | 0 | 0 |
| fprime-code-review | 1 | 0 | 1 | 0 |
| stale-documentation-review | 0 | 3 | 2 | 0 |
| architecture-review | 0 | 1 | 0 | 0 |
| test-quality-review | 0 | 0 | 1 | 0 |
| maintainability-review | 0 | 3 | 4 | 0 |

Unique findings after de-duplicating across lenses: 3 must fix, all fixed.

## Dispositions (fixed in the iteration-1 follow-up commit unless noted)

| Finding | Lens(es) | Disposition |
|---|---|---|
| SDD claims handle calls never take a slot; with exFAT `f_sync`/`f_close` do, and exhaustion there loses the directory-entry update | correctness 1, operational OC-1 (**must fix**) | Fixed: SDD "Why" and slot-count text corrected, WARNING added; exhaustion made impossible under REENTRANT by the `FF_VOLUMES` static_assert below |
| `FatFilenameAllocatorStats` members uninitialized (CPP-19) | fprime-code F1 (**must fix**) | Fixed: default member initializers; `getStats()` assigns fields by name |
| Slot demand is bounded by the per-volume mutex (`min(threads, mounted volumes)`), not thread count; nothing ties slot count to it | correctness 2, design DR-1, operational OC-2 | Fixed: SDD/config comment state the bound; `static_assert(SLOTS >= FF_VOLUMES)` when `FF_FS_REENTRANT`. Default stays 4 (approved); PROVES has `FF_VOLUMES = 1` |
| Constant initialization of the pool not enforced | correctness 3, maintainability | Fixed: `static_assert((Pool(), true))` in the Zephyr TU and a `constexpr` pool probe in the UT |
| `zephyr_ld_options` silently drops a rejected flag | correctness 4 | Fixed: `zephyr_link_libraries(-Wl,--wrap=...)`, so an unsupported linker fails loudly |
| SDD omits `f_fdisk()` caller | correctness 5, stale-docs | Fixed in SDD |
| Slot size mirrors private FatFs macros with no revision guard | supply-chain | Fixed: `static_assert(FF_DEFINED == 80386)` (R0.16, verified in the Zephyr v4.4.2 fatfs module) |
| SDD says Zephyr allocator "stays in the image"; `dir_clear` halving wording; README has no mention | stale-docs | Fixed: SDD wording; README section added |
| Doxygen `\tparam LockPolicy` wrap | stale-docs, maintainability | Fixed |
| `alignas(std::max_align_t)` vs `SLOT_ALIGNMENT`; nested CMake early `return()`/`skip_on_sub_build` | maintainability | Fixed |
| `getStats()` not `const` | fprime-code F2 | Fixed (`mutable` lock) |
| No stated reason for new `Fs` package vs `Os/` | architecture | Fixed: rationale added to SDD |
| `k_mem_slab` not considered | design DR-2 | Fixed: rationale added to SDD (no size check/counters, `__ASSERT` only, not host-testable) |
| Behavior change for existing HEAP-mode deployments | design DR-3 | Fixed: "Upgrading" section in SDD |
| Accessor exists only when the pool is installed | design DR-4 | Fixed: documented in SDD |
| `dir_clear` single-sector burst under volume mutex | operational OC-3 | Fixed: WARNING in SDD (behavior is approved decision 2) |
| Repeated invalid-release assertion pattern in UT | test-quality | Fixed: `expectReleaseAssert()` helper |
| `FprimeZephyrFatFilenameAllocator` name for a stats-only facade | maintainability | Acknowledged: the name is mandated by the task |

## Verification after fixes
- Host UT `fprime-zephyr_Fs_FatFilenameAllocator_ut_exe`: 14/14 pass (ASan/UBSan) — `ut-iter1.log`.
- PROVES link (REENTRANT, LFN_MODE_HEAP, MAX_LFN=255, exFAT, HEAP_MEM_POOL_SIZE=256): links; FatFs calls reach only `__wrap_ff_memalloc`/`__wrap_ff_memfree` (10/13 call sites in dir_clear, f_mkdir, f_mkfs, f_open, f_opendir, f_readdir, f_rename, f_stat, f_sync, f_unlink); no `ff_memalloc`/`ff_memfree`/`k_malloc` call sites; `ff_mutex_*` present; no static constructor — `proves-link-iter1.log`.
