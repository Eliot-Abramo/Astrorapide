#ifndef UTILS_H
#define UTILS_H

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <complex>
#include <stdint.h>
#include <time.h>

struct InParameters {
    std::string signal_file;
    std::string out_file;
    std::string profile_file;
    int config_id = -1;
    double peaks_prom = 0.0;
    double peaks_height = 0.875;
    float freq_res = 0;
    float vel_res = 0;
};

struct DatasetParam {
    double fc;
    double rate;
    int32_t num_samples;
    int32_t channels;
    int32_t gain;
    int32_t intTime;
};

struct TTimes {
    uint64_t timePSD = 0;
    uint64_t timeMA = 0;
    uint64_t timePS = 0;
    uint64_t timeGF = 0;
    uint64_t timeCal = 0;
    uint64_t timeDetectPeaks = 0;
    uint64_t timeVelocity = 0;
    uint64_t timeTotal = 0;    
    uint64_t timeHannGen = 0;
    uint64_t timeHannWin = 0;
    uint64_t timeRed = 0;
    uint64_t timeFFT = 0;
    uint64_t timeMag = 0;
    uint64_t timeNorm = 0;
    uint64_t timeReord = 0;
    uint64_t timeFreqGen = 0;
};

struct TEnergies {
    double energyPSD = 0.0;
    double energyMA = 0.0;
    double energyPS = 0.0;
    double energyGF = 0.0;
    double energyCal = 0.0;
    double energyDetectPeaks = 0.0;
    double energyVelocity = 0.0;
    double energyTotal = 0.0;    
    double energyHannGen = 0.0;
    double energyHannWin = 0.0;
    double energyRed = 0.0;
    double energyFFT = 0.0;
    double energyMag = 0.0;
    double energyNorm = 0.0;
    double energyReord = 0.0;
    double energyFreqGen = 0.0;
};

struct configurations_t {
    int log2_nfft;
    int ma_w;
    int ps_w;
    int gf_w;
    int pd_w;
    std::string gain_file;
    std::string ref_file;
};

InParameters load_in_param(int argc, char* argv[]);
uint64_t CalcTimeDiff(const struct timespec & time2, const struct timespec & time1);
void PrintTimes(TTimes & times);
void PrintEnergies(TEnergies & energies);
void PrintConfigs(InParameters params, configurations_t config);
void save_metrics(const std::string& filename, int config_id, configurations_t config, TTimes times, TEnergies energies, float snr, float freq_res, float velocity_res);

template <typename T>
void load_bin_signal(const std::string& bin_file_name, DatasetParam&  Param, std::complex<T> **data){
    std::ifstream bin_file;
    bin_file.open(bin_file_name, std::ios::in | std::ios::binary);
    if (!bin_file.is_open()){
        std::cerr << "Error: could not open file " << bin_file_name << std::endl;
        exit(1);
    }
    // Load parameters
    bin_file.read(reinterpret_cast<char*>(&Param.fc), sizeof(double));
    bin_file.read(reinterpret_cast<char*>(&Param.rate), sizeof(double));
    bin_file.read(reinterpret_cast<char*>(&Param.num_samples), sizeof(int32_t));
    bin_file.read(reinterpret_cast<char*>(&Param.channels), sizeof(int32_t));
    bin_file.read(reinterpret_cast<char*>(&Param.gain), sizeof(int32_t));
    bin_file.read(reinterpret_cast<char*>(&Param.intTime), sizeof(int32_t));

    *data = (std::complex<T>*)malloc(Param.num_samples*sizeof(std::complex<T>));
    // Load samples
    bin_file.read(reinterpret_cast<char*>(*data), Param.num_samples * sizeof(std::complex<T>));
    bin_file.close();
}

template <typename T>
void write_res_bin(const std::string& filename, T * amp, T * freq, int64_t size, T * peak_h, T * peak_f, int64_t peaks_len){
    std::ofstream file(filename, std::ios::binary);

    if (file.is_open()) {
        // Metadata
        file.write(reinterpret_cast<const char*>(&size), sizeof(int64_t));
        file.write(reinterpret_cast<const char*>(&peaks_len), sizeof(int64_t));
        // Data
        file.write(reinterpret_cast<const char*>(amp), sizeof(T) * size);
        file.write(reinterpret_cast<const char*>(freq), sizeof(T) * size);
        file.write(reinterpret_cast<const char*>(peak_h), sizeof(T) * peaks_len);
        file.write(reinterpret_cast<const char*>(peak_f), sizeof(T) * peaks_len);
        file.close();
    }
}

template <typename T>
int64_t read_res_bin(const std::string& filename, T **amp){
    std::ifstream file(filename, std::ios::binary);
    int64_t size;
    int64_t peaks_len;
  
    if (file.is_open()) {
        // Metadata
        file.read(reinterpret_cast<char*>(&size), sizeof(int64_t));
        file.read(reinterpret_cast<char*>(&peaks_len), sizeof(int64_t));
        // Data
        *amp = (T*)malloc(sizeof(T) * size);
        file.read(reinterpret_cast<char*>(*amp), sizeof(T) * size);
        file.close();
    }else {
        std::cerr << "Unable to open file" << std::endl;
    }
  
    return size;
}

template <typename T1, typename T2>
double SNR(T1* exact, T2* target, size_t size) {
    double magnitude_sum = 0;
    double noise_sum = 0;
    for (size_t i = 0; i < size; i++) {
        double magnitude = std::pow(std::abs(exact[i]), 2);
        double diff = exact[i] - target[i];
        double noise = std::pow(std::abs(diff), 2);
        magnitude_sum += magnitude;
        noise_sum += noise;
    }

    double snr = 10 * std::log10(magnitude_sum / noise_sum);
    return snr;
}

#endif
