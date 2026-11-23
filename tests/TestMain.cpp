#include "TestFramework.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <new>

// ---- allocation tracking for real-time safety tests ---------------------------------------
namespace fmtest
{
std::atomic<int> g_allocations { 0 };
thread_local bool g_trackAllocations = false;
}
void* operator new (std::size_t n)
{
    if (fmtest::g_trackAllocations) fmtest::g_allocations.fetch_add (1);
    if (void* p = std::malloc (n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n) { return operator new (n); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }


int main (int argc, char** argv)
{
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    for (auto& t : fmtest::registry())
    {
        if (filter != nullptr && std::strcmp (filter, t.group) != 0 && std::strstr (t.name, filter) == nullptr)
            continue;
        const int before = fmtest::failures();
        std::printf ("[%s] %s\n", t.group, t.name);
        std::fflush (stdout);
        t.fn();
        if (fmtest::failures() != before) std::printf ("  -> FAILED\n");
        ++run;
    }
    std::printf ("\n%d test cases, %d checks, %d failures\n", run, fmtest::checks(), fmtest::failures());
    if (run == 0) { std::printf ("no tests matched\n"); return 2; }
    return fmtest::failures() == 0 ? 0 : 1;
}
