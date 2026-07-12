#include <stdint.h> 
#include <stdio.h>
#include <iostream>
#include <cmath>
#include <map>
#include <cstring>
#include "CAccelProxy.hpp"
#include "CXADCProxy.hpp"
#include "CFFTDriver.hpp"
#include <cstdlib>   // for posix_memalign
#include <omp.h>     // for OpenMP pragmas (compile with -fopenmp)
#include <algorithm>
#include <set>
#include <complex>
#include <cassert>
#include <queue>
#include <vector>
#include "signal_processing.h"

// Function to check if a point is a local maximum
bool is_local_max(const float* signal, int idx, int len, int window) {
    float val = signal[idx];
    for (int i = std::max(0, idx - window); i <= std::min(len - 1, idx + window); ++i) {
        if (i != idx && signal[i] >= val) {
            return false;
        }
    }
    return true;
}

// Function to detect peaks
int find_peaks(float* signal, int signal_len, float height, float prominence, int peak_window, float* filtered_peaks) {
    
    float peaks[PEAKS_MAX];
    int peaks_len = 0;

    // Detect candidate peaks
    for (int i = 0; i < signal_len; ++i) {
        if (peaks_len == PEAKS_MAX) break;
        if (signal[i] >= height && is_local_max(signal, i, signal_len, peak_window)) {
            peaks[peaks_len++] = i;
        }
    }
    
    // Check if the peak is prominent
    int filtered_peaks_len = 0;
    for (int i = 0; i < peaks_len; i++) {
        int peak_idx = peaks[i];
        float left_min = signal[peak_idx], right_min = signal[peak_idx];
        
        // Find left valley
        for (int j = peak_idx - 1; j >= 0; --j) {
            if (signal[j] < left_min) left_min = signal[j];
            else break;  // Stop at first rising edge
        }
        
        // Find right valley
        for (int j = peak_idx + 1; j < signal_len; ++j) {
            if (signal[j] < right_min) right_min = signal[j];
            else break;  // Stop at first rising edge
        }
        
        float peak_prominence = signal[peak_idx] - std::max(left_min, right_min);
        if (peak_prominence >= prominence) {
            filtered_peaks[filtered_peaks_len] = peak_idx;
            filtered_peaks_len++;
        }
    }

    return filtered_peaks_len;
}

float custom_hanning_window(float* win, int N){
    if (N <= 0) return 0.0f; 
    float location = -M_PI * 4 / 2.0f;
    float norm_factor = 0.0f;
    for (int i = 0; i < N; i++){
        location += M_PI / N;
        win[i] = sin(location) / (location);
        win[i] *= sin(M_PI*i/(N-1))*sin(M_PI*i/(N-1));
        norm_factor += win[i]* win[i];
    }
    return norm_factor;
}     

float hanning_window(float* win, int N){
    float norm_factor = 0;
    for (int i = 0; i < N; i++){
        win[i] = sin(M_PI*i/(N-1))*sin(M_PI*i/(N-1));
        norm_factor += win[i]* win[i];
    }
    return norm_factor;
}

void window(std::complex<float>* In, int N, float* kernel, std::complex<float>* Out){
    #pragma omp simd
    for (int i = 0; i < N; i++){
        Out[i] = In[i] * kernel[i];
    }
}

void add_reduction_4(std::complex<float>* In, int N){
    #pragma omp simd
    for (int i = 0; i < N; i++){
        In[i] += In[i+N] + In[i+2*N] + In[i+3*N];
    }
}

// Scale the spectrum by the norm of the window to compensate for windowing loss
void normalization(float* spectrum, float Fs, int size, float norm_factor){
    #pragma omp simd
    for (int i = 0; i < size; i++){
        spectrum[i] = spectrum[i]/(norm_factor*Fs);
    }
}

void welch_psd(std::complex<float>* samples, int Nseg, float Fs, float Fc, float* freqs, int log2_nfft, float* spectrum, TTimes & times, CXADCProxy * powerRanger, TEnergies & energies, 
    CFFT * fft_accel, std::complex<TFXP> * batch_in, std::complex<TFXP> * batch_out, int chunk_size){
    
    struct timespec start_segment{}, end_segment{};
    double energy_start_segment{}, energy_end_segment{};

    const int nfft = 1 << log2_nfft; // NFFT = 2^NStages
    float norm_factor = 0.0f;
    
    std::vector<float> hann_win(4*nfft);

    #pragma omp parallel for simd
    for (int i = 0; i < nfft; ++i){
        spectrum[i] = 0.0f;
    }

    energy_start_segment = powerRanger->GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    norm_factor = custom_hanning_window(hann_win.data(), 4 * nfft);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger->GetEnergy();
    times.timeHannGen = CalcTimeDiff(end_segment, start_segment);
    energies.energyHannGen = energy_end_segment - energy_start_segment;
    
    times.timeHannWin = times.timeRed = times.timeFFT = times.timeMag = 0;

    auto f2fxp = [](float v) -> TFXP { return Float2Fxp(v); };
    auto fxp2f = [](TFXP v) -> float { return Fxp2Float(v); };

    #pragma omp parallel for collapse(2)
    for (int c = 0; c < chunk_size; c++) {
        for (int k = 0; k < nfft; k++) {
            const std::complex<float>& s = samples[c * nfft + k];
            batch_in[c * nfft + k] = { f2fxp(s.real()), f2fxp(s.imag()) };
        }
    }

    energy_start_segment = powerRanger->GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    fft_accel->FFT_HW(batch_in, batch_out, chunk_size);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger->GetEnergy();
    times.timeFFT += CalcTimeDiff(end_segment, start_segment);
    energies.energyFFT += energy_end_segment - energy_start_segment;
    
    energy_start_segment = powerRanger->GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    #pragma omp parallel
    {
        std::vector<float> local_spectrum(nfft, 0.0f);

        #pragma omp for
        for (int c = 0; c < (chunk_size - 3); ++c) {
            const std::complex<TFXP>* out = batch_out + c * nfft;
            for (int k = 0; k < nfft; ++k) {
                float re = fxp2f(out[k].real());
                float im = fxp2f(out[k].imag());
                local_spectrum[k] += (re * re + im * im) / Nseg;
            }
        }

        // Combine local spectra into the shared spectrum
        #pragma omp critical
        for (int k = 0; k < nfft; ++k) {
            spectrum[k] += local_spectrum[k];
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger->GetEnergy();
    times.timeMag += CalcTimeDiff(end_segment, start_segment); 
    energies.energyMag += energy_end_segment - energy_start_segment;

    for (int i = (chunk_size-3); i < Nseg; i += (chunk_size-3)) {
        const int current_chunk = std::min(chunk_size, Nseg - i);
        #pragma omp parallel for collapse(2)
        for (int c = 0; c < current_chunk; ++c) {
            for (int k = 0; k < nfft; ++k) {
                const std::complex<float>& s = samples[(i + c) * nfft + k];
                batch_in[c * nfft + k] = { f2fxp(s.real()), f2fxp(s.imag()) };
            }
        }
                    
        energy_start_segment = powerRanger->GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
        fft_accel->FFT_HW(batch_in, batch_out, current_chunk);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
        energy_end_segment = powerRanger->GetEnergy();
        times.timeFFT += CalcTimeDiff(end_segment, start_segment);
        energies.energyFFT += energy_end_segment - energy_start_segment;
        
        energy_start_segment = powerRanger->GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);

        #pragma omp parallel
        {
            std::vector<float> local_spectrum(nfft, 0.0f);

            #pragma omp for
            for (int c = 0; c < (current_chunk - 3); ++c) {
            const std::complex<TFXP>* out = batch_out + c * nfft;
                for (int k = 0; k < nfft; ++k) {
                    float re = fxp2f(out[k].real());
                    float im = fxp2f(out[k].imag());
                    local_spectrum[k] += (re * re + im * im) / Nseg;
                }
            }

            #pragma omp critical
            for (int k = 0; k < nfft; ++k) {
                spectrum[k] += local_spectrum[k];
            }
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
        energy_end_segment = powerRanger->GetEnergy();
        times.timeMag += CalcTimeDiff(end_segment, start_segment);
        energies.energyMag += energy_end_segment - energy_start_segment;
    }

    // Normalize the power spectrum density
    energy_start_segment = powerRanger->GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    normalization(spectrum, Fs, nfft, norm_factor);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger->GetEnergy();
    times.timeNorm = CalcTimeDiff(end_segment, start_segment);
    energies.energyNorm = energy_end_segment - energy_start_segment;

    // Translate spectrum around the central frequency
    energy_start_segment = powerRanger->GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    #pragma omp simd
    for (int i = nfft / 2; i < nfft; i++) {
        std::swap(spectrum[i], spectrum[i - nfft / 2]);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger->GetEnergy();
    times.timeReord = CalcTimeDiff(end_segment, start_segment);
    energies.energyReord = energy_end_segment - energy_start_segment;

    // Generate frequencies
    energy_start_segment = powerRanger->GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    #pragma omp simd
    for (int i = 0; i < nfft; i++){
        freqs[i] = i * Fs / nfft + Fc - Fs / 2;
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger->GetEnergy();
    times.timeFreqGen = CalcTimeDiff(end_segment, start_segment);
    energies.energyFreqGen = energy_end_segment - energy_start_segment;
}


void moving_average(float* a, int a_len, int window, float* filt_a) {
    if (window <= 0 || a_len <= 0 || filt_a == nullptr) {
        std::cerr << "Error: Invalid input parameters in moving_average!" << std::endl;
        return;
    }

    float sum = 0.0f;
    for (int i = 0; i < window; ++i) {
        sum += a[i];
    }

    int a_idx = 0;
    filt_a[a_idx] = sum / window;

    for (int i = window; i < a_len; ++i) {
        sum += a[i] - a[i - window];
        a_idx++;
        filt_a[a_idx] = sum / window;
    }
}

void peak_smoothing(float* data, int data_len, int avg_window, float* filtered_data){    
    double mean = 0.0;
    
    #pragma omp parallel for reduction(+:mean)
    for (int i = 0; i < data_len; ++i) mean += data[i];
    mean /= data_len;
    const float threshold = 1.20f * static_cast<float>(mean);
    std::vector<int> idx_peaks;
    idx_peaks.reserve(data_len / 8);

    #pragma omp parallel
    {
        std::vector<int> local_idx;
        local_idx.reserve(256);

        #pragma omp for nowait
        for (int i = avg_window; i < data_len - avg_window; ++i)
            if (data[i] > threshold) local_idx.push_back(i);

        #pragma omp critical
        idx_peaks.insert(idx_peaks.end(), local_idx.begin(), local_idx.end());
    }
    std::sort(idx_peaks.begin(), idx_peaks.end());
    
    moving_median(data, data_len,
                  idx_peaks.data(), static_cast<int>(idx_peaks.size()),
                  avg_window, filtered_data);
}

void moving_median(const float* data, int data_len,
                   const int* index, int index_len, int window,
                   float* filtered_spike)
{
    const int window_size = 2 * window + 1;
    std::copy(data, data + data_len, filtered_spike);

    std::vector<float> window_vals(window_size);

    for (int n = 0; n < index_len; ++n) {
        int center = index[n];
        int start  = center - window;
        int end    = center + window;

        if (start < 0){
            start = 0;
            end = std::min(data_len - 1, start + window_size - 1); 
        }
        
        if (end >= data_len) { 
            end = data_len - 1; 
            start = std::max(0, end - window_size + 1); 
        }

        int j = 0;
        for (int i = start; i <= end; ++i, ++j) window_vals[j] = data[i];

        std::nth_element(window_vals.begin(),
                         window_vals.begin() + j / 2,
                         window_vals.begin() + j);
        filtered_spike[center] = window_vals[j / 2];
    }
}

void gauss_window(float* window, int size) {
    int fwhm_cal = int(size / 4);
    float normal_factor_cal = 1 / sqrt(2 * M_PI * fwhm_cal * fwhm_cal);
    float* gx_cal = (float*)malloc((2 * size + 1) * sizeof(float));
    for (int i = 0; i <= 2 * size; i++) {
        gx_cal[i] = i - size;
    }
    float win_sum = 0;
    for (int i = 0; i <= 2 * size; i++) {
        window[i] = normal_factor_cal * exp(-gx_cal[i] * gx_cal[i] / (2 * fwhm_cal * fwhm_cal));
        win_sum += window[i];
    }
    for (int i = 0; i <= 2 * size; i++) {
        window[i] = window[i] / win_sum;
    }
    free(gx_cal);
}

void gauss_smoothing(float* data, int data_len, int window_size, float* smooth_data) {
    float* window = (float*)malloc((2 * window_size + 1) * sizeof(float));
    gauss_window(window, window_size);
    #pragma omp parallel for
    for (int i = 0; i < data_len; ++i) {
        float acc = 0.0f;
        for (int j = 0; j <= 2 * window_size; ++j) {
            int data_idx = i + j - window_size;
            if (data_idx >= 0 && data_idx < data_len) {
                acc += data[data_idx] * window[j];
            }
        }
        smooth_data[i] = acc;
    }

    free(window);
}
