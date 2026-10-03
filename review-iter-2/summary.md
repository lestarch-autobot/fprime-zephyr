<!-- LOCAL REVIEW ONLY — not posted to GitHub -->
# Local review summary — iteration 2

Reviewed head `165ccf3` (iteration-1 fixes), full change `b76db2b...165ccf3`, same 10 lenses / 8 local reviewer sessions as
iteration 1, definitions from `nasa/fprime@devel`. Nothing was posted to GitHub.

**Trend:** iteration 1: 3 must fix (unique) + ~30 suggestion/could fix → iteration 2: **0 must fix**, 6 unique new
suggestion/could-fix items (all documentation/comment wording except one test nit). All iteration-1 findings resolved
except two acknowledged naming nits.

| Lens | Iter-1 findings resolved | New must fix | New suggestion / could fix | Verdict |
|---|---|---|---|---|
| security-review | n/a (none) | 0 | 0 | Go |
| correctness-review | 5/5 | 0 | 1 could fix | Go |
| supply-chain-review | 1/1 | 0 | 1 could fix | Go |
| design-review | 3/4 (DR-4 partial) | 0 | 1 could fix | Go |
| operational-consequences-review | 3/3 | 0 | 1 suggestion | Go |
| fprime-code-review | 2/2 | 0 | 0 (2 cross-lens notes) | Go |
| stale-documentation-review | 5/5 | 0 | 2 suggestion, 4 could fix | Go |
| architecture-review | 1/1 | 0 | 0 (1 cross-lens note) | Go |
| test-quality-review | 1/1 | 0 | 0 | Go |
| maintainability-review | 5/7 | 0 | 2 suggestion | Go |

## Dispositions (fixed in the iteration-2 follow-up commit unless noted)

| Finding | Lens(es) | Disposition |
|---|---|---|
| SDD's `#if defined(CONFIG_FS_FATFS_LFN_MODE_HEAP)` guard for `getStats()` does not cover the CMake opt-out | DR-5, DOC-6, architecture note | Fixed: module exports `FPRIME_ZEPHYR_FAT_FILENAME_ALLOCATOR_INSTALLED=1` (INTERFACE); SDD shows the `if (TARGET ...)` + `#ifdef` pattern |
| SDD slot-count rule ("smaller of mounted volumes and threads") contradicts the `>= FF_VOLUMES` build check; `FF_VOLUMES` counts disk nodes, not mounts | OC-4, DOC-7, correctness could fix | Fixed: SDD and config comment describe the build check and what `FF_VOLUMES` counts |
| `INIT_NAMBUF` should be `INIT_NAMEBUFF` | SC-2, DOC-8 | Fixed in cpp comments, assert message, SDD |
| "exhaustion impossible" comments omit the `f_fdisk(..., NULL)` exception | MAINT-8, DOC-10, fprime-code note | Fixed: comments reworded |
| `static_assert` sits between `s_pool` doc comment and `s_pool` | MAINT-9 | Fixed: assert moved below the object |
| Requirements table lacks the new build checks | DOC-9 | Fixed: FZFA-007 verification updated, FZFA-009 (`FF_VOLUMES`) and FZFA-010 (FatFs revision) added |
| `dir_clear` warning assumes 512 B sectors and splits the bullet list | DOC-11 | Fixed: sector size stated, warning moved after the list |
| Unused `constexpr` probe variable warns under clang `-Wall` | fprime-code note | Fixed: replaced by a `static_assert` with no variable |
| `FprimeZephyrFatFilenameAllocator` name for a stats-only facade; long config-constant name | MAINT-6, MAINT-7 | Acknowledged nit: both names are mandated by the task statement |

## Verification after fixes
- Host UT `fprime-zephyr_Fs_FatFilenameAllocator_ut_exe`: 14/14 pass (ASan/UBSan) — `ut-iter2.log`.
- PROVES link (REENTRANT, LFN_MODE_HEAP, MAX_LFN=255, exFAT, HEAP_MEM_POOL_SIZE=256): `--wrap` flags on both link
  stages; 10 `__wrap_ff_memalloc` / 13 `__wrap_ff_memfree` call sites; 0 `ff_memalloc`/`ff_memfree`/`k_malloc` call
  sites; `ff_mutex_*` present; `s_pool` 4528 B in `.bss`; no allocator static constructor — `proves-link-iter2.log`.
