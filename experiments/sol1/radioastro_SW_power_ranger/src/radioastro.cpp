// main.cpp
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <locale.h>
#include <unistd.h>
#include <pthread.h>
#include <map>

#include "utils.h"
#include "CAccelProxy.hpp"
#include "CXADCProxy.hpp"
#include "signal_processing.h"

///////////////////////////////////////
// Address constants
const uint32_t MAP_SIZE = 64*1024; // Size of address range mapped to the adder registers
const uint32_t XADC_HW_ADDR = 0x43C00000; // From Vivado's address editor

// Configuration parameters
#define NUM_CONFIGS 10
configurations_t config_settings[NUM_CONFIGS] = {
    {10,    12,    12,      12,     3       , "../data_bin/gain_0.bin", "../data_bin/ref_0.bin" },  // config_id = 0
    {11,    24,    24,      24,     10      , "../data_bin/gain_1.bin", "../data_bin/ref_1.bin" },  // config_id = 1
    {12,    48,    48,      48,     10      , "../data_bin/gain_2.bin", "../data_bin/ref_2.bin" },  // config_id = 2
    {13,    96,    96,      96,     10      , "../data_bin/gain_3.bin", "../data_bin/ref_3.bin" },  // config_id = 3
    {14,    192,   192,     192,    200     , "../data_bin/gain_4.bin", "../data_bin/ref_4.bin" },  // config_id = 4
    {15,    384,   384,     384,    10      , "../data_bin/gain_5.bin", "../data_bin/ref_5.bin" },  // config_id = 5
    {16,    768,   768,     768,    10      , "../data_bin/gain_6.bin", "../data_bin/ref_6.bin" },  // config_id = 6
    {17,    2304,  2304,    2304,   200     , "../data_bin/gain_7.bin", "../data_bin/ref_7.bin" },  // config_id = 7
    {18,    3072,  3072,    3072,   200     , "../data_bin/gain_8.bin", "../data_bin/ref_8.bin" },  // config_id = 8
    {19,    6144,  6144,    6144,   200     , "../data_bin/gain_9.bin", "../data_bin/ref_9.bin" }   // config_id = 9
};


///////////////////////////////////////////////////////////////////////////////
bool InitDevice(CXADCProxy & xadc, bool log=false)
{
    if (log) {
        printf("\n\nThis program requires that the bitstream is loaded in the FPGA.\n");
        printf("Press ENTER to confirm that the bitstream is loaded (proceeding without it can crash the board).\n\n");
        getchar(); // Commented to allow execution in batch
    }

    if ( xadc.Open(XADC_HW_ADDR, MAP_SIZE) != CAccelProxy::OK ) {
        printf("Error mapping device at physical address 0x%08X\n", XADC_HW_ADDR);
        return false;
    }
    if (log)
        printf("Device at physical address 0x%08X successfully mapped into the application virtual address space\n\n",
            XADC_HW_ADDR);

    return true;
}


///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[]) {
    /*----------------------
    Processing pipeline
    ----------------------*/ 
    TTimes times;
    TEnergies energies;
    struct timespec start_program, end_program;
    struct timespec start_segment, end_segment;
    double energy_start_program, energy_end_program;
    double energy_start_segment, energy_end_segment;
    int peaks_len = 0;
    
    // Verify that we are running with sudo.
    if (geteuid()) {
      printf("\n\n###### This program has to be run with sudo!!! ######\n\n\n");
      return -1;
    }
    CXADCProxy powerRanger(false); // Deactivate logging
    if (!InitDevice(powerRanger))
      return -1;

    // Load program parameters
    InParameters params = load_in_param(argc, argv);

    // Select configuration
    configurations_t config = config_settings[params.config_id];
    
    // Constants
    float sample_rate = 2.048e6; // From the antenna 2.048 MHz
    float c = 3e8; // Speed of light in m/s
    float hydro_freq = 1420.40575177e6; // Hydrogen line rest frequency in Hz
    int nfft = 1 << config.log2_nfft; // NFFT = 2^NStages
    params.freq_res = sample_rate / nfft; // Frequency resolution
    params.vel_res = c * params.freq_res / hydro_freq; // Velocity resolution
    int len_avg = nfft - (config.ma_w-1);
    
    PrintConfigs(params, config);
    
    // Memory allocation
    std::complex<float> *signal = nullptr;
    float *freqs = (float*)malloc(nfft*sizeof(float));
    float *freqs_ma = (float*)malloc(len_avg*sizeof(float));
    float *psd = (float*)malloc(nfft*sizeof(float));
    float *psd_ma = (float*)malloc(len_avg*sizeof(float));
    float *psd_ps = (float*)malloc(len_avg*sizeof(float));
    float *psd_gf = (float*)malloc(len_avg*sizeof(float));
    float* gain = nullptr;
    float *psd_norm = (float*)malloc(nfft*sizeof(float));
    float* peaks_idx = (float*)malloc(PEAKS_MAX*sizeof(float));
    float *velocities = nullptr;
    float *peak_freq = nullptr;
    float *peak_height = nullptr;
    float *save = psd_gf;
    
    // Load input data
    // Sky data
    DatasetParam signal_param;
    load_bin_signal(params.signal_file, signal_param, &signal);
    
    // Load Gain data
    read_res_bin(config.gain_file, &gain);
    
    // 3) Compute power spectral density using Weltch's method
    int num_samples = (int)signal_param.num_samples;
    if (num_samples % nfft != 0) {
        std::cerr << "Error: num_samples must be divisible by NFFT without a remainder." << std::endl;
        return -1;
    }
    int Nseg = num_samples / nfft;

    /*---------------------*\
        Start processing
    \*---------------------*/ 

    powerRanger.StartMeasurements();
    energy_start_program = powerRanger.GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_program);

    // 1) Sky spectrum
    energy_start_segment = powerRanger.GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    welch_psd(signal, Nseg, sample_rate, (int)signal_param.fc, freqs, config.log2_nfft, psd, times, &powerRanger, energies);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger.GetEnergy();
    times.timePSD = CalcTimeDiff(end_segment, start_segment);
    energies.energyPSD = energy_end_segment - energy_start_segment;

    // 2) Moving average
    energy_start_segment = powerRanger.GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    moving_average(psd, nfft, config.ma_w, psd_ma);
    for (int i = 0; i < len_avg; i++){
        freqs_ma[i] = freqs[(int)(i + config.ma_w/2)];
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger.GetEnergy();
    times.timeMA = CalcTimeDiff(end_segment, start_segment);
    energies.energyMA = energy_end_segment - energy_start_segment;

    // 3) Peak Smoothing
    energy_start_segment = powerRanger.GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    peak_smoothing(psd_ma, len_avg, config.ps_w, psd_ps);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger.GetEnergy();
    times.timePS = CalcTimeDiff(end_segment, start_segment);
    energies.energyPS = energy_end_segment - energy_start_segment;

    // 4) Gaussian filter
    energy_start_segment = powerRanger.GetEnergy();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
    gauss_smoothing(psd_ps, len_avg, config.gf_w, psd_gf);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
    energy_end_segment = powerRanger.GetEnergy();
    times.timeGF = CalcTimeDiff(end_segment, start_segment);
    energies.energyGF = energy_end_segment - energy_start_segment;

    if (gain != nullptr){
        // 5) Calibration
        energy_start_segment = powerRanger.GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);    
        for (int i = 0; i < len_avg; i++){
            psd_norm[i] = psd_gf[i]/gain[i];
        }
        save = psd_norm;
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
        energy_end_segment = powerRanger.GetEnergy();
        times.timeCal = CalcTimeDiff(end_segment, start_segment);
        energies.energyCal = energy_end_segment - energy_start_segment;
        
        // 6) Detect peaks
        peaks_len = 0;
        energy_start_segment = powerRanger.GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);
        peaks_len = find_peaks(psd_norm, len_avg, params.peaks_height, params.peaks_prom, config.pd_w, peaks_idx);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
        energy_end_segment = powerRanger.GetEnergy();
        times.timeDetectPeaks = CalcTimeDiff(end_segment, start_segment);
        energies.energyDetectPeaks = energy_end_segment - energy_start_segment;
    
        // 7) Compute velocity shift due to Doppler effect
        velocities = (float*)malloc(peaks_len*sizeof(float));
        peak_freq = (float*)malloc(peaks_len*sizeof(float));
        peak_height = (float*)malloc(peaks_len*sizeof(float));
        energy_start_segment = powerRanger.GetEnergy();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_segment);  
        for(int i = 0; i < peaks_len; i++){
            int peak = peaks_idx[i];
            peak_freq[i] = freqs_ma[peak];
            peak_height[i] = psd_norm[peak];
            velocities[i] = c * (peak_freq[i] - hydro_freq) / hydro_freq;
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_segment);
        energy_end_segment = powerRanger.GetEnergy();
        times.timeVelocity = CalcTimeDiff(end_segment, start_segment);
        energies.energyVelocity = energy_end_segment - energy_start_segment;
    
    }
    
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_program);
    energy_end_program = powerRanger.GetEnergy();
    times.timeTotal = CalcTimeDiff(end_program, start_program);
    energies.energyTotal = energy_end_program - energy_start_program;
    powerRanger.StopMeasurements();

    /*---------------------*\
        End processing
    \*---------------------*/ 

    // Print results
    for (int i = 0; i < peaks_len; i++){
        std::cout << "Arm " << i << ": Peak Frequency = " << peak_freq[i] << "Hz, Velocity = " << velocities[i]/1000 << " km/s"<< std::endl;
    }

    // Save output data
    if (!params.out_file.empty()){
        write_res_bin(params.out_file, save, freqs_ma, (int64_t)len_avg, peak_height, peak_freq, (int64_t)peaks_len);
    }

    // Reference data
    float snr = -1;
    float* reference;
    read_res_bin(config.ref_file, &reference);
    snr = SNR(reference, save, len_avg);
    std::cout << "SNR: " << snr << " dB" << std::endl;
    
    // Deallocate dynamically allocated memory
    free(signal);
    free(freqs);
    free(psd);
    free(gain);
    free(psd_norm);
    free(freqs_ma);
    free(psd_ma);
    free(psd_ps);
    free(psd_gf);
    free(peaks_idx);
    free(peak_freq);
    free(peak_height);
    free(velocities);
    
    
    // Save metrics to CSV
    if (!params.profile_file.empty()){
        save_metrics(params.profile_file, params.config_id, config, times, energies, snr, params.freq_res, params.vel_res);
    }
    
    // Print profiling info
    PrintTimes(times); 
    PrintEnergies(energies);
    
    // Check SNR
    if (std::isfinite(snr) && snr < SNR_LIMIT){
        std::cout << "******************************************" << std::endl;
        std::cout << "Warning: SNR is lower than " << SNR_LIMIT << " dB!!" << std::endl;
        std::cout << "******************************************" << std::endl;
        return -1;
    }else{
        return 0;
    }
}
