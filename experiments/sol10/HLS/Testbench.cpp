#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <cmath>
#include <math.h>
#include <complex>


#include "FFT_HW.h"

#define nFFT 1024
#define log2nFFT 10

typedef float DTYPE;

//--------------------------------------------------------------------------------------------------------------------
//Input & Output Buffer for the data to test
//--------------------------------------------------------------------------------------------------------------------



//--------------------------------------------------------------------------------------------------------------------
//SW functionality to implement in HW
//--------------------------------------------------------------------------------------------------------------------

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
        float c = std::cos(a);
        float s = std::sin(a);
        std::complex<float> twiddle = std::complex<float>(c,s);
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


std::complex<float> Stage[log2nFFT][nFFT];
void fft_SW(std::complex<float>* In, int log2_nfft, std::complex<float>* Out) {

    int nstages = log2_nfft;
    int nfft = 1 << nstages; // NFFT = 2^NStages

    bit_reverse(In, nfft, nstages, Stage[0]);
    for (int stage = 1; stage < nstages; stage++) { // Do M-1 stages of butterflies
        fft_stage(stage, Stage[stage-1], nfft, Stage[stage] );
    }
    fft_stage(nstages, Stage[nstages-1], nfft, Out);
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

float custom_hanning_window(float* win, int N){
    float location = -M_PI * 4 / 2.0;
    float norm_factor = 0;
    for (int i = 0; i < N; i++){
        location += M_PI / N;
        win[i] = sin(location) / (location);
        float temp1 = M_PI*i/(N-1);
        win[i] *= sin(temp1)*sin(temp1);
        norm_factor += win[i]* win[i];
    }
    return norm_factor;
}
//--------------------------------------------------------------------------------------------------------------------
// Functions 4 the testbench values
//--------------------------------------------------------------------------------------------------------------------

void InitVectors(std::complex<float> * inputSW,ap_uint<64>* inputHW,ap_uint<32> sizeInput)
{
  for (ap_uint<32> ii = 0; ii < sizeInput; ++ ii){
    float re = 8.0f * (static_cast<float>(rand()) / RAND_MAX) - 4.0f;
    float im = 8.0f * (static_cast<float>(rand()) / RAND_MAX) - 4.0f;
    inputSW[ii] = std::complex<float>(re,im);
  }
  std::memcpy(inputHW,inputSW, 2*sizeof(float)*sizeInput);
}

bool CompareVectors(std::complex<float>* sw_output, ap_uint<64>* hw_output, ap_uint<32> size) {
    bool success = true;
    printf("BA\n");
    std::complex<float>* HW2TEST = new std::complex<float>[size];
    printf("LA\n");
    std::memcpy(HW2TEST,hw_output,size*sizeof(std::complex<float>));

    printf("ICI\n");
    for (ap_uint<32> i = 0; i < size; ++i) {

        float sw_real = sw_output[i].real();
        float sw_imag = sw_output[i].imag();

        float hw_real = HW2TEST[i].real();
        float hw_imag = HW2TEST[i].imag();

        float error = (sw_real - hw_real) * (sw_real - hw_real) +
                      (sw_imag - hw_imag) * (sw_imag - hw_imag);

        float mag_sq = sw_real * sw_real + sw_imag * sw_imag;
        float relative_error = (mag_sq > 1e-6f) ? error / mag_sq : error;

        if (relative_error > 1e-5f) {
            success = false;
            printf("@iter %u:\n", (unsigned)i);
            printf("  SW: %f + %f j\n", sw_real, sw_imag);
            printf("  HW: %f + %f j\n", hw_real, hw_imag);
            printf("  Abs error     : %.8f\n", error);
            printf("  Relative error: %.8f %%\n", 100 * relative_error);
        }
    }

    return success;
}

void PrintVect(std::complex<float> * input1, hls::x_complex<float> * input2, ap_uint<32> size) {
  for (uint32_t ii = 0; ii < size; ++ ii)
    printf("Input 1 (SW) : %f+%f j , Input 2 (HW) : %f+%f j  \n",input1[ii].real(),input1[ii].imag(),input2[ii].real(),input2[ii].imag());
}



//--------------------------------------------------------------------------------------------------------------------
// Main
//--------------------------------------------------------------------------------------------------------------------

int main(int argc, char ** argv)
{

	printf("Launching testbench \n");

	int num = 12 ;

	//SW Memory necessary :
	float* Window = new float[nFFT*4];
	std::complex<float>* input_SW = new std::complex<float>[nFFT*num];
	std::complex<float>* buffer = new std::complex<float>[4 * nFFT];
	std::complex<float>* output_SW = new std::complex<float>[(num-3) * nFFT];

	//Computing the window factor :
	float temp = custom_hanning_window(Window,4*nFFT);



	//HW Memory necessary :
	ap_uint<64>* input_HW = new ap_uint<64>[nFFT*num];
	ap_uint<64>* output_HW = new ap_uint<64>[nFFT*(num-3)];


	// Initaliser les vecteurs :
	InitVectors(input_SW,input_HW,nFFT*num);

	for (int i = 0 ; i < num-3 ; i ++) {
	    window(input_SW+i*nFFT, 4 * nFFT, Window, buffer);
	    add_reduction_4(buffer, nFFT);
		fft_SW(buffer,log2nFFT,output_SW+i*nFFT);
	}



	printf("Processig HW : \n");

	int chunck = 6;
	Processing_HW(input_HW,output_HW,6);
	for (int i = chunck ; i < num ; i +=(chunck-3) ) {
		printf("%d\n",i);
		Processing_HW(input_HW + ((i-3)*nFFT),output_HW+((i-3)*nFFT),6);
	}
	printf("post HW : \n");
	if(CompareVectors(output_SW,output_HW,nFFT*(num-3))){
		printf("relative error less then >0.000001 :)\n");
	}


/*
	std::complex<float> test = (12.0f,12.5f);

	ap_uint<64> send ;

	std::memcpy(&send,&test, 8);
	printf("send: %d \n", send);

	float* repoint = uint32_to_float(send.range(31,0));
	float* impoint = uint32_to_float(send.range(63,32));

	float re = *repoint;
	float im = *impoint;

	hls::x_complex<float> recu =(re,im);
	printf("recu, re: %f, im: %f \n", recu.real(), recu.imag());

	hls::x_complex<float> fact = (1.0f,1.0f);
	printf("fact, re: %f, im: %f \n", fact.real(), fact.imag());

	hls::x_complex<float> end = fact+recu;
	printf("end, re: %f, im: %f \n", end.real(), end.imag());

	ap_uint<32>* re2sendpoint = float_to_uint32(end.real());
	ap_uint<32>* im2sendpoint = float_to_uint32(end.imag());

	ap_uint<32> re2send = *im2sendpoint;
	ap_uint<32> im2send = *im2sendpoint;
	printf("re2send: %f, im2send: %f", re2send, im2send);

	send.range(31,0) = re2send;
	send.range(63,32) = im2send;

	std::memcpy(&test,&send, 2*sizeof(float));

	printf("RE test = %f IM test = %f,  RE end = %f, IM end = %f\n",test.real(),test.imag(),end.real(),end.imag());
*/





	return 0;
}
