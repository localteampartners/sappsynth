#pragma once
#include <complex>
#include <vector>

namespace sappsynth {

// Offline analysis for tests and Lab experiments. Not realtime code.
namespace analyzer {

// In-place iterative radix-2 FFT. size must be a power of two.
void fft(std::vector<std::complex<double>>& data);

// Hann-windowed magnitude spectrum (linear), bins 0..fftSize/2-1.
std::vector<double> magnitudeSpectrum(const float* signal, int numSamples, int fftSize);

// Peak frequency via parabolic interpolation around the largest bin.
double peakFrequencyHz(const std::vector<double>& spectrum, double sampleRate);

// Ratio (dB) of energy *outside* harmonic neighborhoods of f0 to energy at
// the harmonics. More negative = cleaner. The alias-energy regression metric.
double aliasEnergyDb(const std::vector<double>& spectrum, double sampleRate,
                     double fundamentalHz, int maxHarmonics = 64,
                     int neighborhoodBins = 3);

double rms(const float* signal, int numSamples);
double peak(const float* signal, int numSamples);

} // namespace analyzer
} // namespace sappsynth
