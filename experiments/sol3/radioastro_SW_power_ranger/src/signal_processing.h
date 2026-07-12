#ifndef SIGNAL_PROCESSING_H
#define SIGNAL_PROCESSING_H

#include <math.h> // Use M_PI for the value of pi

#include "utils.h"
#include "fxp_utils.h"

// #define NStages 10 // Nstages = log2(NFFT)
// #define MAX_NSTAGES 20 // the maximum number of stages
// #define MAX_NFFT (1 << MAX_NSTAGES) // the maximum number of FFT points
// #define PEAKS_MAX 1000 // the maximum number of peaks to find
#define SNR_LIMIT 70 // the minimum SNR to consider
constexpr int LOG2_NFFT = 17;
constexpr int NFFT = 1 << LOG2_NFFT;
constexpr int GAUSS_WINDOW_SIZE = 2304;
constexpr int MEDIAN_WINDOW = 200;
constexpr int PEAK_DETECTION_WINDOW = 200;
constexpr int PEAKS_MAX = 10000; // Adjust as needed

void moving_average(float* a, int a_len, int window, float* filt_a) ;
int find_peaks(float* signal, int signal_len, float height, float prominence, int peak_window, float* filtered_peaks);
unsigned int reverse_bits(unsigned int input, int num_stages);
void bit_reverse(std::complex<float>* X, int nfft, int num_stages, std::complex<float>* OUT);
void fft_stage(int stage, std::complex<float>* X, int nfft, std::complex<float>* Out);
void fft(std::complex<float>* In, int log2_nfft, std::complex<float>* Out);
float custom_hanning_window(float* win, int N);
float hanning_window(float* win, int N);
void window(std::complex<float>* In, int N, float* kernel, std::complex<float>* Out);
void add_reduction_4(float* In_R, float* In_I, int N);
void welch_psd(std::complex<float>* samples, int Nseg, float Fs, float Fc, float* freqs, int log2_nfft, float* spectrum, TTimes & times, CXADCProxy * powerRanger, TEnergies & energies);
void normalization(float* spectrum, float Fs, int size, float* kernel);
void gauss_window(float* window, int size);
void gauss_smoothing(float* data, int data_len, int window_size, float* smooth_data);
//void moving_median(float* data, int data_len, int* index, int index_len, int window, float* filtered_spike);
void moving_median(const float* data, int data_len, const int* index, int index_len, int window, float* filtered_spike);
int compare (const void * a, const void * b);
void peak_smoothing(float* data, int data_len, int avg_window, float* filtered_data);

#endif
