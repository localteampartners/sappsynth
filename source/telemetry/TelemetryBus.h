#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace sappsynth {

// Lock-free audio -> UI sample tap (architecture §21, high-rate scope
// channel). Single producer (audio thread) writes the post-master mono mix;
// single consumer (UI timer) copies the latest window. Writes are plain
// stores into a ring guarded by an atomic position — the UI may read a torn
// window during a wrap, which for an oscilloscope is harmless. Audio thread
// never blocks, never allocates; when disabled the cost is one atomic load.
class TelemetryBus
{
public:
    static constexpr int kSize = 1 << 14; // ~340 ms at 48 kHz
    static constexpr int kMask = kSize - 1;

    void setEnabled(bool shouldCapture) noexcept { enabled.store(shouldCapture, std::memory_order_relaxed); }
    bool isEnabled() const noexcept { return enabled.load(std::memory_order_relaxed); }

    void push(const float* left, const float* right, int n) noexcept
    {
        if (!isEnabled())
            return;
        std::uint64_t w = writePos.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            ring[static_cast<std::size_t>(w & kMask)] = 0.5f * (left[i] + right[i]);
            ++w;
        }
        writePos.store(w, std::memory_order_release);
    }

    // Copies the most recent `n` samples (n <= kSize) into dest, oldest first.
    // Returns how many were valid.
    int readLatest(float* dest, int n) const noexcept
    {
        const std::uint64_t w = writePos.load(std::memory_order_acquire);
        const int available = static_cast<int>(w < static_cast<std::uint64_t>(n) ? w : static_cast<std::uint64_t>(n));
        const std::uint64_t start = w - static_cast<std::uint64_t>(available);
        for (int i = 0; i < available; ++i)
            dest[i] = ring[static_cast<std::size_t>((start + static_cast<std::uint64_t>(i)) & kMask)];
        return available;
    }

private:
    std::array<float, kSize> ring {};
    std::atomic<std::uint64_t> writePos { 0 };
    std::atomic<bool> enabled { false };
};

} // namespace sappsynth
