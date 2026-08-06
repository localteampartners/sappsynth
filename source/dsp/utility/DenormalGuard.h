#pragma once
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
  #include <xmmintrin.h>
  #include <pmmintrin.h>
#endif

namespace sappsynth {

// RAII: enable flush-to-zero (and denormals-are-zero where available) for the
// enclosing scope — typically the whole audio callback — and restore on exit.
class DenormalGuard
{
public:
    DenormalGuard() noexcept
    {
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
        previousCsr = _mm_getcsr();
        _mm_setcsr(previousCsr | 0x8040); // FTZ | DAZ
#elif defined(__aarch64__)
        asm volatile("mrs %0, fpcr" : "=r"(previousFpcr));
        asm volatile("msr fpcr, %0" ::"r"(previousFpcr | (1ull << 24))); // FZ
#endif
    }

    ~DenormalGuard() noexcept
    {
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
        _mm_setcsr(previousCsr);
#elif defined(__aarch64__)
        asm volatile("msr fpcr, %0" ::"r"(previousFpcr));
#endif
    }

    DenormalGuard(const DenormalGuard&) = delete;
    DenormalGuard& operator=(const DenormalGuard&) = delete;

private:
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
    std::uint32_t previousCsr { 0 };
#elif defined(__aarch64__)
    std::uint64_t previousFpcr { 0 };
#endif
};

} // namespace sappsynth
