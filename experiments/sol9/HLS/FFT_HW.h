#ifndef FFT_HW_H
#define FFT_HW_H

#include <hls_math.h>

#include <ap_int.h>

#include "ap_fixed.h"




void Processing_HW(ap_uint<64>* input, ap_uint<64>* output, ap_uint<10> numFFT);



#endif // FFT_HW_H
