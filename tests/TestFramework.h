// Minimal dependency-free test harness (see docs/DECISIONS.md D-003).
#pragma once
#include <atomic>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace fmtest
{
struct TestCase { const char* group; const char* name; std::function<void()> fn; };
inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }
struct Registrar { Registrar (const char* g, const char* n, std::function<void()> f) { registry().push_back ({ g, n, std::move (f) }); } };
inline void report (bool ok, const char* expr, const char* file, int line, const std::string& extra = {})
{
    ++checks();
    if (! ok) { ++failures(); std::printf ("    FAIL %s:%d  %s %s\n", file, line, expr, extra.c_str()); }
}
}

namespace fmtest
{
extern std::atomic<int> g_allocations;
extern thread_local bool g_trackAllocations;
// Counts operator-new calls made on this thread while alive.
struct AllocationScope
{
    AllocationScope() { g_allocations.store (0); g_trackAllocations = true; }
    ~AllocationScope() { g_trackAllocations = false; }
    int count() const { return g_allocations.load(); }
};
}

#define FM_CAT2(a, b) a##b
#define FM_CAT(a, b) FM_CAT2 (a, b)
#define FM_TEST(group, name) \
    static void FM_CAT (fmtest_fn_, __LINE__)(); \
    static fmtest::Registrar FM_CAT (fmtest_reg_, __LINE__) (group, name, FM_CAT (fmtest_fn_, __LINE__)); \
    static void FM_CAT (fmtest_fn_, __LINE__)()
#define CHECK(expr) fmtest::report (static_cast<bool> (expr), #expr, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, tol) \
    do { const double fm_a = (a), fm_b = (b); \
         fmtest::report (std::fabs (fm_a - fm_b) <= (tol), #a " ~= " #b, __FILE__, __LINE__, \
                         "(" + std::to_string (fm_a) + " vs " + std::to_string (fm_b) + ")"); } while (false)
