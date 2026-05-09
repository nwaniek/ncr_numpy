# ncr_numpy benchmarks

Self-contained benchmark harness for the four hot paths in `ncr_numpy.hpp`:
loading a large `.npy`, byte-swapping a large array in-place via
`apply`, iterating a large array via per-item callbacks, and loading a
multi-array `.npz`. Two binaries are produced so the mmap fast path and
the vector fallback can be A/B compared without rebuilding from scratch.

```
make
./bench               # generates ~1.7 GiB of fixtures on first run
./bench_no_mmap       # same binary, built with -DNCR_NUMPY_NO_MMAP_LOAD
```

Fixtures live in `assets/` and are not regenerated on subsequent runs;
generation uses raw `.npy` writes (no Python/numpy required). Use
`make clean-fixtures` to remove them.

## Build flags

| binary           | NCR_NUMPY_NO_MMAP_LOAD | what `from_npy()` does          |
|------------------|------------------------|---------------------------------|
| `bench`          | undefined              | mmap when ≥ 64 KiB, else ifstream |
| `bench_no_mmap`  | defined                | always ifstream (vector path)   |

The macro is the *only* difference between the two binaries. It only
affects the `from_npy()` whole-array load: `from_npy_callback` always
uses chunked ifstream reads regardless, and `loadz` uses libzip
regardless.

## What is measured

- **(a) load 1 GiB f32.** Runs the vector path explicitly, then
  `from_npy()` (which is mmap by default and the vector path when
  `NCR_NUMPY_NO_MMAP_LOAD` is set; in the latter case the row is
  *skipped* rather than mislabeled). The mmap variant also reports a
  "touch all pages" row so you can see how much of the apparent speed-up
  is deferred to first access.
- **(b) byte-swap a 512 MiB f64.** Reports a separate **COW prepass**
  row that write-touches every 4 KiB page up front (using a volatile
  store the compiler cannot elide). On an mmap'd buffer this fires
  copy-on-write for the whole mapping; on a heap buffer the same loop
  is just a cache warm-up. Then runs three apply variants on the now-
  warm, private-page buffer: `apply<f64>(bswap<f64>)`, the in-place
  `apply(u8_span -> void)`, and the legacy
  `apply(u8_const_span -> u8_vector)`.
- **(c) per-item callbacks over 256 MiB u64.** Typed flat callback,
  span-based generic callback, and the legacy vector-returning generic
  callback.
- **(d) loadz of a 100-array .npz.** libzip latency baseline.

`drop_page_cache()` is called between (a) and (c) measurements but only
takes effect when run as root (it writes to `/proc/sys/vm/drop_caches`).
The banner reports whether cold-cache mode is active so warm-cache
numbers can be read as such.

## Sample results

Measured on gcc 16, `-O3 -march=native`, kernel 7.0, warm cache.
Numbers vary run-to-run; structural differences are stable.

```
=== bench (mmap on) ===
(a) load big_f32_1g.npy (1024 MiB)
  from_npy_ifstream (vector path)            684.6 ms     1.46 GiB/s
  from_npy (mmap, no touch)                    0.07 ms ~14000 GiB/s
  from_npy (mmap, touch all pages)            38.7 ms    25.83 GiB/s

(b) bswap apply<f64> on big_f64_512m.npy (512 MiB payload)
  COW prepass (one byte per 4 KiB)           313.1 ms     1.60 GiB/s
  apply<f64>(bswap<f64>)                      74.6 ms     6.71 GiB/s
  apply(span -> void)  in-place               79.4 ms     6.30 GiB/s
  apply(span -> u8_vector) legacy            472.4 ms     1.06 GiB/s

(c) per-item callbacks over small_u64_256m.npy (256 MiB, u64)
  typed flat callback                         77.3 ms     3.23 GiB/s
  generic span callback                       78.7 ms     3.18 GiB/s
  generic vector callback (legacy)           332.3 ms     0.75 GiB/s

(d) load .npz many_arrays.npz                  1.9 ms

=== bench_no_mmap ===
(a) load big_f32_1g.npy (1024 MiB)
  from_npy_ifstream (vector path)            656.6 ms     1.52 GiB/s
  (mmap path skipped: NCR_NUMPY_NO_MMAP_LOAD is set,
   so from_npy() collapses to from_npy_ifstream in this build.)

(b) bswap apply<f64> on big_f64_512m.npy (512 MiB payload)
  COW prepass (one byte per 4 KiB)             2.2 ms   229.3 GiB/s
  apply<f64>(bswap<f64>)                      73.6 ms     6.80 GiB/s
  apply(span -> void)  in-place               77.9 ms     6.42 GiB/s
  apply(span -> u8_vector) legacy            474.0 ms     1.06 GiB/s

(c) per-item callbacks over small_u64_256m.npy (256 MiB, u64)
  typed flat callback                         77.8 ms     3.21 GiB/s
  generic span callback                       77.9 ms     3.21 GiB/s
  generic vector callback (legacy)           350.4 ms     0.71 GiB/s

(d) load .npz many_arrays.npz                  2.0 ms
```

## How to read the numbers

**(a) is the mmap-vs-vector trade-off, made explicit.** mmap returns
from `from_npy()` in microseconds because it just establishes a VMA;
the actual page-cache -> process-address-space cost (~40 ms here for
1 GiB on warm cache) is paid by the first access and shown on the
"touch all pages" row. The vector path pays its full I/O cost (~660 ms)
upfront. When `NCR_NUMPY_NO_MMAP_LOAD` is defined `from_npy()` is just
the vector path, so the mmap rows are skipped — earlier versions of
this bench printed them anyway with a misleading label.

**(b) is the most important section to read carefully.** The
`apply<f64>(bswap<f64>)` row used to look ~7× slower on mmap than on
heap, which suggested the typed apply was failing to vectorize. It
isn't. The cost is **copy-on-write of the entire mapping** taken on
the first mutating pass: with `MAP_PRIVATE | PROT_READ | PROT_WRITE`,
each 4 KiB page is shared-clean until the first write to it, at which
point the kernel allocates a private anon page, copies the original
contents, and updates the PTE. On a 512 MiB mapping that's ~131k page
faults at a few microseconds each: the ~313 ms shown on the COW
prepass row.

The previous warm-up loop (`p[i] ^= 0`) was supposed to pre-fault and
pre-COW so the apply rows would all start from the same baseline. With
`-O3` and no `volatile`, gcc proves `x ^ 0 == x` and DCEs the entire
loop: no faults, no COW, no warm-up. Whichever apply ran first then
absorbed the full 313 ms. The current `cow_prepass` uses a volatile
store the compiler cannot elide, so the COW shows up where it
belongs and all three apply variants run at their actual transform
speed (~6.5 GiB/s) on both backends.

The legacy `apply(u8_const_span -> u8_vector)` is dominated by per-item
heap alloc/free (~470 ms in both binaries); the underlying buffer kind
is irrelevant for it.

**(c) is unaffected by the build flag** because `from_npy_callback`
always uses chunked ifstream reads. Typed flat and the new span-based
generic callback tie at ~3.2 GiB/s; the legacy vector-returning
callback loses ~4× to per-item heap allocation.

**(d) is a libzip latency baseline.** 19 KiB file, ~2 ms: small file,
small file structures, bound by libzip's per-entry overhead.

## Headlines

1. mmap eliminates the `from_npy` payload copy entirely; the cost
   transfers to first access (~40 ms for 1 GiB warm-cache), which is
   far smaller than the ~660 ms the vector path pays during load.
2. On a `MAP_PRIVATE` buffer, the **first mutating pass** also pays
   the COW cost for the whole mapping (~310 ms for 512 MiB). This is
   structurally fair to the I/O winner -- the vector path pre-pays the
   I/O during load, mmap defers it -- but the cost has to be attributed
   to COW, not to the apply operation that happens to run first.
3. `apply<T>` and the new in-place `apply(u8_span -> void)` tie at
   ~6.5 GiB/s on both backends. Both auto-vectorize cleanly under `-O3`.
   The legacy `apply(u8_const_span -> u8_vector)` is ~6× slower
   because every iteration heap-allocates the result buffer.
4. The new span-based generic callback removes per-item heap allocs
   without changing the per-item dispatch cost; it ties the typed
   callback (~3.2 GiB/s vs ~0.75 GiB/s for the vector legacy form).
