#include <stdint.h> 
#include <stdio.h>
#include <iostream>
#include <cmath>
#include <map>
#include <cassert>
#include <algorithm>
#include <cstring>   

#include "CAccelProxy.hpp"
#include "CXADCProxy.hpp"
#include "signal_processing.h"

// The length of the output vector is len(a)-(mavavg_points-1)
void moving_average(float* a, int a_len, int window, float* filt_a) {
    float sum = 0.0;

    // Compute the initial window sum
    for (int i = 0; i < window; ++i) {
        sum += a[i];
    }
    int a_idx = 0;
    filt_a[a_idx] = sum/window;

    // Compute moving average using a sliding window
    for (int i = window; i < a_len; ++i) {
        sum += a[i] - a[i - window];  // Update sum by adding new element and removing old one
        a_idx++;
        filt_a[a_idx] = sum / window;
    }
}

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

class TwiddleCache {
    std::vector<std::vector<std::complex<float>>> twiddles;
    int max_log2;

public:
    TwiddleCache(int max_log2) : max_log2(max_log2), twiddles(max_log2 + 1) {}

    const std::vector<std::complex<float>>& get_twiddles(int stage, bool inverse) {
        if (!twiddles[stage].empty()) return twiddles[stage];

        int len = 1 << stage;
        int half = len >> 1;
        twiddles[stage].resize(half);

        const float PI = 3.14159265358979323846f;
        int sign = inverse ? 1 : -1;
        for (int k = 0; k < half; ++k) {
            float angle = sign * 2.0f * PI * k / len;
            twiddles[stage][k] = std::complex<float>(std::cos(angle), std::sin(angle));
        }
        return twiddles[stage];
    }
};

static void bit_reversal_permute(std::complex<float>* data, int n) {
    int j = 0;
    for (int i = 1; i < n; i++) {
        int bit = n >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j |= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }
}

void fft(std::complex<float>* data, int log2_nfft, bool inverse) {
    int n = 1 << log2_nfft;
    assert((n & (n - 1)) == 0);

    static TwiddleCache tw_cache(20);

    bit_reversal_permute(data, n);

    for (int stage = 1; stage <= log2_nfft; ++stage) {
        int len = 1 << stage;
        int half = len >> 1;
        const auto& twiddles = tw_cache.get_twiddles(stage, inverse);

        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; j++) {
                auto u = data[i + j];
                auto v = data[i + j + half] * twiddles[j];
                data[i + j] = u + v;
                data[i + j + half] = u - v;
            }
        }
    }

    if (inverse) {
        for (int i = 0; i < n; i++) {
            data[i] /= float(n);
        }
    }
}
float custom_hanning_window(float* win, int N){
    float location = -M_PI * 4 / 2.0;
    float norm_factor = 0;
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
    for (int i = 0; i < N; i++){
        Out[i] = In[i] * kernel[i];
    }
}

void add_reduction_4(std::complex<float>* In, int N){
    for (int i = 0; i < N; i++){
        In[i] += In[i + N] + In[i + 2 * N] + In[i + 3 * N];
    }
}

// Scale the spectrum by the norm of the window to compensate for windowing loss
void normalization(float* spectrum, float Fs, int size, float norm_factor){
    for (int i = 0; i < size; i++){
        spectrum[i] = spectrum[i]/(norm_factor*Fs);
    }
}

void welch_psd(std::complex<float>* samples, int Nseg, float Fs, float Fc, float* freqs, int log2_nfft, float* spectrum, TTimes & times, CXADCProxy * powerRanger, TEnergies & energies){
    struct timespec start_segment, end_segment;
    double energy_start_segment, energy_end_segment;

    int nfft = 1 << log2_nfft; // NFFT = 2^NStages
    float norm_factor = 0;
    float* hann_win = (float*)malloc(4 * nfft * sizeof(float));
    std::complex<float>* samples_buffer = (std::complex<float>*)malloc(4 * nfft * sizeof(std::complex<float>));
    std::complex<float>* coeff = (std::complex<float>*)malloc(nfft * sizeof(std::complex<float>));

    // initailize the spectrum
    std::fill(spectrum, spectrum + nfft, 0.0f);

    // Compute the Hanning window
    energy_start_segment = powerRanger->GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    norm_factor = custom_hanning_window(hann_win, 4 * nfft);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger->GetEnergy();
    times.timeHannGen = CalcTimeDiff(end_segment, start_segment);
    energies.energyHannGen = energy_end_segment - energy_start_segment;
    
    // window buffer
    times.timeHannWin = 0;
    times.timeRed = 0;
    times.timeFFT = 0;
    times.timeMag = 0;
    for (int i = 0; i < Nseg-3; i+=1){ // Slide 1 block

        // Apply hanning wondow to 4 blocks
        energy_start_segment = powerRanger->GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
        window(samples+i*nfft, 4 * nfft, hann_win, samples_buffer);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
        energy_end_segment = powerRanger->GetEnergy();
        times.timeHannWin += CalcTimeDiff(end_segment, start_segment);
        energies.energyHannWin += energy_end_segment - energy_start_segment;

        // Add the 4 blocks
        energy_start_segment = powerRanger->GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
        add_reduction_4(samples_buffer, nfft);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
        energy_end_segment = powerRanger->GetEnergy();
        times.timeRed += CalcTimeDiff(end_segment, start_segment);
        energies.energyRed += energy_end_segment - energy_start_segment;

        // FFT
        energy_start_segment = powerRanger->GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
        fft(samples_buffer, log2_nfft, false);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
        energy_end_segment = powerRanger->GetEnergy();
        times.timeFFT += CalcTimeDiff(end_segment, start_segment);
        energies.energyFFT += energy_end_segment - energy_start_segment;

        // Integrate ~10 seconds of the signal
        energy_start_segment = powerRanger->GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
        for (int k = 0; k < nfft; k++){
            spectrum[k] += (samples_buffer[k].real() * samples_buffer[k].real() + samples_buffer[k].imag() * samples_buffer[k].imag()) / Nseg;
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
    for (int i = nfft/2; i < nfft; i++){
        std::swap(spectrum[i], spectrum[i - nfft / 2]);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger->GetEnergy();
    times.timeReord = CalcTimeDiff(end_segment, start_segment);
    energies.energyReord = energy_end_segment - energy_start_segment;

    // Generate frequencies
    energy_start_segment = powerRanger->GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    for (int i = 0; i < nfft; i++){
        freqs[i] = i * Fs / nfft + Fc - Fs / 2;
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger->GetEnergy();
    times.timeFreqGen = CalcTimeDiff(end_segment, start_segment);
    energies.energyFreqGen = energy_end_segment - energy_start_segment;

    // Free allocated memory
    free(hann_win);
    free(samples_buffer);
    free(coeff);
}

void gauss_window(float* window, int size){
    int fwhm_cal = int(size/4);
    float normal_factor_cal = 1/sqrt(2*M_PI*fwhm_cal*fwhm_cal);
    float *gx_cal = (float*)malloc((2*size + 1)*sizeof(float));
    for (int i = 0; i <= 2*size; i++){
        gx_cal[i] = i - size;
    }
    float win_sum = 0;
    for (int i = 0; i <= 2*size; i++){
        window[i] = normal_factor_cal*exp(-gx_cal[i]*gx_cal[i]/(2*fwhm_cal*fwhm_cal));
        win_sum += window[i];
    }
    for (int i = 0; i <= 2*size; i++){
        window[i] = window[i]/win_sum;
    }
    free(gx_cal);
}

void gauss_smoothing(float* data, int data_len, int window_size, float* smooth_data){
    float *window = (float*)malloc((2*window_size + 1)*sizeof(float));
    gauss_window(window,window_size);
    for (int i = 0; i < data_len; ++i) {
        float acc = 0.0;
        for (int j = 0; j <= 2*window_size; ++j) {
            int data_idx = i + j - window_size;
            if (data_idx >= 0 && data_idx < data_len) {
                acc += data[data_idx] * window[j];
            }
        }
        smooth_data[i] = acc;
    }
    free(window);
}

int compare (const void * a, const void * b)
{
  return ( *(float*)a < *(float*)b );
}

void moving_median(float* data, int data_len, int* index, int index_len, int window, float* filtered_spike){
    float* med_window = (float*)malloc((2*window+1)*sizeof(float));
    for (int i = 0; i < data_len; i++){
        filtered_spike[i] = data[i];
    }
    for (int i = 0; i < index_len; i++){
        int idx = index[i];
        for (int j = -window; j <= window; j++){
            med_window[j+window] = data[idx + j];
        }
        qsort(med_window, 2*window+1, sizeof(float), compare);
        filtered_spike[idx] = med_window[window];
    }    
    free(med_window);
}

void peak_smoothing(float* data, int data_len, int avg_window, float* filtered_data){
    float mean = 0;
    for (int i = 0; i < data_len; i++){
        mean += data[i];
    }
    mean /= data_len;
    float threshold = 1.20 * mean;
    int* idx_peaks = (int*)malloc(data_len*sizeof(int));
    int idx_peaks_len = 0; 
    for (int i = avg_window; i < data_len - avg_window; i++){
        if (data[i] > threshold){
            idx_peaks[idx_peaks_len] = i;
            idx_peaks_len++;
        }
    }

    moving_median(data, data_len, idx_peaks, idx_peaks_len, avg_window, filtered_data);

    free(idx_peaks);
}
