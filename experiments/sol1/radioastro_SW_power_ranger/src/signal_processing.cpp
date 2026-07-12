#include <stdint.h> 
#include <stdio.h>
#include <iostream>
#include <cmath>
#include <map>

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
    
    float* peaks = (float*)malloc(PEAKS_MAX*sizeof(float));
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

    free(peaks);
    return filtered_peaks_len;
}

unsigned int reverse_bits(unsigned int input, int num_stages) {
	int i, rev = 0;
	for (i = 0; i < num_stages; i++) {
		rev = (rev << 1) | (input & 1);
		input = input >> 1;
	}
	return rev;
}

void bit_reverse(std::complex<float>* X, int nfft, int num_stages, std::complex<float>* OUT) {
    int reversed;
    std::complex<float> temp;
  
    for (int i = 0; i < nfft; i++) {
	    reversed = reverse_bits(i, num_stages); // Find the bit reversed index
		if (i <= reversed) {
			// Swap the real values
			temp = X[i];
			OUT[i] = X[reversed];
			OUT[reversed] = temp;
		}
	}
}

void fft_stage(int stage, std::complex<float>* X, int nfft, std::complex<float>* Out) {
    int DFTpts = 1 << stage;    // DFT = 2^stage = points in sub DFT
    int numBF = DFTpts / 2;     // Butterfly WIDTHS in sub-DFT
    float e = -2 * M_PI / DFTpts;
    float a = 0.0;
    // Perform butterflies for j-th stage
    for (int j = 0; j < numBF; j++) {
        // Can be computed once as a look-up table (for the last stage)
        float c = cos(a);
        float s = sin(a);
        std::complex<float> twiddle = std::complex<float>(c, s);
        a = a + e;
        // Compute butterflies that use same W**k
        for (int i = j; i < nfft; i += DFTpts) {
            int i_lower = i + numBF; // index of lower point in butterfly
            std::complex<float> temp = X[i_lower] * twiddle;
            Out[i_lower] = X[i] - temp;
            Out[i] = X[i] + temp;
        }
    }
}

std::complex<float> Stage[MAX_NSTAGES][MAX_NFFT];
void fft(std::complex<float>* In, int log2_nfft, std::complex<float>* Out) {
    
    int nstages = log2_nfft;
    int nfft = 1 << nstages; // NFFT = 2^NStages 

    bit_reverse(In, nfft, nstages, Stage[0]);
    for (int stage = 1; stage < nstages; stage++) { // Do M-1 stages of butterflies
        fft_stage(stage, Stage[stage-1], nfft, Stage[stage]);
    }
    fft_stage(nstages, Stage[nstages-1], nfft, Out);
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
        In[i] = In[i] + In[i+N] + In[i+2*N] + In[i+3*N];
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
    for (int i = 0; i < nfft; i++){
        spectrum[i] = 0;
    }

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
        fft(samples_buffer, log2_nfft, coeff);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
        energy_end_segment = powerRanger->GetEnergy();
        times.timeFFT += CalcTimeDiff(end_segment, start_segment);
        energies.energyFFT += energy_end_segment - energy_start_segment;

        // Integrate ~10 seconds of the signal
        energy_start_segment = powerRanger->GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
        for (int k = 0; k < nfft; k++){
            spectrum[k] += (coeff[k].real()*coeff[k].real() + coeff[k].imag()*coeff[k].imag())/Nseg;
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
        float temp_spectrum = spectrum[i];
        spectrum[i] = spectrum[i-nfft/2];
        spectrum[i-nfft/2] = temp_spectrum;
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
