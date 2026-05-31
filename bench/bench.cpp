/*
 * bench/bench.cpp - performance baseline for ncr_numpy.
 *
 * Measures the following four cases:
 *   (a) load 1 GiB f32 .npy  - mmap vs vector
 *   (b) byte-swap a big f64 array via apply<f64>(bswap<f64>)
 *   (c) iterate items via typed callback (chunked vs hypothetical-unchunked)
 *   (d) load a multi-array .npz
 *
 * Test files are generated on first run if they don't exist already; this
 * keeps the bench self-contained (no python/numpy needed). Files live in
 * bench/assets/ next to this binary.
 *
 * Build via the local Makefile.
 */

#define NCR_NUMPY_STANDALONE
#include "../ncr_numpy.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <random>
#include <string>
#include <vector>

#if defined(__linux__)
	#include <unistd.h>
#endif

using namespace ncr;
namespace fs = std::filesystem;

using clk = std::chrono::steady_clock;


/*
 * print a timing line. Throughput is computed from the supplied byte count.
 */
static void
print_timing(const std::string &label, double seconds, u64 bytes = 0)
{
	std::cout << "  " << std::left << std::setw(40) << label
		<< std::right << std::setw(10) << std::fixed << std::setprecision(3)
		<< (seconds * 1000.0) << " ms";
	if (bytes > 0) {
		double gibps = (double)bytes / seconds / (1024.0 * 1024.0 * 1024.0);
		std::cout << "    " << std::setw(8) << std::setprecision(3) << gibps << " GiB/s";
	}
	std::cout << "\n";
}


/*
 * write_npy_raw - emit a minimal version-1.0 .npy file directly. Avoids
 * having to build an in-memory ndarray for very large test fixtures.
 *
 * `descr` is the numpy dtype description string (e.g. "<f4", "<f8", "<u8").
 * `shape` is the array shape, written as a python tuple. `payload` is the
 * raw bytes that follow the header.
 */
static bool
write_npy_raw(const fs::path &path,
              const std::string &descr,
              const std::vector<u64> &shape,
              const u8 *payload, u64 payload_bytes)
{
	std::string shape_str = "(";
	for (size_t i = 0; i < shape.size(); ++i) {
		if (i > 0)
			shape_str += ", ";
		shape_str += std::to_string(shape[i]);
	}
	if (shape.size() == 1)
		shape_str += ",";
	shape_str += ")";

	std::string header_dict = "{'descr': '" + descr + "', 'fortran_order': False, 'shape': " + shape_str + ", }";
	// header is: 6 magic + 2 version + 2 header_size + dict + padding ' ' + '\n'
	// total divisible by 64, dict is padded with 0x20 except final \n
	const size_t fixed = 6 + 2 + 2;
	size_t total = fixed + header_dict.size() + 1; // +1 for the trailing \n
	size_t padded_total = ((total + 63) / 64) * 64;
	size_t pad = padded_total - total;
	std::string padded_dict = header_dict + std::string(pad, ' ') + "\n";
	if ((fixed + padded_dict.size()) % 64 != 0) {
		std::cerr << "internal error: header padding wrong\n";
		return false;
	}
	u16 header_len = static_cast<u16>(padded_dict.size());

	std::ofstream f(path, std::ios::binary);
	if (!f)
		return false;

	const u8 magic[6] = {0x93, 'N', 'U', 'M', 'P', 'Y'};
	f.write(reinterpret_cast<const char*>(magic), 6);

	const u8 version[2] = {0x01, 0x00};
	f.write(reinterpret_cast<const char*>(version), 2);

	u8 hl[2] = {static_cast<u8>(header_len & 0xff), static_cast<u8>((header_len >> 8) & 0xff)};
	f.write(reinterpret_cast<const char*>(hl), 2);
	f.write(padded_dict.data(), padded_dict.size());
	f.write(reinterpret_cast<const char*>(payload), payload_bytes);

	return f.good();
}


/*
 * generate the large fixtures if absent. We don't regenerate every run -
 * a 1 GiB write would dwarf the actual benchmark.
 */
static bool
ensure_fixtures(const fs::path &dir)
{
	std::error_code ec;
	fs::create_directories(dir, ec);

	const fs::path big_f32 = dir / "big_f32_1g.npy";
	const fs::path big_f64 = dir / "big_f64_512m.npy";
	const fs::path small_u64 = dir / "small_u64_256m.npy"; // 32 Mi items
	const fs::path npz_many = dir / "many_arrays.npz";

	// big_f32: 256 Mi f32 = 1 GiB
	if (!fs::exists(big_f32)) {
		std::cout << "[gen] " << big_f32.string() << " (1 GiB) ...\n";
		const u64 N = 256ull * 1024 * 1024;
		auto buf = std::make_unique<f32[]>(N);
		std::mt19937 rng(0xc0ffee);
		std::uniform_real_distribution<float> d(-1.0f, 1.0f);

		for (u64 i = 0; i < N; ++i)
			buf[i] = d(rng);

		if (!write_npy_raw(big_f32, "<f4", {N}, reinterpret_cast<u8*>(buf.get()), N * sizeof(f32))) {
			std::cerr << "  failed\n";
			return false;
		}
	}

	// big_f64: 64 Mi f64 = 512 MiB; we'll byteswap-bench this
	if (!fs::exists(big_f64)) {
		std::cout << "[gen] " << big_f64.string() << " (512 MiB) ...\n";
		const u64 N = 64ull * 1024 * 1024;
		auto buf = std::make_unique<f64[]>(N);
		std::mt19937 rng(0xdeadbeef);
		std::uniform_real_distribution<double> d(-1.0, 1.0);

		for (u64 i = 0; i < N; ++i)
			buf[i] = d(rng);

		if (!write_npy_raw(big_f64, "<f8", {N}, reinterpret_cast<u8*>(buf.get()), N * sizeof(f64))) {
			std::cerr << "  failed\n";
			return false;
		}
	}

	// small_u64: 32 Mi u64 = 256 MiB; sized for the per-item callback bench
	if (!fs::exists(small_u64)) {
		std::cout << "[gen] " << small_u64.string() << " (256 MiB) ...\n";
		const u64 N = 32ull * 1024 * 1024;
		auto buf = std::make_unique<u64[]>(N);

		for (u64 i = 0; i < N; ++i)
			buf[i] = i;

		if (!write_npy_raw(small_u64, "<u8", {N}, reinterpret_cast<u8*>(buf.get()), N * sizeof(u64))) {
			std::cerr << "  failed\n";
			return false;
		}
	}

	// many_arrays.npz: write 100 small arrays in an uncompressed zip
	if (!fs::exists(npz_many)) {
		std::cout << "[gen] " << npz_many.string() << " (100 small arrays) ...\n";
		std::vector<numpy::ndarray> arrs;
		arrs.reserve(100);
		for (int i = 0; i < 100; ++i) {
			numpy::ndarray a({1024}, numpy::dtype_float32());
			arrs.push_back(std::move(a));
		}
		std::vector<numpy::savez_arg> args;
		for (size_t i = 0; i < arrs.size(); ++i)
			args.push_back({"arr_" + std::to_string(i), arrs[i]});
		auto res = numpy::savez(npz_many, std::move(args), true);

		// arrays still own their data here; release them to free
		for (auto &a : arrs)
			numpy::release(a);

		if (!res.is_ok()) {
			std::cerr << "  savez failed: " << numpy::to_string(res) << "\n";
			return false;
		}
	}
	return true;
}


/*
 * drop_caches - hint the kernel to drop file caches between runs so we
 * actually measure cold-load timings. Best-effort - requires root and
 * /proc/sys/vm/drop_caches. If it fails the timings will be warm-cache,
 * which is still a useful relative measurement. Run with sudo if cold
 * numbers are desired.
 */
static void
drop_page_cache()
{
#if defined(__linux__)
	std::ofstream f("/proc/sys/vm/drop_caches");
	if (f) f << "3\n";
#endif
}


/*
 * cold_cache_supported - returns true iff drop_page_cache() can actually
 * evict pages on this run. Used purely for the banner so the reader knows
 * whether the (a) and (c) timings are cold-cache or warm-cache.
 */
static bool
cold_cache_supported()
{
#if defined(__linux__)
	if (geteuid() != 0)
		return false;
	std::ofstream f("/proc/sys/vm/drop_caches");
	return f.good();
#else
	return false;
#endif
}


/*
 * cow_prepass - touch one byte in every 4 KiB page with a write the
 * compiler cannot elide. On a MAP_PRIVATE mmap this triggers copy-on-write
 * for every page up front, so the apply variants that follow all see the
 * same warm, private-page baseline. On a heap-backed buffer this is just
 * a cheap streaming pass that warms the cache.
 *
 * The volatile lvalue forces the store to be issued; the read on the rhs
 * is then live (its result feeds the volatile store) and also survives
 * -O3. Without volatile, gcc rewrites `p[i] = p[i]` (or `p[i] ^= 0`) to a
 * no-op and no faults are taken - which silently invalidates the bswap
 * benchmark.
 */
static u64
cow_prepass(u8 *p, size_t n)
{
	constexpr size_t page = 4096;
	for (size_t i = 0; i < n; i += page)
		*reinterpret_cast<volatile u8*>(p + i) = p[i];
	return n;
}


static void
bench_load_big_f32(const fs::path &path)
{
	const u64 file_bytes = fs::file_size(path);
	std::cout << "(a) load " << path.filename().string()
		<< " (" << (file_bytes / (1024.0 * 1024.0)) << " MiB)\n";

	// vector path: drive from_npy_ifstream directly so the row is the
	// vector path regardless of build flags.
	{
		drop_page_cache();
		auto t0 = clk::now();
		std::ifstream f;
		auto res = numpy::open_npy(path, f);
		if (!res.is_ok()) {
			std::cout << "  open failed\n";
			return;
		}

		numpy::ndarray arr;
		res = numpy::from_npy_ifstream(f, arr);
		auto t1 = clk::now();
		double s = std::chrono::duration<double>(t1 - t0).count();
		print_timing("from_npy_ifstream (vector path)", s, file_bytes);
		// touch one element to be honest about read latency
		volatile f32 v = arr.value<f32>(0);
		(void)v;
		numpy::release(arr);
	}

	// mmap path: this is what from_npy() picks by default for files above
	// NCR_NUMPY_MMAP_THRESHOLD_BYTES. When NCR_NUMPY_NO_MMAP_LOAD is
	// defined (or the build has no mmap support), from_npy() collapses to
	// the vector path - measuring it here would just duplicate the row
	// above with a misleading label, so we skip and say so explicitly.
#if defined(NCR_NUMPY_HAS_MMAP) && !defined(NCR_NUMPY_NO_MMAP_LOAD)
	{
		drop_page_cache();
		auto t0 = clk::now();
		numpy::ndarray arr;
		auto res = numpy::from_npy(path, arr);
		auto t1 = clk::now();
		double s = std::chrono::duration<double>(t1 - t0).count();
		print_timing("from_npy (mmap, no touch)", s, file_bytes);
		if (!res.is_ok()) {
			std::cout << "  load failed\n";
			return;
		}

		// also measure touching every page. mmap defers the I/O cost from
		// load to first access; this row reveals it. On a heap-backed
		// buffer the same loop runs on already-resident memory and is
		// essentially free, which is why we don't print it in that case.
		auto t2 = clk::now();
		const u8 *p = arr.data();
		size_t n = arr.bytesize();
		volatile u64 sum = 0;
		for (size_t i = 0; i < n; i += 4096)
			sum += p[i];

		auto t3 = clk::now();
		double s2 = std::chrono::duration<double>(t3 - t2).count();
		print_timing("from_npy (mmap, touch all pages)", s2, n);
		(void)sum;
		numpy::release(arr);
	}
#else
	std::cout << "  (mmap path skipped: NCR_NUMPY_NO_MMAP_LOAD is set,\n"
		<< "   so from_npy() collapses to from_npy_ifstream in this build.)\n";
#endif
}


static void
bench_bswap(const fs::path &path)
{
	const u64 file_bytes = fs::file_size(path);
	std::cout << "(b) bswap apply<f64> on " << path.filename().string()
		<< " (" << (file_bytes / (1024.0 * 1024.0)) << " MiB payload)\n";

	// load the array via from_npy (mmap by default; vector path when
	// NCR_NUMPY_NO_MMAP_LOAD is set).
	numpy::ndarray arr;
	auto res = numpy::from_npy(path, arr);
	if (!res.is_ok()) {
		std::cout << "  load failed: " << numpy::to_string(res) << "\n";
		return;
	}
	const size_t payload = arr.bytesize();

	// COW prepass: write-touch every page so MAP_PRIVATE COW fires up
	// front and all three apply variants below are measured against the
	// same warm, private-page baseline. We time it explicitly because on
	// an mmap'd buffer this is the dominant cost of the first mutating
	// pass, and folding it silently into apply<f64> (as the previous
	// version of this bench did) makes apply<f64> look 5-7x slower than
	// it is. On a heap-backed buffer the prepass is just a fast cache
	// warm-up; the row times near zero and there is no COW to attribute.
	{
		u8 *p = const_cast<u8*>(arr.data());
		auto t0 = clk::now();
		u64 touched = cow_prepass(p, arr.bytesize());
		auto t1 = clk::now();
		double s = std::chrono::duration<double>(t1 - t0).count();
		print_timing("COW prepass (one byte per 4 KiB)", s, touched);
	}

	// 1) typed apply<T> path (already optimal-ish, auto-vectorisable)
	{
		auto t0 = clk::now();
		arr.apply<f64>([](f64 v) { return bswap<f64>(v); });
		auto t1 = clk::now();
		double s = std::chrono::duration<double>(t1 - t0).count();
		print_timing("apply<f64>(bswap<f64>)", s, payload);
	}

	// 2) untyped in-place apply (new API)
	{
		auto t0 = clk::now();
		arr.apply([](u8_span span) {
			f64 v;
			std::memcpy(&v, span.data(), sizeof(f64));
			v = bswap<f64>(v);
			std::memcpy(span.data(), &v, sizeof(f64));
		});
		auto t1 = clk::now();
		double s = std::chrono::duration<double>(t1 - t0).count();
		print_timing("apply(span -> void)  in-place", s, payload);
	}

	// 3) untyped allocating apply (legacy contract, kept for compat)
	{
		auto t0 = clk::now();
		arr.apply([](u8_const_span span) {
			u8_vector r(sizeof(f64));
			f64 v;
			std::memcpy(&v, span.data(), sizeof(f64));
			v = bswap<f64>(v);
			std::memcpy(r.data(), &v, sizeof(f64));
			return r;
		});
		auto t1 = clk::now();
		double s = std::chrono::duration<double>(t1 - t0).count();
		print_timing("apply(span -> u8_vector) legacy", s, payload);
	}

	numpy::release(arr);
}


static void
bench_callbacks(const fs::path &path)
{
	const u64 file_bytes = fs::file_size(path);
	std::cout << "(c) per-item callbacks over " << path.filename().string()
		<< " (" << (file_bytes / (1024.0 * 1024.0)) << " MiB, u64)\n"
		<< "    note: from_npy_callback always uses chunked ifstream reads;\n"
		<< "    NCR_NUMPY_NO_MMAP_LOAD has no effect on this section.\n";

	// 1) typed flat callback (chunked-read fast path)
	{
		drop_page_cache();
		auto t0 = clk::now();
		volatile u64 sum = 0;
		auto res = numpy::from_npy<u64>(path, [&](u64, u64 v) {
			sum = sum + v;
			return true;
		});
		auto t1 = clk::now();
		(void)sum; (void)res;
		double s = std::chrono::duration<double>(t1 - t0).count();
		print_timing("typed flat callback", s, file_bytes);
	}

	// 2) span-based generic callback (new API, no per-item alloc)
	{
		drop_page_cache();
		auto t0 = clk::now();
		volatile u64 sum = 0;
		auto res = numpy::from_npy(path, [&](const numpy::dtype&, const u64_vector&, const storage_order&, u64, u8_const_span item) {
			u64 v;
			std::memcpy(&v, item.data(), sizeof(u64));
			sum = sum + v;
			return true;
		});
		auto t1 = clk::now();
		(void)sum; (void)res;
		double s = std::chrono::duration<double>(t1 - t0).count();
		print_timing("generic span callback", s, file_bytes);
	}

	// 3) generic vector callback (legacy: one heap alloc per item)
	{
		drop_page_cache();
		auto t0 = clk::now();
		volatile u64 sum = 0;
		auto res = numpy::from_npy(path, [&](const numpy::dtype&, const u64_vector&, const storage_order&, u64, u8_vector item) {
			u64 v;
			std::memcpy(&v, item.data(), sizeof(u64));
			sum = sum + v;
			return true;
		});
		auto t1 = clk::now();
		(void)sum; (void)res;
		double s = std::chrono::duration<double>(t1 - t0).count();
		print_timing("generic vector callback (legacy)", s, file_bytes);
	}
}


static void
bench_npz(const fs::path &path)
{
	const u64 file_bytes = fs::file_size(path);
	std::cout << "(d) load .npz " << path.filename().string()
		<< " (" << file_bytes << " bytes)\n";
	{
		drop_page_cache();
		auto t0 = clk::now();
		numpy::npzfile npz;
		auto res = numpy::loadz(path, npz);
		auto t1 = clk::now();
		double s = std::chrono::duration<double>(t1 - t0).count();
		print_timing(std::string("loadz, ") + std::to_string(npz.names.size()) + " arrays", s, file_bytes);
		if (!res.is_ok())
			std::cout << "  load result: " << numpy::to_string(res) << "\n";
		numpy::release(npz);
	}
}


int
main(int argc, char **argv)
{
	const fs::path dir = (argc > 1) ? fs::path(argv[1]) : fs::path("assets");
	if (!ensure_fixtures(dir)) {
		std::cerr << "fixture generation failed\n";
		return 1;
	}

	std::cout << "\n=== ncr_numpy benchmarks ===\n";
	std::cout << "fixture dir:           " << fs::absolute(dir).string() << "\n";
#if defined(NCR_NUMPY_HAS_MMAP)
	std::cout << "mmap support:          yes\n";
#else
	std::cout << "mmap support:          no\n";
#endif
#if defined(NCR_NUMPY_NO_MMAP_LOAD)
	std::cout << "NCR_NUMPY_NO_MMAP_LOAD: defined  (from_npy() forced to vector path)\n";
#else
	std::cout << "NCR_NUMPY_NO_MMAP_LOAD: undefined (from_npy() picks mmap when available)\n";
#endif
	std::cout << "cold-cache mode:       "
		<< (cold_cache_supported() ? "active (drop_caches works)"
		                           : "inactive (need root for /proc/sys/vm/drop_caches; warm-cache numbers)")
		<< "\n\n";

	bench_load_big_f32(dir / "big_f32_1g.npy");
	std::cout << "\n";
	bench_bswap(dir / "big_f64_512m.npy");
	std::cout << "\n";
	bench_callbacks(dir / "small_u64_256m.npy");
	std::cout << "\n";
	bench_npz(dir / "many_arrays.npz");

	return 0;
}
