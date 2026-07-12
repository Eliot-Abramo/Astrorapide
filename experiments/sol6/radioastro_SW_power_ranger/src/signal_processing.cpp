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

const uint32_t MAP_SIZE = 64*1024; // Size of address range mapped to the adder registers
const uint32_t FFT_HW_ADDR = 0x40000000;

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
    // free(peaks);
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
        In[i] = In[i] + In[i+N] + In[i+2*N] + In[i+3*N];
    }
}

// Scale the spectrum by the norm of the window to compensate for windowing loss
void normalization(float* spectrum, float Fs, int size, float norm_factor){
    #pragma omp simd
    for (int i = 0; i < size; i++){
        spectrum[i] = spectrum[i]/(norm_factor*Fs);
    }
}

bool InitDevice(CFFT & fft, std::complex<float>* &sample_buffer, std::complex<float>* &coeff, int nfft, int chunk_size, bool log)
{
    if (log) {
        printf("\n\nThis program requires that the bitstream is loaded in the FPGA.\n");
        printf("Press ENTER to confirm that the bitstream is loaded (proceeding without it can crash the board).\n\n");
        getchar(); // Commented to allow execution in batch
    }

    if (fft.Open(FFT_HW_ADDR, MAP_SIZE) != CAccelProxy::OK ) {
        printf("Error mapping fft device at physical address 0x%08X\n", FFT_HW_ADDR);
        return false;
    }
    if (log)
        printf("Device at physical fft address 0x%08X successfully mapped into the application virtual address space\n\n",
            FFT_HW_ADDR);

    if (log)
        printf("Allocating DMA memory...\n");
    sample_buffer = (std::complex<float>*)fft.AllocDMACompatible(chunk_size * nfft * sizeof(std::complex<float>), 1);
    coeff = (std::complex<float>*)fft.AllocDMACompatible(chunk_size * nfft * sizeof(std::complex<float>), 1);
    
    if (log)
    {
        printf("DMA memory allocated.\n");
        printf("Input: Virtual address: 0x%08X (%u)\n", (uint32_t)sample_buffer , (uint32_t)sample_buffer );
        printf("Output: Virtual address: 0x%08X (%u)\n", (uint32_t)coeff , (uint32_t)coeff);

    }

    if ((sample_buffer == NULL) || (coeff  == NULL))
    {
        if (log)
            printf("Error allocating DMA memory.\n");
        return false;
    }


    return true;
}

void welch_psd(std::complex<float>* samples, int Nseg, float Fs, float Fc, float* freqs, int log2_nfft, float* spectrum, TTimes & times, CXADCProxy * powerRanger, TEnergies & energies){
    struct timespec start_segment, end_segment;
    double energy_start_segment, energy_end_segment;

    CFFT fft_accel(false);

    int nfft = 1 << log2_nfft; // NFFT = 2^NStages
    const int chunk_size = 1024;

    float norm_factor = 0.0f;
    float* hann_win = nullptr;
    std::complex<float>* samples_buffer = nullptr;
    std::complex<float>* batch_in = nullptr;
    std::complex<float>* batch_out = nullptr;

    // Aligned + cache-coherent allocations for input and output buffers
    if (posix_memalign((void**)&hann_win, 64, 4 * nfft * sizeof(float)) != 0) {
        printf("Failed to allocate aligned hann_win\n");
        return;
    }
    if (posix_memalign((void**)&samples_buffer, 64, 4 * nfft * sizeof(std::complex<float>)) != 0) {
        printf("Failed to allocate aligned samples_buffer\n");
        free(hann_win);
        return;
    }

    if (!InitDevice(fft_accel, batch_in, batch_out, nfft, chunk_size, false)) {
        printf("InitDevice failed\n");
        free(hann_win);
        free(samples_buffer);
        return;
    }

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
    // for (int i = 0; i < Nseg-3; i+=1){ // Slide 1 block
    for (int i = 0; i < Nseg - 3; i += chunk_size) {
        int current_chunk = std::min(chunk_size, Nseg - 3 - i);

        for (int c = 0; c < current_chunk; c++) {
            // Apply hanning wondow to 4 blocks
            energy_start_segment = powerRanger->GetEnergy();
            clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
            window(samples + (i + c) * nfft, 4 * nfft, hann_win, samples_buffer);
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

            memcpy(batch_in + c * nfft, samples_buffer, nfft * sizeof(std::complex<float>));
        }

        // FFT
        energy_start_segment = powerRanger->GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
        // fft(samples_buffer, log2_nfft, coeff);
        fft_accel.FFT_HW(batch_in, batch_out, current_chunk);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
        energy_end_segment = powerRanger->GetEnergy();
        times.timeFFT += CalcTimeDiff(end_segment, start_segment);
        energies.energyFFT += energy_end_segment - energy_start_segment;

        // Integrate ~10 seconds of the signal
        energy_start_segment = powerRanger->GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
        for (int c = 0; c < current_chunk; c++) {
            const std::complex<float>* out = batch_out + c * nfft;
            #pragma omp parallel for

            for (int k = 0; k < nfft; k++){
                spectrum[k] += (out[k].real()*out[k].real() + out[k].imag()*out[k].imag())/Nseg;
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
    #pragma omp simd
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
    fft_accel.FreeDMACompatible(batch_in);
    fft_accel.FreeDMACompatible(batch_out);
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

    std::vector<float> window_vals(window_size);   // allocated once

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

    // Use SIMD to accelerate the smoothing process
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
