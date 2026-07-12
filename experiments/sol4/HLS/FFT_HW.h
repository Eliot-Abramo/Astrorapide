#ifndef FFT_HW_H
#define FFT_HW_H

#include <hls_math.h>

#include <ap_int.h>

#include "ap_fixed.h"




void fft_HW(hls::x_complex<float>* input, hls::x_complex<float>* output, ap_uint<32> numFFT);



#endif // FFT_HW_H
