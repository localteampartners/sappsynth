#include "SignalAnalyzer.h"
#include <cmath>
#include <algorithm>

namespace sappsynth {
namespace analyzer {

namespace {
constexpr double pi = 3.14159265358979323846;
}

void fft(std::vector<std::complex<double>>& data)
{
    const std::size_t n = data.size();
    if (n < 2)
        return;

    // Bit-reversal permutation.
    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }

    for (std::size_t len = 2; len <= n; len <<= 1)
    {
        const double angle = -2.0 * pi / static_cast<double>(len);
        const std::complex<double> wLen(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t k = 0; k < len / 2; ++k)
            {
                const auto u = data[i + k];
                const auto v = data[i + k + len / 2] * w;
                data[i + k] = u + v;
                data[i + k + len / 2] = u - v;
                w *= wLen;
            }
        }
    }
}

std::vector<double> magnitudeSpectrum(const float* signal, int numSamples, int fftSize)
{
    std::vector<std::complex<double>> buffer(static_cast<std::size_t>(fftSize));
    for (int i = 0; i < fftSize; ++i)
    {
        const double sample = i < numSamples ? static_cast<double>(signal[i]) : 0.0;
        const double window = 0.5 * (1.0 - std::cos(2.0 * pi * i / (fftSize - 1)));
        buffer[static_cast<std::size_t>(i)] = sample * window;
    }
    fft(buffer);

    std::vector<double> magnitudes(static_cast<std::size_t>(fftSize / 2));
    for (int i = 0; i < fftSize / 2; ++i)
        magnitudes[static_cast<std::size_t>(i)] = std::abs(buffer[static_cast<std::size_t>(i)]) / (fftSize / 2);
    return magnitudes;
}

double peakFrequencyHz(const std::vector<double>& spectrum, double sampleRate)
{
    if (spectrum.size() < 3)
        return 0.0;

    std::size_t peakBin = 1;
    for (std::size_t i = 1; i + 1 < spectrum.size(); ++i)
        if (spectrum[i] > spectrum[peakBin])
            peakBin = i;

    // Parabolic interpolation on log magnitudes.
    const double a = std::log(std::max(spectrum[peakBin - 1], 1e-30));
    const double b = std::log(std::max(spectrum[peakBin], 1e-30));
    const double c = std::log(std::max(spectrum[peakBin + 1], 1e-30));
    const double denom = a - 2.0 * b + c;
    const double delta = std::abs(denom) > 1e-12 ? 0.5 * (a - c) / denom : 0.0;

    const double binWidth = sampleRate / (2.0 * static_cast<double>(spectrum.size()));
    return (static_cast<double>(peakBin) + delta) * binWidth;
}

double aliasEnergyDb(const std::vector<double>& spectrum, double sampleRate,
                     double fundamentalHz, int maxHarmonics, int neighborhoodBins)
{
    const double binWidth = sampleRate / (2.0 * static_cast<double>(spectrum.size()));
    std::vector<bool> isHarmonic(spectrum.size(), false);

    for (int h = 1; h <= maxHarmonics; ++h)
    {
        const double freq = fundamentalHz * h;
        if (freq >= sampleRate / 2.0)
            break;
        const int centerBin = static_cast<int>(freq / binWidth + 0.5);
        for (int b = centerBin - neighborhoodBins; b <= centerBin + neighborhoodBins; ++b)
            if (b >= 0 && b < static_cast<int>(spectrum.size()))
                isHarmonic[static_cast<std::size_t>(b)] = true;
    }

    // Skip the DC/window-leakage region below half the fundamental.
    const int startBin = std::max(2, static_cast<int>(fundamentalHz / (2.0 * binWidth)));
    double harmonicEnergy = 0.0;
    double residualEnergy = 0.0;
    for (std::size_t i = static_cast<std::size_t>(startBin); i < spectrum.size(); ++i)
    {
        const double e = spectrum[i] * spectrum[i];
        if (isHarmonic[i])
            harmonicEnergy += e;
        else
            residualEnergy += e;
    }

    if (harmonicEnergy <= 0.0)
        return 0.0;
    return 10.0 * std::log10(std::max(residualEnergy, 1e-30) / harmonicEnergy);
}

double rms(const float* signal, int numSamples)
{
    double sum = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sum += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
    return std::sqrt(sum / std::max(numSamples, 1));
}

double peak(const float* signal, int numSamples)
{
    double p = 0.0;
    for (int i = 0; i < numSamples; ++i)
        p = std::max(p, std::abs(static_cast<double>(signal[i])));
    return p;
}

} // namespace analyzer
} // namespace sappsynth
