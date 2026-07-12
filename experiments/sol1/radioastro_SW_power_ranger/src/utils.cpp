#include "utils.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>
#include <ctime>

//namespace plt = matplotlibcpp;

InParameters load_in_param(int argc, char* argv[]) {
    InParameters params;
    int arg_idx = 1;
    while (arg_idx < argc){
        std::string arg = argv[arg_idx];
        if((arg == "-s") || (arg == "--signal")){
            params.signal_file = argv[++arg_idx];
        } else if((arg == "-c") || (arg == "--config")){
            params.config_id = std::stoi(argv[++arg_idx]);
        } else if((arg == "-p") || (arg == "--profile")){
            params.profile_file = argv[++arg_idx];
        } else if((arg == "-o") || (arg == "--output")){
            params.out_file = argv[++arg_idx];
        }
        else{
            std::cerr << "Invalid argument: " << arg << std::endl;
            std::cerr << "Usage: ./radioastro -s <signal_file> -c <config_id> [-o <output_file> -p <profile_file>]" << std::endl;
            exit(-1);
        }
        arg_idx++;
    }
    if (params.signal_file.empty() || params.config_id < 0){
        std::cerr << "Error: no singal file was provided." << std::endl;
        std::cerr << "Usage: ./radioastro -s <signal_file> -c <config_id> [-o <output_file> -p <profile_file>]" << std::endl;
        exit(-1);
    }
    return params;
}

uint64_t CalcTimeDiff(const struct timespec & time2, const struct timespec & time1)
{
  return time2.tv_sec == time1.tv_sec ?
    time2.tv_nsec - time1.tv_nsec :
    (time2.tv_sec - time1.tv_sec - 1) * 1e9 + (1e9 - time1.tv_nsec) + time2.tv_nsec;
}

void PrintConfigs(InParameters params, configurations_t config){
    
    std::cout << "Running with:\n";
    std::cout << "Configuration ID: " << params.config_id << "\n";
    std::cout << "Signal file: " << params.signal_file << "\n";
    std::cout << "Gain file: " << config.gain_file << "\n";
    std::cout << "Frequency resolution: " << params.freq_res << " Hz\n";
    std::cout << "Velocity resolution: " << params.vel_res << " m/s\n";

    printf("+-----------------------------+-------------------+\n");
    printf("+                   Config Table                  +\n");
    printf("+-----------------------------+-------------------+\n");
    printf("| LOG2 FFT SIZE               | %17d |\n", config.log2_nfft);
    printf("| FFT SIZE                    | %17d |\n", (1 << config.log2_nfft));
    printf("| Window MA                   | %17d |\n", config.ma_w);
    printf("| Window PS                   | %17d |\n", config.ps_w);
    printf("| Window GF                   | %17d |\n", config.gf_w);
    printf("+-----------------------------+-------------------+\n");
}

void PrintTimes(TTimes & times){
    printf("+-----------------------------+-------------------+-------------------+------------------+\n");
    printf("+                                 Time Profiling Table                                   +\n");
    printf("+-----------------------------+-------------------+-------------------+------------------+\n");
    printf("| Task                        | Time (s)          | Time (ns)         | Percentage (%%)   |\n");
    printf("+-----------------------------+-------------------+-------------------+------------------+\n");
    printf("| Total processing time       | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
           times.timeTotal / 1e9, times.timeTotal, 100.0);
    printf("| + Power Spectral density    | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
           times.timePSD / 1e9, times.timePSD, (times.timePSD * 1.0 / times.timeTotal) * 100);
    printf("|   + Compute Hanning Gen     | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
            times.timeHannGen / 1e9, times.timeHannGen, (times.timeHannGen * 1.0 / times.timeTotal) * 100);
    printf("|   + Compute Hanning Window  | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
            times.timeHannWin / 1e9, times.timeHannWin, (times.timeHannWin * 1.0 / times.timeTotal) * 100);
    printf("|   + Compute Window Reduct   | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
            times.timeRed / 1e9, times.timeRed, (times.timeRed * 1.0 / times.timeTotal) * 100);
    printf("|   + Compute FFT             | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
            times.timeFFT / 1e9, times.timeFFT, (times.timeFFT * 1.0 / times.timeTotal) * 100);
    printf("|   + Compute Magnitude       | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
            times.timeMag / 1e9, times.timeMag, (times.timeMag * 1.0 / times.timeTotal) * 100);
    printf("|   + Compute Normalization   | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
            times.timeNorm / 1e9, times.timeNorm, (times.timeNorm * 1.0 / times.timeTotal) * 100);
    printf("|   + Compute Reordering FFT  | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
            times.timeReord / 1e9, times.timeReord, (times.timeReord * 1.0 / times.timeTotal) * 100);
    printf("|   + Compute Frequency Gen   | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
            times.timeFreqGen / 1e9, times.timeFreqGen, (times.timeFreqGen * 1.0 / times.timeTotal) * 100);
    printf("| + Moving average result     | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
           times.timeMA / 1e9, times.timeMA, (times.timeMA * 1.0 / times.timeTotal) * 100);
    printf("| + Peak smoothing result     | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
           times.timePS / 1e9, times.timePS, (times.timePS * 1.0 / times.timeTotal) * 100);
    printf("| + Gauss smoothing result    | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
           times.timeGF / 1e9, times.timeGF, (times.timeGF * 1.0 / times.timeTotal) * 100);
    printf("| + Calibration result        | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
           times.timeCal / 1e9, times.timeCal, (times.timeCal * 1.0 / times.timeTotal) * 100);
    printf("| + Detect peaks              | %17.3lf | %17" PRIu64 " | %16.1lf |\n", 
           times.timeDetectPeaks / 1e9, times.timeDetectPeaks, (times.timeDetectPeaks * 1.0 / times.timeTotal) * 100);
    printf("| + Compute velocity          | %17.3lf | %17" PRIu64 " | %16.1lf |\n",
           times.timeVelocity / 1e9, times.timeVelocity, (times.timeVelocity * 1.0 / times.timeTotal) * 100);
    printf("+-----------------------------+-------------------+-------------------+------------------+\n");
}

void PrintEnergies(TEnergies & energies){
    printf("+-----------------------------+-------------------+------------------+\n");
    printf("+                       Energy Profiling Table                       +\n");
    printf("+-----------------------------+-------------------+------------------+\n");
    printf("| Task                        | Energy (J)        | Percentage (%%)   |\n");
    printf("+-----------------------------+-------------------+------------------+\n");
    printf("| Total processing energy     | %17.3lf | %16.1lf |\n", 
           energies.energyTotal, 100.0);
    printf("| + Power Spectral density    | %17.3lf | %16.1lf |\n", 
           energies.energyPSD, (energies.energyPSD / energies.energyTotal) * 100);
    printf("|   + Compute Hanning Gen     | %17.3lf | %16.1lf |\n", 
            energies.energyHannGen, (energies.energyHannGen / energies.energyTotal) * 100);
    printf("|   + Compute Hanning Window  | %17.3lf | %16.1lf |\n", 
            energies.energyHannWin, (energies.energyHannWin / energies.energyTotal) * 100);
    printf("|   + Compute Window Reduct   | %17.3lf | %16.1lf |\n", 
            energies.energyRed, (energies.energyRed / energies.energyTotal) * 100);
    printf("|   + Compute FFT             | %17.3lf | %16.1lf |\n", 
            energies.energyFFT, (energies.energyFFT / energies.energyTotal) * 100);
    printf("|   + Compute Magnitude       | %17.3lf | %16.1lf |\n", 
            energies.energyMag, (energies.energyMag / energies.energyTotal) * 100);
    printf("|   + Compute Normalization   | %17.3lf | %16.1lf |\n", 
            energies.energyNorm, (energies.energyNorm / energies.energyTotal) * 100);
    printf("|   + Compute Reordering FFT  | %17.3lf | %16.1lf |\n", 
            energies.energyReord, (energies.energyReord / energies.energyTotal) * 100);
    printf("|   + Compute Frequency Gen   | %17.3lf | %16.1lf |\n", 
            energies.energyFreqGen, (energies.energyFreqGen / energies.energyTotal) * 100);
    printf("| + Moving average result     | %17.3lf | %16.1lf |\n", 
           energies.energyMA, (energies.energyMA / energies.energyTotal) * 100);
    printf("| + Peak smoothing result     | %17.3lf | %16.1lf |\n", 
           energies.energyPS, (energies.energyPS / energies.energyTotal) * 100);
    printf("| + Gauss smoothing result    | %17.3lf | %16.1lf |\n", 
           energies.energyGF, (energies.energyGF / energies.energyTotal) * 100);
    printf("| + Calibration result        | %17.3lf | %16.1lf |\n", 
           energies.energyCal, (energies.energyCal / energies.energyTotal) * 100);
    printf("| + Detect peaks              | %17.3lf | %16.1lf |\n", 
           energies.energyDetectPeaks, (energies.energyDetectPeaks / energies.energyTotal) * 100);
    printf("| + Compute velocity          | %17.3lf | %16.1lf |\n",
           energies.energyVelocity, (energies.energyVelocity / energies.energyTotal) * 100);
    printf("+-----------------------------+-------------------+------------------+\n");
}

void save_metrics(const std::string& filename, int config_id, configurations_t config, TTimes times, TEnergies energies, float snr, float freq_res, float velocity_res) {
    std::ofstream file;
    bool file_exists = std::ifstream(filename).good();

    file.open(filename, std::ios::out | std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file " << filename << std::endl;
        return;
    }

    if (!file_exists) {
        // Write header
        const std::string header = 
            "Config ID,LOG2 FFT,Window MA,Window PS,Window GF,Window PD,"
            "Time Power Spectrum Density,Time Hanning Generation,Time Hanning Window,"
            "Time Window Reduction,Time FFT,Time Magnitude,Time Normalization,"
            "Time Reordering FFT,Time Frequency Generation,Time Median Averaging,"
            "Time Peak Smoothing,Time Gauss Smoothing,Time Calibration,"
            "Time Peak Detection,Time Velocity,Time Total,"
            "Energy Power Spectrum Density,Energy Hanning Generation,Energy Hanning Window,"
            "Energy Window Reduction,Energy FFT,Energy Magnitude,Energy Normalization,"
            "Energy Reordering FFT,Energy Frequency Generation,Energy Median Averaging,"
            "Energy Peak Smoothing,Energy Gauss Smoothing,Energy Calibration,"
            "Energy Peak Detection,Energy Velocity,Energy Total,"
            "SNR,Freq Res,Vel Res,Timestamp\n";
        file << header;
    }

    // Timestamp
    std::time_t now = std::time(nullptr);
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    
    // Write metrics
    const std::string metrics = 
        std::to_string(config_id) + "," +
        std::to_string(config.log2_nfft) + "," +
        std::to_string(config.ma_w) + "," +
        std::to_string(config.ps_w) + "," +
        std::to_string(config.gf_w) + "," +
        std::to_string(config.pd_w) + "," +

        std::to_string(times.timePSD) + "," +
        std::to_string(times.timeHannGen) + "," +
        std::to_string(times.timeHannWin) + "," +
        std::to_string(times.timeRed) + "," +
        std::to_string(times.timeFFT) + "," +
        std::to_string(times.timeMag) + "," +
        std::to_string(times.timeNorm) + "," +
        std::to_string(times.timeReord) + "," +
        std::to_string(times.timeFreqGen) + "," +
        std::to_string(times.timeMA) + "," +
        std::to_string(times.timePS) + "," +
        std::to_string(times.timeGF) + "," +
        std::to_string(times.timeCal) + "," +
        std::to_string(times.timeDetectPeaks) + "," +
        std::to_string(times.timeVelocity) + "," +
        std::to_string(times.timeTotal) + "," +

        std::to_string(energies.energyPSD) + "," +
        std::to_string(energies.energyHannGen) + "," +
        std::to_string(energies.energyHannWin) + "," +
        std::to_string(energies.energyRed) + "," +
        std::to_string(energies.energyFFT) + "," +
        std::to_string(energies.energyMag) + "," +
        std::to_string(energies.energyNorm) + "," +
        std::to_string(energies.energyReord) + "," +
        std::to_string(energies.energyFreqGen) + "," +
        std::to_string(energies.energyMA) + "," +
        std::to_string(energies.energyPS) + "," +
        std::to_string(energies.energyGF) + "," +
        std::to_string(energies.energyCal) + "," +
        std::to_string(energies.energyDetectPeaks) + "," +
        std::to_string(energies.energyVelocity) + "," +
        std::to_string(energies.energyTotal) + "," +

        std::to_string(snr) + "," + 
        std::to_string(freq_res) + "," +
        std::to_string(velocity_res) + "," +
        timestamp;
    
    // Write metrics to file
    file << metrics;
    file << "\n";

    file.close();
}
