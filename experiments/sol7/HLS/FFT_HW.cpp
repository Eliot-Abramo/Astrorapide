#include <ap_int.h>
#include "ap_fixed.h"
#include <stdlib.h>
#include <hls_math.h>
#include <hls_task.h>
#include <hls_np_channel.h>
#include <complex>
#include <cstdint>
#include <cstring>

using Cplx = hls::x_complex<float>;
using axi_cplx = ap_uint<64>;

typedef hls::stream<Cplx> stream_cplx;

#define nFFT 1024
#define log2nFFT 10


//--------------------------------------------------------------------------------------------------------------------
//Other necessary function prototypes :
//--------------------------------------------------------------------------------------------------------------------

//Type changing function 4 burst:
float bitcast_32_float(ap_uint<32> in);
ap_uint<32> bitcast_float_32(float in);

//Preparing the FFT :
ap_uint<32> reverse_bits(ap_uint<32> input, ap_int<32> num_stages);

//Recover value + FFT + Retrieving value :
void load_nfft_value(axi_cplx* in, stream_cplx &out, ap_uint<32> numFFT);
void fft_stage_1(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT);
void fft_stage_2(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT);
void fft_stage_3(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT);
void fft_stage_4(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT);
void fft_stage_5(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT);
void fft_stage_6(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT);
void fft_stage_7(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT);
void fft_stage_8(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT);
void fft_stage_9(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT);
void fft_stage_10(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT);
void retrieve_nfft_value(stream_cplx &in, axi_cplx* out, ap_uint<32> numFFT);

//Top function :
void Top_DataFlox(axi_cplx* in, axi_cplx* out, ap_uint<32> num);

//
//--------------------------------------------------------------------------------------------------------------------
//Hardware function :
//--------------------------------------------------------------------------------------------------------------------

void fft_HW(axi_cplx* input, axi_cplx* output, ap_uint<32> numFFT){
#pragma HLS INTERFACE mode=m_axi depth=1024 max_widen_bitwidth=128 port=input offset=slave
#pragma HLS INTERFACE mode=m_axi depth=1024 max_widen_bitwidth=128 port=output offset=slave

#pragma HLS INTERFACE s_axilite port=numFFT
#pragma HLS INTERFACE s_axilite port=return

	Top_DataFlox(input, output, numFFT);
}

//--------------------------------------------------------------------------------------------------------------------
//Other necessary function descriptions :
//--------------------------------------------------------------------------------------------------------------------
void Top_DataFlox(axi_cplx* in, axi_cplx* out, ap_uint<32> num){
	stream_cplx S0,S1,S2,S3,S4,S5,S6,S7,S8,S9,S10 ;
	#pragma HLS STREAM variable=S0 depth=nFFT
	#pragma HLS STREAM variable=S1 depth=nFFT
	#pragma HLS STREAM variable=S2 depth=nFFT
	#pragma HLS STREAM variable=S3 depth=nFFT
	#pragma HLS STREAM variable=S4 depth=nFFT
	#pragma HLS STREAM variable=S5 depth=nFFT
	#pragma HLS STREAM variable=S6 depth=nFFT
	#pragma HLS STREAM variable=S7 depth=nFFT
	#pragma HLS STREAM variable=S8 depth=nFFT
	#pragma HLS STREAM variable=S9 depth=nFFT
	#pragma HLS STREAM variable=S10 depth=nFFT

#pragma HLS DATAFLOW
	load_nfft_value(in,S0,num);
	fft_stage_1(S0,S1,num);
	fft_stage_2(S1,S2,num);
	fft_stage_3(S2,S3,num);
	fft_stage_4(S3,S4,num);
	fft_stage_5(S4,S5,num);
	fft_stage_6(S5,S6,num);
	fft_stage_7(S6,S7,num);
	fft_stage_8(S7,S8,num);
	fft_stage_9(S8,S9,num);
	fft_stage_10(S9,S10,num);
	retrieve_nfft_value(S10, out,num);
}

float bitcast_32_float(ap_uint<32> in){
#pragma HLS INLINE
	uint32_t temp = in.to_uint();
	float out;
	std::memcpy(&out,&temp,sizeof(float));
	return out;
}

ap_uint<32> bitcast_float_32(float in){
#pragma HLS INLINE
	uint32_t temp ;
	std::memcpy(&temp,&in,sizeof(float));
	ap_uint<32> out = temp;
	return out;
}

void load_nfft_value(axi_cplx* in, stream_cplx &out, ap_uint<32> numFFT){
#pragma HLS INLINE off

	axi_cplx BufferIN[nFFT];
	#pragma HLS ARRAY_PARTITION variable=BufferIN type=cyclic factor=2

    ap_uint<32> reversed;

    for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
    	for (ap_uint<32> y = 0 ; y < nFFT ; y ++){
		#pragma HLS PIPELINE II=1
    		BufferIN[y] = in[y+ counter*nFFT];
    	}

    	for (ap_uint<32> i = 0; i < nFFT; i++) {
		#pragma HLS PIPELINE II = 1
			reversed = reverse_bits(i, log2nFFT);
			axi_cplx raw = BufferIN[reversed];
            ap_uint<32> lo = raw.range(31,0);
            ap_uint<32> hi = raw.range(63,32);
            float re = bitcast_32_float(lo);
            float im = bitcast_32_float(hi);
			out.write(Cplx(re,im));
		}
    }
}

ap_uint<32> reverse_bits(ap_uint<32> input, ap_int<32> num_stages) {
	ap_int<32> i, rev = 0;
	for (i = 0; i < num_stages; i++) {
		rev = (rev << 1) | (input & 1);
		input = input >> 1;
	}
	return rev;
}

void fft_stage_1(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT){
#pragma HLS INLINE off
//#pragma HLS PIPELINE
	Cplx Twidle = Cplx(0x1p+0f, -0x0p+0f) ;

	stage_1_counter:for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
		stage_1_total:for(ap_uint<32> jj = 0 ; jj < nFFT/2 ; jj ++) {
		#pragma HLS PIPELINE II = 2
			Cplx a = in.read();
			Cplx b = in.read() * Twidle;
			out.write(a + b);
			out.write(a - b);
		}
	}
}
void fft_stage_2(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT){
#pragma HLS INLINE off
//#pragma HLS PIPELINE
	Cplx In[4];
	Cplx OutBuff[2];

	Cplx Twiddle[2] = {Cplx(0x1p+0f, -0x0p+0f),
			Cplx(-0x1.777a5cp-25f, -0x1p+0f)};
	stage_2_counter:for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
		stage_2_total:for(ap_uint<32> jj = 0 ; jj < nFFT/4 ; jj ++){
			stage_2_0:for(ap_uint<3> i = 0 ; i < 4 ; i ++){
			#pragma HLS PIPELINE II=1
				In[i] = in.read();
			}
			stage_2_1:for (ap_uint<2> ii = 0 ; ii < 2 ; ii++){
			#pragma HLS PIPELINE II=2
				Cplx a = In[ii];
				Cplx b = In[ii + 2] * Twiddle[ii];
				out.write(a+b);
				OutBuff[ii] = a-b ;
			}
			stage_2_2:for (ap_uint<2> ii = 0 ; ii < 2 ; ii ++){
			#pragma HLS PIPELINE II=1
				out.write(OutBuff[ii]);
			}
		}
	}
}
void fft_stage_3(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT){
#pragma HLS INLINE off
//#pragma HLS PIPELINE
	Cplx In[8];
	Cplx OutBuff[4];
	Cplx Twiddle[4] = {Cplx(0x1p+0f, -0x0p+0f),
			Cplx(0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(-0x1.777a5cp-25f, -0x1p+0f),
			Cplx(-0x1.6a09e6p-1f, -0x1.6a09e6p-1f)};

	stage_3_counter:for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
		stage_3_total:for (ap_uint<32> jj = 0 ; jj < nFFT/8; jj ++){
			stage_3_0:for (ap_uint<4> i = 0 ; i < 8 ; i ++){
			#pragma HLS PIPELINE II=1
				In[i] = in.read();
			}

			stage_3_1:for (ap_uint<3> ii = 0 ; ii < 4 ; ii++){
			#pragma HLS PIPELINE II=2
				Cplx a = In[ii];
				Cplx b = In[ii + 4] * Twiddle[ii];
				out.write(a+b);
				OutBuff[ii] = a-b ;
			}

			stage_3_2:for (ap_uint<3> ii = 0 ; ii < 4 ; ii ++){
			#pragma HLS PIPELINE II=1
				out.write(OutBuff[ii]);
			}
		}
	}
}
void fft_stage_4(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT) {
#pragma HLS INLINE off
//#pragma HLS PIPELINE
	Cplx In[16];
	Cplx OutBuff[8];
	Cplx Twiddle[8] = {Cplx(0x1p+0f, -0x0p+0f),
			Cplx(0x1.d906bcp-1f, -0x1.87de2cp-2f),
			Cplx(0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(0x1.87de2ap-2f, -0x1.d906bcp-1f),
			Cplx(-0x1.777a5cp-25f, -0x1p+0f),
			Cplx(-0x1.87de3p-2f, -0x1.d906bcp-1f),
			Cplx(-0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(-0x1.d906cp-1f, -0x1.87de2p-2f)};
	stage_4_counter :for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
		stage_4_total :for (ap_uint<32> jj = 0 ; jj < nFFT/16 ; jj++){
			stage_4_0 :for (ap_uint<5> i = 0 ; i < 16 ; i ++){
			#pragma HLS PIPELINE II=1
				In[i] = in.read();
			}

			stage_4_1 :for (ap_uint<4> ii = 0 ; ii < 8 ; ii++){
			#pragma HLS PIPELINE II=2
				Cplx a = In[ii];
				Cplx b = In[ii + 8] * Twiddle[ii];
				out.write(a+b);
				OutBuff[ii] = a-b ;
			}

			stage_4_2 :for (ap_uint<4> ii = 0 ; ii < 8 ; ii ++){
			#pragma HLS PIPELINE II=1
				out.write(OutBuff[ii]);
			}
		}
	}
}
void fft_stage_5(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT) {
#pragma HLS INLINE off
//#pragma HLS PIPELINE
	Cplx In[32];
	Cplx OutBuff[16];
	Cplx Twiddle[16] = {Cplx(0x1p+0f, -0x0p+0f),
			Cplx(0x1.f6297cp-1f, -0x1.8f8b84p-3f),
			Cplx(0x1.d906bcp-1f, -0x1.87de2cp-2f),
			Cplx(0x1.a9b662p-1f, -0x1.1c73b4p-1f),
			Cplx(0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(0x1.1c73b2p-1f, -0x1.a9b664p-1f),
			Cplx(0x1.87de2ap-2f, -0x1.d906bcp-1f),
			Cplx(0x1.8f8b78p-3f, -0x1.f6297ep-1f),
			Cplx(-0x1.777a5cp-25f, -0x1p+0f),
			Cplx(-0x1.8f8b84p-3f, -0x1.f6297cp-1f),
			Cplx(-0x1.87de3p-2f, -0x1.d906bcp-1f),
			Cplx(-0x1.1c73b8p-1f, -0x1.a9b66p-1f),
			Cplx(-0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(-0x1.a9b664p-1f, -0x1.1c73b2p-1f),
			Cplx(-0x1.d906cp-1f, -0x1.87de2p-2f),
			Cplx(-0x1.f6297ep-1f, -0x1.8f8b82p-3f)};
	stage_5_counter :for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
		stage_5_total :for (ap_uint<32> jj = 0 ; jj < nFFT/32 ; jj++){
			stage_5_0 :for (ap_uint<6> i = 0 ; i < 32 ; i ++){
			#pragma HLS PIPELINE II=1
				In[i] = in.read();
			}

			stage_5_1 :for (ap_uint<5> ii = 0 ; ii < 16 ; ii++){
			#pragma HLS PIPELINE II=2
				Cplx a = In[ii];
				Cplx b = In[ii + 16] * Twiddle[ii];
				out.write(a+b);
				OutBuff[ii] = a-b ;
			}

			stage_5_2 :for (ap_uint<5> ii = 0 ; ii < 16 ; ii ++){
			#pragma HLS PIPELINE II=1
				out.write(OutBuff[ii]);
			}
		}
	}
}
void fft_stage_6(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT) {
#pragma HLS INLINE off
//#pragma HLS PIPELINE
	Cplx In[64];
	Cplx OutBuff[32];
	Cplx Twiddle[32] = {Cplx(0x1p+0f, -0x0p+0f),
			Cplx(0x1.fd88dap-1f, -0x1.917a6cp-4f),
			Cplx(0x1.f6297cp-1f, -0x1.8f8b84p-3f),
			Cplx(0x1.e9f416p-1f, -0x1.294064p-2f),
			Cplx(0x1.d906bcp-1f, -0x1.87de2cp-2f),
			Cplx(0x1.c38b2ep-1f, -0x1.e2b5d4p-2f),
			Cplx(0x1.a9b662p-1f, -0x1.1c73b4p-1f),
			Cplx(0x1.8bc806p-1f, -0x1.44cf34p-1f),
			Cplx(0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(0x1.44cf32p-1f, -0x1.8bc806p-1f),
			Cplx(0x1.1c73b2p-1f, -0x1.a9b664p-1f),
			Cplx(0x1.e2b5cep-2f, -0x1.c38b3p-1f),
			Cplx(0x1.87de2ap-2f, -0x1.d906bcp-1f),
			Cplx(0x1.29406p-2f, -0x1.e9f416p-1f),
			Cplx(0x1.8f8b78p-3f, -0x1.f6297ep-1f),
			Cplx(0x1.917a6ap-4f, -0x1.fd88dap-1f),
			Cplx(-0x1.777a5cp-25f, -0x1p+0f),
			Cplx(-0x1.917a82p-4f, -0x1.fd88dap-1f),
			Cplx(-0x1.8f8b84p-3f, -0x1.f6297cp-1f),
			Cplx(-0x1.294066p-2f, -0x1.e9f414p-1f),
			Cplx(-0x1.87de3p-2f, -0x1.d906bcp-1f),
			Cplx(-0x1.e2b5dap-2f, -0x1.c38b2ep-1f),
			Cplx(-0x1.1c73b8p-1f, -0x1.a9b66p-1f),
			Cplx(-0x1.44cf32p-1f, -0x1.8bc808p-1f),
			Cplx(-0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(-0x1.8bc808p-1f, -0x1.44cf32p-1f),
			Cplx(-0x1.a9b664p-1f, -0x1.1c73b2p-1f),
			Cplx(-0x1.c38b32p-1f, -0x1.e2b5ccp-2f),
			Cplx(-0x1.d906cp-1f, -0x1.87de2p-2f),
			Cplx(-0x1.e9f416p-1f, -0x1.294066p-2f),
			Cplx(-0x1.f6297ep-1f, -0x1.8f8b82p-3f),
			Cplx(-0x1.fd88dap-1f, -0x1.917a6p-4f)};
	stage_6_counter :for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
		stage_6_total :for (ap_uint<32> jj = 0 ; jj < nFFT/64 ; jj++){
			stage_6_0 :for (ap_uint<7> i = 0 ; i < 64 ; i ++){
			#pragma HLS PIPELINE II=1
				In[i] = in.read();
			}

			stage_6_1 :for (ap_uint<6> ii = 0 ; ii < 32 ; ii++){
			#pragma HLS PIPELINE II=2
				Cplx a = In[ii];
				Cplx b = In[ii + 32] * Twiddle[ii];
				out.write(a+b);
				OutBuff[ii] = a-b ;
			}

			stage_6_2 :for (ap_uint<6> ii = 0 ; ii < 32 ; ii ++){
			#pragma HLS PIPELINE II=1
				out.write(OutBuff[ii]);
			}
		}
	}
}
void fft_stage_7(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT) {
#pragma HLS INLINE off
//#pragma HLS PIPELINE
	Cplx In[128];
	Cplx OutBuff[64];
	Cplx Twiddle[64] = {Cplx(0x1p+0f, -0x0p+0f),
			Cplx(0x1.ff621ep-1f, -0x1.91f66p-5f),
			Cplx(0x1.fd88dap-1f, -0x1.917a6cp-4f),
			Cplx(0x1.fa7558p-1f, -0x1.2c8106p-3f),
			Cplx(0x1.f6297cp-1f, -0x1.8f8b84p-3f),
			Cplx(0x1.f0a7fp-1f, -0x1.f19f9ap-3f),
			Cplx(0x1.e9f416p-1f, -0x1.294064p-2f),
			Cplx(0x1.e2121p-1f, -0x1.58f9a8p-2f),
			Cplx(0x1.d906bcp-1f, -0x1.87de2cp-2f),
			Cplx(0x1.ced7bp-1f, -0x1.b5d1p-2f),
			Cplx(0x1.c38b2ep-1f, -0x1.e2b5d4p-2f),
			Cplx(0x1.b72834p-1f, -0x1.07387ap-1f),
			Cplx(0x1.a9b662p-1f, -0x1.1c73b4p-1f),
			Cplx(0x1.9b3e04p-1f, -0x1.30ff8p-1f),
			Cplx(0x1.8bc806p-1f, -0x1.44cf34p-1f),
			Cplx(0x1.7b5df2p-1f, -0x1.57d694p-1f),
			Cplx(0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(0x1.57d692p-1f, -0x1.7b5df4p-1f),
			Cplx(0x1.44cf32p-1f, -0x1.8bc806p-1f),
			Cplx(0x1.30ff8p-1f, -0x1.9b3e04p-1f),
			Cplx(0x1.1c73b2p-1f, -0x1.a9b664p-1f),
			Cplx(0x1.073878p-1f, -0x1.b72836p-1f),
			Cplx(0x1.e2b5cep-2f, -0x1.c38b3p-1f),
			Cplx(0x1.b5d102p-2f, -0x1.ced7bp-1f),
			Cplx(0x1.87de2ap-2f, -0x1.d906bcp-1f),
			Cplx(0x1.58f9a6p-2f, -0x1.e2121p-1f),
			Cplx(0x1.29406p-2f, -0x1.e9f416p-1f),
			Cplx(0x1.f19f9p-3f, -0x1.f0a7fp-1f),
			Cplx(0x1.8f8b78p-3f, -0x1.f6297ep-1f),
			Cplx(0x1.2c810ap-3f, -0x1.fa7558p-1f),
			Cplx(0x1.917a6ap-4f, -0x1.fd88dap-1f),
			Cplx(0x1.91f652p-5f, -0x1.ff621ep-1f),
			Cplx(-0x1.777a5cp-25f, -0x1p+0f),
			Cplx(-0x1.91f682p-5f, -0x1.ff621ep-1f),
			Cplx(-0x1.917a82p-4f, -0x1.fd88dap-1f),
			Cplx(-0x1.2c8114p-3f, -0x1.fa7558p-1f),
			Cplx(-0x1.8f8b84p-3f, -0x1.f6297cp-1f),
			Cplx(-0x1.f19f9ap-3f, -0x1.f0a7fp-1f),
			Cplx(-0x1.294066p-2f, -0x1.e9f414p-1f),
			Cplx(-0x1.58f9acp-2f, -0x1.e2121p-1f),
			Cplx(-0x1.87de3p-2f, -0x1.d906bcp-1f),
			Cplx(-0x1.b5d1p-2f, -0x1.ced7bp-1f),
			Cplx(-0x1.e2b5dap-2f, -0x1.c38b2ep-1f),
			Cplx(-0x1.07387ap-1f, -0x1.b72834p-1f),
			Cplx(-0x1.1c73b8p-1f, -0x1.a9b66p-1f),
			Cplx(-0x1.30ff82p-1f, -0x1.9b3e04p-1f),
			Cplx(-0x1.44cf32p-1f, -0x1.8bc808p-1f),
			Cplx(-0x1.57d696p-1f, -0x1.7b5dfp-1f),
			Cplx(-0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(-0x1.7b5df6p-1f, -0x1.57d69p-1f),
			Cplx(-0x1.8bc808p-1f, -0x1.44cf32p-1f),
			Cplx(-0x1.9b3e08p-1f, -0x1.30ff7ap-1f),
			Cplx(-0x1.a9b664p-1f, -0x1.1c73b2p-1f),
			Cplx(-0x1.b72834p-1f, -0x1.07387ap-1f),
			Cplx(-0x1.c38b32p-1f, -0x1.e2b5ccp-2f),
			Cplx(-0x1.ced7bp-1f, -0x1.b5d0fep-2f),
			Cplx(-0x1.d906cp-1f, -0x1.87de2p-2f),
			Cplx(-0x1.e21212p-1f, -0x1.58f9a4p-2f),
			Cplx(-0x1.e9f416p-1f, -0x1.294066p-2f),
			Cplx(-0x1.f0a7fp-1f, -0x1.f19f8ap-3f),
			Cplx(-0x1.f6297ep-1f, -0x1.8f8b82p-3f),
			Cplx(-0x1.fa7558p-1f, -0x1.2c80f4p-3f),
			Cplx(-0x1.fd88dap-1f, -0x1.917a6p-4f),
			Cplx(-0x1.ff621ep-1f, -0x1.91f5fap-5f)};
	stage_7_counter :for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
		stage_7_total :for (ap_uint<32> jj = 0 ; jj < nFFT/128 ; jj++){
			stage_7_0 :for (ap_uint<8> i = 0 ; i < 128 ; i ++){
			#pragma HLS PIPELINE II=1
				In[i] = in.read();
			}

			stage_7_1 :for (ap_uint<7> ii = 0 ; ii < 64 ; ii++){
			#pragma HLS PIPELINE II=2
				Cplx a = In[ii];
				Cplx b = In[ii + 64] * Twiddle[ii];
				out.write(a+b);
				OutBuff[ii] = a-b ;
			}

			stage_7_2 :for (ap_uint<7> ii = 0 ; ii < 64 ; ii ++){
			#pragma HLS PIPELINE II=1
				out.write(OutBuff[ii]);
			}
		}
	}
}
void fft_stage_8(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT){
#pragma HLS INLINE off
//#pragma HLS PIPELINE
	Cplx In[256];
#pragma HLS BIND_STORAGE variable=In type=RAM_T2P
#pragma HLS ARRAY_PARTITION variable=In type=block factor=2
	Cplx OutBuff[128];

	Cplx Twiddle[128]={Cplx(0x1p+0f, -0x0p+0f),
			Cplx(0x1.ffd886p-1f, -0x1.92156p-6f),
			Cplx(0x1.ff621ep-1f, -0x1.91f66p-5f),
			Cplx(0x1.fe9cdap-1f, -0x1.2d520ap-4f),
			Cplx(0x1.fd88dap-1f, -0x1.917a6cp-4f),
			Cplx(0x1.fc2648p-1f, -0x1.f564e6p-4f),
			Cplx(0x1.fa7558p-1f, -0x1.2c8106p-3f),
			Cplx(0x1.f8765p-1f, -0x1.5e2146p-3f),
			Cplx(0x1.f6297cp-1f, -0x1.8f8b84p-3f),
			Cplx(0x1.f38f3ap-1f, -0x1.c0b826p-3f),
			Cplx(0x1.f0a7fp-1f, -0x1.f19f9ap-3f),
			Cplx(0x1.ed740ep-1f, -0x1.111d28p-2f),
			Cplx(0x1.e9f416p-1f, -0x1.294064p-2f),
			Cplx(0x1.e6288ep-1f, -0x1.4135cap-2f),
			Cplx(0x1.e2121p-1f, -0x1.58f9a8p-2f),
			Cplx(0x1.ddb13cp-1f, -0x1.708854p-2f),
			Cplx(0x1.d906bcp-1f, -0x1.87de2cp-2f),
			Cplx(0x1.d4134cp-1f, -0x1.9ef796p-2f),
			Cplx(0x1.ced7bp-1f, -0x1.b5d1p-2f),
			Cplx(0x1.c954b2p-1f, -0x1.cc66eap-2f),
			Cplx(0x1.c38b2ep-1f, -0x1.e2b5d4p-2f),
			Cplx(0x1.bd7c0ap-1f, -0x1.f8ba5p-2f),
			Cplx(0x1.b72834p-1f, -0x1.07387ap-1f),
			Cplx(0x1.b090a6p-1f, -0x1.11eb34p-1f),
			Cplx(0x1.a9b662p-1f, -0x1.1c73b4p-1f),
			Cplx(0x1.a29a7ap-1f, -0x1.26d056p-1f),
			Cplx(0x1.9b3e04p-1f, -0x1.30ff8p-1f),
			Cplx(0x1.93a224p-1f, -0x1.3affa4p-1f),
			Cplx(0x1.8bc806p-1f, -0x1.44cf34p-1f),
			Cplx(0x1.83b0ep-1f, -0x1.4e6cacp-1f),
			Cplx(0x1.7b5df2p-1f, -0x1.57d694p-1f),
			Cplx(0x1.72d084p-1f, -0x1.610b76p-1f),
			Cplx(0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(0x1.610b74p-1f, -0x1.72d084p-1f),
			Cplx(0x1.57d692p-1f, -0x1.7b5df4p-1f),
			Cplx(0x1.4e6caap-1f, -0x1.83b0e2p-1f),
			Cplx(0x1.44cf32p-1f, -0x1.8bc806p-1f),
			Cplx(0x1.3affa2p-1f, -0x1.93a224p-1f),
			Cplx(0x1.30ff8p-1f, -0x1.9b3e04p-1f),
			Cplx(0x1.26d054p-1f, -0x1.a29a7ap-1f),
			Cplx(0x1.1c73b2p-1f, -0x1.a9b664p-1f),
			Cplx(0x1.11eb36p-1f, -0x1.b090a6p-1f),
			Cplx(0x1.073878p-1f, -0x1.b72836p-1f),
			Cplx(0x1.f8ba4cp-2f, -0x1.bd7c0ap-1f),
			Cplx(0x1.e2b5cep-2f, -0x1.c38b3p-1f),
			Cplx(0x1.cc66e8p-2f, -0x1.c954b2p-1f),
			Cplx(0x1.b5d102p-2f, -0x1.ced7bp-1f),
			Cplx(0x1.9ef792p-2f, -0x1.d4134ep-1f),
			Cplx(0x1.87de2ap-2f, -0x1.d906bcp-1f),
			Cplx(0x1.70884ep-2f, -0x1.ddb13cp-1f),
			Cplx(0x1.58f9a6p-2f, -0x1.e2121p-1f),
			Cplx(0x1.4135c4p-2f, -0x1.e6289p-1f),
			Cplx(0x1.29406p-2f, -0x1.e9f416p-1f),
			Cplx(0x1.111d26p-2f, -0x1.ed740ep-1f),
			Cplx(0x1.f19f9p-3f, -0x1.f0a7fp-1f),
			Cplx(0x1.c0b826p-3f, -0x1.f38f3ap-1f),
			Cplx(0x1.8f8b78p-3f, -0x1.f6297ep-1f),
			Cplx(0x1.5e214p-3f, -0x1.f8765p-1f),
			Cplx(0x1.2c810ap-3f, -0x1.fa7558p-1f),
			Cplx(0x1.f564d8p-4f, -0x1.fc2648p-1f),
			Cplx(0x1.917a6ap-4f, -0x1.fd88dap-1f),
			Cplx(0x1.2d51f6p-4f, -0x1.fe9cdap-1f),
			Cplx(0x1.91f652p-5f, -0x1.ff621ep-1f),
			Cplx(0x1.9214fcp-6f, -0x1.ffd886p-1f),
			Cplx(-0x1.777a5cp-25f, -0x1p+0f),
			Cplx(-0x1.92155ap-6f, -0x1.ffd886p-1f),
			Cplx(-0x1.91f682p-5f, -0x1.ff621ep-1f),
			Cplx(-0x1.2d520cp-4f, -0x1.fe9cdap-1f),
			Cplx(-0x1.917a82p-4f, -0x1.fd88dap-1f),
			Cplx(-0x1.f564eep-4f, -0x1.fc2646p-1f),
			Cplx(-0x1.2c8114p-3f, -0x1.fa7558p-1f),
			Cplx(-0x1.5e214cp-3f, -0x1.f8765p-1f),
			Cplx(-0x1.8f8b84p-3f, -0x1.f6297cp-1f),
			Cplx(-0x1.c0b83p-3f, -0x1.f38f3ap-1f),
			Cplx(-0x1.f19f9ap-3f, -0x1.f0a7fp-1f),
			Cplx(-0x1.111d2cp-2f, -0x1.ed740ep-1f),
			Cplx(-0x1.294066p-2f, -0x1.e9f414p-1f),
			Cplx(-0x1.4135c8p-2f, -0x1.e6288ep-1f),
			Cplx(-0x1.58f9acp-2f, -0x1.e2121p-1f),
			Cplx(-0x1.708854p-2f, -0x1.ddb13cp-1f),
			Cplx(-0x1.87de3p-2f, -0x1.d906bcp-1f),
			Cplx(-0x1.9ef796p-2f, -0x1.d4134cp-1f),
			Cplx(-0x1.b5d1p-2f, -0x1.ced7bp-1f),
			Cplx(-0x1.cc66ecp-2f, -0x1.c954b2p-1f),
			Cplx(-0x1.e2b5dap-2f, -0x1.c38b2ep-1f),
			Cplx(-0x1.f8ba4cp-2f, -0x1.bd7c0cp-1f),
			Cplx(-0x1.07387ap-1f, -0x1.b72834p-1f),
			Cplx(-0x1.11eb38p-1f, -0x1.b090a4p-1f),
			Cplx(-0x1.1c73b8p-1f, -0x1.a9b66p-1f),
			Cplx(-0x1.26d054p-1f, -0x1.a29a7ap-1f),
			Cplx(-0x1.30ff82p-1f, -0x1.9b3e04p-1f),
			Cplx(-0x1.3affa6p-1f, -0x1.93a222p-1f),
			Cplx(-0x1.44cf32p-1f, -0x1.8bc808p-1f),
			Cplx(-0x1.4e6cacp-1f, -0x1.83b0ep-1f),
			Cplx(-0x1.57d696p-1f, -0x1.7b5dfp-1f),
			Cplx(-0x1.610b7ap-1f, -0x1.72d08p-1f),
			Cplx(-0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(-0x1.72d086p-1f, -0x1.610b74p-1f),
			Cplx(-0x1.7b5df6p-1f, -0x1.57d69p-1f),
			Cplx(-0x1.83b0ep-1f, -0x1.4e6cacp-1f),
			Cplx(-0x1.8bc808p-1f, -0x1.44cf32p-1f),
			Cplx(-0x1.93a226p-1f, -0x1.3affap-1f),
			Cplx(-0x1.9b3e08p-1f, -0x1.30ff7ap-1f),
			Cplx(-0x1.a29a7ap-1f, -0x1.26d054p-1f),
			Cplx(-0x1.a9b664p-1f, -0x1.1c73b2p-1f),
			Cplx(-0x1.b090a8p-1f, -0x1.11eb3p-1f),
			Cplx(-0x1.b72834p-1f, -0x1.07387ap-1f),
			Cplx(-0x1.bd7c0cp-1f, -0x1.f8ba4ap-2f),
			Cplx(-0x1.c38b32p-1f, -0x1.e2b5ccp-2f),
			Cplx(-0x1.c954b2p-1f, -0x1.cc66ecp-2f),
			Cplx(-0x1.ced7bp-1f, -0x1.b5d0fep-2f),
			Cplx(-0x1.d4134ep-1f, -0x1.9ef78ep-2f),
			Cplx(-0x1.d906cp-1f, -0x1.87de2p-2f),
			Cplx(-0x1.ddb13cp-1f, -0x1.708854p-2f),
			Cplx(-0x1.e21212p-1f, -0x1.58f9a4p-2f),
			Cplx(-0x1.e6289p-1f, -0x1.4135cp-2f),
			Cplx(-0x1.e9f416p-1f, -0x1.294066p-2f),
			Cplx(-0x1.ed740ep-1f, -0x1.111d24p-2f),
			Cplx(-0x1.f0a7fp-1f, -0x1.f19f8ap-3f),
			Cplx(-0x1.f38f3cp-1f, -0x1.c0b81p-3f),
			Cplx(-0x1.f6297ep-1f, -0x1.8f8b82p-3f),
			Cplx(-0x1.f8765p-1f, -0x1.5e213ap-3f),
			Cplx(-0x1.fa7558p-1f, -0x1.2c80f4p-3f),
			Cplx(-0x1.fc2646p-1f, -0x1.f564ecp-4f),
			Cplx(-0x1.fd88dap-1f, -0x1.917a6p-4f),
			Cplx(-0x1.fe9cdcp-1f, -0x1.2d51eap-4f),
			Cplx(-0x1.ff621ep-1f, -0x1.91f5fap-5f),
			Cplx(-0x1.ffd886p-1f, -0x1.92154cp-6f)};
	stage_8_counter : for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
		stage_8_total :for (ap_uint<32> jj = 0 ; jj < nFFT/256 ; jj++){
			stage_8_0 :for (ap_uint<9> i = 0 ; i < 256 ; i ++){
			#pragma HLS PIPELINE II=1
				In[i] = in.read();
			}
			stage_8_1 :for (ap_uint<8> ii = 0 ; ii < 128 ; ii++){
			#pragma HLS PIPELINE II=2
				Cplx a = In[ii];
				Cplx b = In[ii + 128] * Twiddle[ii];
				out.write(a+b);
				OutBuff[ii] = a-b ;
			}

			stage_8_2 : for (ap_uint<8> ii = 0 ; ii < 128 ; ii ++){
			#pragma HLS PIPELINE II=1
				out.write(OutBuff[ii]);
			}
		}
	}
}
void fft_stage_9(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT){
#pragma HLS INLINE off
//#pragma HLS PIPELINE
	Cplx In[512];
#pragma HLS BIND_STORAGE variable=In type=RAM_T2P
#pragma HLS ARRAY_PARTITION variable=In type=block factor=2
	Cplx OutBuff[256];

	Cplx Twiddle[256] = {Cplx(0x1p+0f, -0x0p+0f),
			Cplx(0x1.fff622p-1f, -0x1.921d2p-7f),
			Cplx(0x1.ffd886p-1f, -0x1.92156p-6f),
			Cplx(0x1.ffa72ep-1f, -0x1.2d8658p-5f),
			Cplx(0x1.ff621ep-1f, -0x1.91f66p-5f),
			Cplx(0x1.ff0956p-1f, -0x1.f656eap-5f),
			Cplx(0x1.fe9cdap-1f, -0x1.2d520ap-4f),
			Cplx(0x1.fe1cbp-1f, -0x1.5f6d02p-4f),
			Cplx(0x1.fd88dap-1f, -0x1.917a6cp-4f),
			Cplx(0x1.fce16p-1f, -0x1.c3785cp-4f),
			Cplx(0x1.fc2648p-1f, -0x1.f564e6p-4f),
			Cplx(0x1.fb5798p-1f, -0x1.139f0ep-3f),
			Cplx(0x1.fa7558p-1f, -0x1.2c8106p-3f),
			Cplx(0x1.f97f92p-1f, -0x1.45576cp-3f),
			Cplx(0x1.f8765p-1f, -0x1.5e2146p-3f),
			Cplx(0x1.f7599ap-1f, -0x1.76dd9ep-3f),
			Cplx(0x1.f6297cp-1f, -0x1.8f8b84p-3f),
			Cplx(0x1.f4e604p-1f, -0x1.a82a04p-3f),
			Cplx(0x1.f38f3ap-1f, -0x1.c0b826p-3f),
			Cplx(0x1.f2253p-1f, -0x1.d935p-3f),
			Cplx(0x1.f0a7fp-1f, -0x1.f19f9ap-3f),
			Cplx(0x1.ef178ap-1f, -0x1.04fb82p-2f),
			Cplx(0x1.ed740ep-1f, -0x1.111d28p-2f),
			Cplx(0x1.ebbd8cp-1f, -0x1.1d3444p-2f),
			Cplx(0x1.e9f416p-1f, -0x1.294064p-2f),
			Cplx(0x1.e817bap-1f, -0x1.35410cp-2f),
			Cplx(0x1.e6288ep-1f, -0x1.4135cap-2f),
			Cplx(0x1.e426a4p-1f, -0x1.4d1e26p-2f),
			Cplx(0x1.e2121p-1f, -0x1.58f9a8p-2f),
			Cplx(0x1.dfeae6p-1f, -0x1.64c7dep-2f),
			Cplx(0x1.ddb13cp-1f, -0x1.708854p-2f),
			Cplx(0x1.db6526p-1f, -0x1.7c3a94p-2f),
			Cplx(0x1.d906bcp-1f, -0x1.87de2cp-2f),
			Cplx(0x1.d69618p-1f, -0x1.9372a8p-2f),
			Cplx(0x1.d4134cp-1f, -0x1.9ef796p-2f),
			Cplx(0x1.d17e76p-1f, -0x1.aa6c84p-2f),
			Cplx(0x1.ced7bp-1f, -0x1.b5d1p-2f),
			Cplx(0x1.cc1f1p-1f, -0x1.c1249ep-2f),
			Cplx(0x1.c954b2p-1f, -0x1.cc66eap-2f),
			Cplx(0x1.c678b4p-1f, -0x1.d79776p-2f),
			Cplx(0x1.c38b2ep-1f, -0x1.e2b5d4p-2f),
			Cplx(0x1.c08c42p-1f, -0x1.edc194p-2f),
			Cplx(0x1.bd7c0ap-1f, -0x1.f8ba5p-2f),
			Cplx(0x1.ba5aa6p-1f, -0x1.01cfc8p-1f),
			Cplx(0x1.b72834p-1f, -0x1.07387ap-1f),
			Cplx(0x1.b3e4d4p-1f, -0x1.0c9706p-1f),
			Cplx(0x1.b090a6p-1f, -0x1.11eb34p-1f),
			Cplx(0x1.ad2bcap-1f, -0x1.1734d6p-1f),
			Cplx(0x1.a9b662p-1f, -0x1.1c73b4p-1f),
			Cplx(0x1.a63092p-1f, -0x1.21a79ap-1f),
			Cplx(0x1.a29a7ap-1f, -0x1.26d056p-1f),
			Cplx(0x1.9ef43ep-1f, -0x1.2bedb4p-1f),
			Cplx(0x1.9b3e04p-1f, -0x1.30ff8p-1f),
			Cplx(0x1.9777fp-1f, -0x1.36058ap-1f),
			Cplx(0x1.93a224p-1f, -0x1.3affa4p-1f),
			Cplx(0x1.8fbccap-1f, -0x1.3fed96p-1f),
			Cplx(0x1.8bc806p-1f, -0x1.44cf34p-1f),
			Cplx(0x1.87c4p-1f, -0x1.49a44ap-1f),
			Cplx(0x1.83b0ep-1f, -0x1.4e6cacp-1f),
			Cplx(0x1.7f8ecep-1f, -0x1.53282ap-1f),
			Cplx(0x1.7b5df2p-1f, -0x1.57d694p-1f),
			Cplx(0x1.771e76p-1f, -0x1.5c77bcp-1f),
			Cplx(0x1.72d084p-1f, -0x1.610b76p-1f),
			Cplx(0x1.6e7444p-1f, -0x1.659194p-1f),
			Cplx(0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(0x1.659192p-1f, -0x1.6e7446p-1f),
			Cplx(0x1.610b74p-1f, -0x1.72d084p-1f),
			Cplx(0x1.5c77bcp-1f, -0x1.771e76p-1f),
			Cplx(0x1.57d692p-1f, -0x1.7b5df4p-1f),
			Cplx(0x1.532828p-1f, -0x1.7f8ecep-1f),
			Cplx(0x1.4e6caap-1f, -0x1.83b0e2p-1f),
			Cplx(0x1.49a44ap-1f, -0x1.87c402p-1f),
			Cplx(0x1.44cf32p-1f, -0x1.8bc806p-1f),
			Cplx(0x1.3fed94p-1f, -0x1.8fbcccp-1f),
			Cplx(0x1.3affa2p-1f, -0x1.93a224p-1f),
			Cplx(0x1.36058ap-1f, -0x1.9777fp-1f),
			Cplx(0x1.30ff8p-1f, -0x1.9b3e04p-1f),
			Cplx(0x1.2bedb2p-1f, -0x1.9ef43ep-1f),
			Cplx(0x1.26d054p-1f, -0x1.a29a7ap-1f),
			Cplx(0x1.21a79ap-1f, -0x1.a63092p-1f),
			Cplx(0x1.1c73b2p-1f, -0x1.a9b664p-1f),
			Cplx(0x1.1734d6p-1f, -0x1.ad2bcap-1f),
			Cplx(0x1.11eb36p-1f, -0x1.b090a6p-1f),
			Cplx(0x1.0c9704p-1f, -0x1.b3e4d4p-1f),
			Cplx(0x1.073878p-1f, -0x1.b72836p-1f),
			Cplx(0x1.01cfcap-1f, -0x1.ba5aa6p-1f),
			Cplx(0x1.f8ba4cp-2f, -0x1.bd7c0ap-1f),
			Cplx(0x1.edc192p-2f, -0x1.c08c44p-1f),
			Cplx(0x1.e2b5cep-2f, -0x1.c38b3p-1f),
			Cplx(0x1.d79776p-2f, -0x1.c678b4p-1f),
			Cplx(0x1.cc66e8p-2f, -0x1.c954b2p-1f),
			Cplx(0x1.c1249ap-2f, -0x1.cc1f1p-1f),
			Cplx(0x1.b5d102p-2f, -0x1.ced7bp-1f),
			Cplx(0x1.aa6c82p-2f, -0x1.d17e78p-1f),
			Cplx(0x1.9ef792p-2f, -0x1.d4134ep-1f),
			Cplx(0x1.9372ap-2f, -0x1.d69618p-1f),
			Cplx(0x1.87de2ap-2f, -0x1.d906bcp-1f),
			Cplx(0x1.7c3a9p-2f, -0x1.db6526p-1f),
			Cplx(0x1.70884ep-2f, -0x1.ddb13cp-1f),
			Cplx(0x1.64c7dep-2f, -0x1.dfeae6p-1f),
			Cplx(0x1.58f9a6p-2f, -0x1.e2121p-1f),
			Cplx(0x1.4d1e2p-2f, -0x1.e426a6p-1f),
			Cplx(0x1.4135c4p-2f, -0x1.e6289p-1f),
			Cplx(0x1.35410cp-2f, -0x1.e817bap-1f),
			Cplx(0x1.29406p-2f, -0x1.e9f416p-1f),
			Cplx(0x1.1d344p-2f, -0x1.ebbd8ep-1f),
			Cplx(0x1.111d26p-2f, -0x1.ed740ep-1f),
			Cplx(0x1.04fb8p-2f, -0x1.ef178ap-1f),
			Cplx(0x1.f19f9p-3f, -0x1.f0a7fp-1f),
			Cplx(0x1.d93502p-3f, -0x1.f2253p-1f),
			Cplx(0x1.c0b826p-3f, -0x1.f38f3ap-1f),
			Cplx(0x1.a829fcp-3f, -0x1.f4e604p-1f),
			Cplx(0x1.8f8b78p-3f, -0x1.f6297ep-1f),
			Cplx(0x1.76dd9ep-3f, -0x1.f7599ap-1f),
			Cplx(0x1.5e214p-3f, -0x1.f8765p-1f),
			Cplx(0x1.455762p-3f, -0x1.f97f92p-1f),
			Cplx(0x1.2c810ap-3f, -0x1.fa7558p-1f),
			Cplx(0x1.139f0ap-3f, -0x1.fb5798p-1f),
			Cplx(0x1.f564d8p-4f, -0x1.fc2648p-1f),
			Cplx(0x1.c37844p-4f, -0x1.fce16p-1f),
			Cplx(0x1.917a6ap-4f, -0x1.fd88dap-1f),
			Cplx(0x1.5f6cf6p-4f, -0x1.fe1cbp-1f),
			Cplx(0x1.2d51f6p-4f, -0x1.fe9cdap-1f),
			Cplx(0x1.f656eep-5f, -0x1.ff0956p-1f),
			Cplx(0x1.91f652p-5f, -0x1.ff621ep-1f),
			Cplx(0x1.2d8638p-5f, -0x1.ffa73p-1f),
			Cplx(0x1.9214fcp-6f, -0x1.ffd886p-1f),
			Cplx(0x1.921d0cp-7f, -0x1.fff622p-1f),
			Cplx(-0x1.777a5cp-25f, -0x1p+0f),
			Cplx(-0x1.921dc8p-7f, -0x1.fff622p-1f),
			Cplx(-0x1.92155ap-6f, -0x1.ffd886p-1f),
			Cplx(-0x1.2d8666p-5f, -0x1.ffa72ep-1f),
			Cplx(-0x1.91f682p-5f, -0x1.ff621ep-1f),
			Cplx(-0x1.f6571cp-5f, -0x1.ff0956p-1f),
			Cplx(-0x1.2d520cp-4f, -0x1.fe9cdap-1f),
			Cplx(-0x1.5f6d0ep-4f, -0x1.fe1cbp-1f),
			Cplx(-0x1.917a82p-4f, -0x1.fd88dap-1f),
			Cplx(-0x1.c3785cp-4f, -0x1.fce16p-1f),
			Cplx(-0x1.f564eep-4f, -0x1.fc2646p-1f),
			Cplx(-0x1.139f16p-3f, -0x1.fb5796p-1f),
			Cplx(-0x1.2c8114p-3f, -0x1.fa7558p-1f),
			Cplx(-0x1.45576ep-3f, -0x1.f97f92p-1f),
			Cplx(-0x1.5e214cp-3f, -0x1.f8765p-1f),
			Cplx(-0x1.76ddaap-3f, -0x1.f7599ap-1f),
			Cplx(-0x1.8f8b84p-3f, -0x1.f6297cp-1f),
			Cplx(-0x1.a82a08p-3f, -0x1.f4e604p-1f),
			Cplx(-0x1.c0b83p-3f, -0x1.f38f3ap-1f),
			Cplx(-0x1.d9350cp-3f, -0x1.f2252ep-1f),
			Cplx(-0x1.f19f9ap-3f, -0x1.f0a7fp-1f),
			Cplx(-0x1.04fb84p-2f, -0x1.ef178ap-1f),
			Cplx(-0x1.111d2cp-2f, -0x1.ed740ep-1f),
			Cplx(-0x1.1d3444p-2f, -0x1.ebbd8cp-1f),
			Cplx(-0x1.294066p-2f, -0x1.e9f414p-1f),
			Cplx(-0x1.354112p-2f, -0x1.e817bap-1f),
			Cplx(-0x1.4135c8p-2f, -0x1.e6288ep-1f),
			Cplx(-0x1.4d1e26p-2f, -0x1.e426a4p-1f),
			Cplx(-0x1.58f9acp-2f, -0x1.e2121p-1f),
			Cplx(-0x1.64c7e4p-2f, -0x1.dfeae4p-1f),
			Cplx(-0x1.708854p-2f, -0x1.ddb13cp-1f),
			Cplx(-0x1.7c3a96p-2f, -0x1.db6526p-1f),
			Cplx(-0x1.87de3p-2f, -0x1.d906bcp-1f),
			Cplx(-0x1.9372a6p-2f, -0x1.d69618p-1f),
			Cplx(-0x1.9ef796p-2f, -0x1.d4134cp-1f),
			Cplx(-0x1.aa6c8p-2f, -0x1.d17e78p-1f),
			Cplx(-0x1.b5d1p-2f, -0x1.ced7bp-1f),
			Cplx(-0x1.c1249ep-2f, -0x1.cc1f0ep-1f),
			Cplx(-0x1.cc66ecp-2f, -0x1.c954b2p-1f),
			Cplx(-0x1.d7977ap-2f, -0x1.c678b2p-1f),
			Cplx(-0x1.e2b5dap-2f, -0x1.c38b2ep-1f),
			Cplx(-0x1.edc19ep-2f, -0x1.c08c4p-1f),
			Cplx(-0x1.f8ba4cp-2f, -0x1.bd7c0cp-1f),
			Cplx(-0x1.01cfc8p-1f, -0x1.ba5aa6p-1f),
			Cplx(-0x1.07387ap-1f, -0x1.b72834p-1f),
			Cplx(-0x1.0c9706p-1f, -0x1.b3e4d2p-1f),
			Cplx(-0x1.11eb38p-1f, -0x1.b090a4p-1f),
			Cplx(-0x1.1734dap-1f, -0x1.ad2bc8p-1f),
			Cplx(-0x1.1c73b8p-1f, -0x1.a9b66p-1f),
			Cplx(-0x1.21a798p-1f, -0x1.a63092p-1f),
			Cplx(-0x1.26d054p-1f, -0x1.a29a7ap-1f),
			Cplx(-0x1.2bedb4p-1f, -0x1.9ef43ep-1f),
			Cplx(-0x1.30ff82p-1f, -0x1.9b3e04p-1f),
			Cplx(-0x1.36058ep-1f, -0x1.9777eep-1f),
			Cplx(-0x1.3affa6p-1f, -0x1.93a222p-1f),
			Cplx(-0x1.3fed9ap-1f, -0x1.8fbcc6p-1f),
			Cplx(-0x1.44cf32p-1f, -0x1.8bc808p-1f),
			Cplx(-0x1.49a44ap-1f, -0x1.87c4p-1f),
			Cplx(-0x1.4e6cacp-1f, -0x1.83b0ep-1f),
			Cplx(-0x1.53282cp-1f, -0x1.7f8eccp-1f),
			Cplx(-0x1.57d696p-1f, -0x1.7b5dfp-1f),
			Cplx(-0x1.5c77cp-1f, -0x1.771e72p-1f),
			Cplx(-0x1.610b7ap-1f, -0x1.72d08p-1f),
			Cplx(-0x1.659192p-1f, -0x1.6e7446p-1f),
			Cplx(-0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(-0x1.6e7446p-1f, -0x1.659192p-1f),
			Cplx(-0x1.72d086p-1f, -0x1.610b74p-1f),
			Cplx(-0x1.771e78p-1f, -0x1.5c77bap-1f),
			Cplx(-0x1.7b5df6p-1f, -0x1.57d69p-1f),
			Cplx(-0x1.7f8ed2p-1f, -0x1.532824p-1f),
			Cplx(-0x1.83b0ep-1f, -0x1.4e6cacp-1f),
			Cplx(-0x1.87c402p-1f, -0x1.49a44ap-1f),
			Cplx(-0x1.8bc808p-1f, -0x1.44cf32p-1f),
			Cplx(-0x1.8fbcccp-1f, -0x1.3fed94p-1f),
			Cplx(-0x1.93a226p-1f, -0x1.3affap-1f),
			Cplx(-0x1.9777f2p-1f, -0x1.360588p-1f),
			Cplx(-0x1.9b3e08p-1f, -0x1.30ff7ap-1f),
			Cplx(-0x1.9ef43ep-1f, -0x1.2bedb2p-1f),
			Cplx(-0x1.a29a7ap-1f, -0x1.26d054p-1f),
			Cplx(-0x1.a63092p-1f, -0x1.21a798p-1f),
			Cplx(-0x1.a9b664p-1f, -0x1.1c73b2p-1f),
			Cplx(-0x1.ad2bccp-1f, -0x1.1734d2p-1f),
			Cplx(-0x1.b090a8p-1f, -0x1.11eb3p-1f),
			Cplx(-0x1.b3e4d8p-1f, -0x1.0c97p-1f),
			Cplx(-0x1.b72834p-1f, -0x1.07387ap-1f),
			Cplx(-0x1.ba5aa6p-1f, -0x1.01cfc8p-1f),
			Cplx(-0x1.bd7c0cp-1f, -0x1.f8ba4ap-2f),
			Cplx(-0x1.c08c44p-1f, -0x1.edc19p-2f),
			Cplx(-0x1.c38b32p-1f, -0x1.e2b5ccp-2f),
			Cplx(-0x1.c678b6p-1f, -0x1.d7976cp-2f),
			Cplx(-0x1.c954b2p-1f, -0x1.cc66ecp-2f),
			Cplx(-0x1.cc1f1p-1f, -0x1.c1249ep-2f),
			Cplx(-0x1.ced7bp-1f, -0x1.b5d0fep-2f),
			Cplx(-0x1.d17e78p-1f, -0x1.aa6c7ep-2f),
			Cplx(-0x1.d4134ep-1f, -0x1.9ef78ep-2f),
			Cplx(-0x1.d69618p-1f, -0x1.93729ep-2f),
			Cplx(-0x1.d906cp-1f, -0x1.87de2p-2f),
			Cplx(-0x1.db6526p-1f, -0x1.7c3a96p-2f),
			Cplx(-0x1.ddb13cp-1f, -0x1.708854p-2f),
			Cplx(-0x1.dfeae6p-1f, -0x1.64c7dcp-2f),
			Cplx(-0x1.e21212p-1f, -0x1.58f9a4p-2f),
			Cplx(-0x1.e426a6p-1f, -0x1.4d1e1ep-2f),
			Cplx(-0x1.e6289p-1f, -0x1.4135cp-2f),
			Cplx(-0x1.e817bcp-1f, -0x1.354102p-2f),
			Cplx(-0x1.e9f416p-1f, -0x1.294066p-2f),
			Cplx(-0x1.ebbd8cp-1f, -0x1.1d3444p-2f),
			Cplx(-0x1.ed740ep-1f, -0x1.111d24p-2f),
			Cplx(-0x1.ef178ap-1f, -0x1.04fb7cp-2f),
			Cplx(-0x1.f0a7fp-1f, -0x1.f19f8ap-3f),
			Cplx(-0x1.f2253p-1f, -0x1.d934ecp-3f),
			Cplx(-0x1.f38f3cp-1f, -0x1.c0b81p-3f),
			Cplx(-0x1.f4e604p-1f, -0x1.a82a06p-3f),
			Cplx(-0x1.f6297ep-1f, -0x1.8f8b82p-3f),
			Cplx(-0x1.f7599ap-1f, -0x1.76dd98p-3f),
			Cplx(-0x1.f8765p-1f, -0x1.5e213ap-3f),
			Cplx(-0x1.f97f92p-1f, -0x1.45575cp-3f),
			Cplx(-0x1.fa7558p-1f, -0x1.2c80f4p-3f),
			Cplx(-0x1.fb5798p-1f, -0x1.139ef4p-3f),
			Cplx(-0x1.fc2646p-1f, -0x1.f564ecp-4f),
			Cplx(-0x1.fce16p-1f, -0x1.c3785ap-4f),
			Cplx(-0x1.fd88dap-1f, -0x1.917a6p-4f),
			Cplx(-0x1.fe1cbp-1f, -0x1.5f6ceap-4f),
			Cplx(-0x1.fe9cdcp-1f, -0x1.2d51eap-4f),
			Cplx(-0x1.ff0956p-1f, -0x1.f65696p-5f),
			Cplx(-0x1.ff621ep-1f, -0x1.91f5fap-5f),
			Cplx(-0x1.ffa72ep-1f, -0x1.2d866p-5f),
			Cplx(-0x1.ffd886p-1f, -0x1.92154cp-6f),
			Cplx(-0x1.fff622p-1f, -0x1.921caep-7f)};
	stage_9_counter :for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
		stage_9_total :for (ap_uint<32> jj = 0 ; jj < nFFT/512 ; jj++){
			stage_9_0 : for (ap_uint<10> i = 0 ; i < 512 ; i ++){
			#pragma HLS PIPELINE II=1
				In[i] = in.read();
			}

			stage_9_1 :for (ap_uint<9> ii = 0 ; ii < 256 ; ii++){
			#pragma HLS PIPELINE II=2
				Cplx a = In[ii];
				Cplx b = In[ii + 256] * Twiddle[ii];
				out.write(a+b);
				OutBuff[ii] = a-b ;
			}

			stage_9_2 :for (ap_uint<9> ii = 0 ; ii < 256 ; ii ++){
			#pragma HLS PIPELINE II=1
				out.write(OutBuff[ii]);
			}
		}
	}
}
void fft_stage_10(stream_cplx &in, stream_cplx &out, ap_uint<32> numFFT){
#pragma HLS INLINE off
//#pragma HLS PIPELINE
	Cplx In[1024];
#pragma HLS BIND_STORAGE variable=In type=RAM_T2P
#pragma HLS ARRAY_PARTITION variable=In type=block factor=2
	Cplx OutBuff[512];
#pragma HLS BIND_STORAGE variable=OutBuff type=RAM_T2P

	Cplx Twiddle[512] = {Cplx(0x1p+0f, -0x0p+0f),
			Cplx(0x1.fffd88p-1f, -0x1.921f1p-8f),
			Cplx(0x1.fff622p-1f, -0x1.921d2p-7f),
			Cplx(0x1.ffe9ccp-1f, -0x1.2d936cp-6f),
			Cplx(0x1.ffd886p-1f, -0x1.92156p-6f),
			Cplx(0x1.ffc252p-1f, -0x1.f69374p-6f),
			Cplx(0x1.ffa72ep-1f, -0x1.2d8658p-5f),
			Cplx(0x1.ff871ep-1f, -0x1.5fc00ep-5f),
			Cplx(0x1.ff621ep-1f, -0x1.91f66p-5f),
			Cplx(0x1.ff383p-1f, -0x1.c428d2p-5f),
			Cplx(0x1.ff0956p-1f, -0x1.f656eap-5f),
			Cplx(0x1.fed58ep-1f, -0x1.144014p-4f),
			Cplx(0x1.fe9cdap-1f, -0x1.2d520ap-4f),
			Cplx(0x1.fe5f3ap-1f, -0x1.466118p-4f),
			Cplx(0x1.fe1cbp-1f, -0x1.5f6d02p-4f),
			Cplx(0x1.fdd53ap-1f, -0x1.787586p-4f),
			Cplx(0x1.fd88dap-1f, -0x1.917a6cp-4f),
			Cplx(0x1.fd3792p-1f, -0x1.aa7b74p-4f),
			Cplx(0x1.fce16p-1f, -0x1.c3785cp-4f),
			Cplx(0x1.fc8646p-1f, -0x1.dc70eep-4f),
			Cplx(0x1.fc2648p-1f, -0x1.f564e6p-4f),
			Cplx(0x1.fbc162p-1f, -0x1.072a06p-3f),
			Cplx(0x1.fb5798p-1f, -0x1.139f0ep-3f),
			Cplx(0x1.fae8e8p-1f, -0x1.20116ep-3f),
			Cplx(0x1.fa7558p-1f, -0x1.2c8106p-3f),
			Cplx(0x1.f9fce6p-1f, -0x1.38edbcp-3f),
			Cplx(0x1.f97f92p-1f, -0x1.45576cp-3f),
			Cplx(0x1.f8fd6p-1f, -0x1.51bdfap-3f),
			Cplx(0x1.f8765p-1f, -0x1.5e2146p-3f),
			Cplx(0x1.f7ea62p-1f, -0x1.6a813p-3f),
			Cplx(0x1.f7599ap-1f, -0x1.76dd9ep-3f),
			Cplx(0x1.f6c3f8p-1f, -0x1.83367p-3f),
			Cplx(0x1.f6297cp-1f, -0x1.8f8b84p-3f),
			Cplx(0x1.f58a2cp-1f, -0x1.9bdccp-3f),
			Cplx(0x1.f4e604p-1f, -0x1.a82a04p-3f),
			Cplx(0x1.f43d08p-1f, -0x1.b4733p-3f),
			Cplx(0x1.f38f3ap-1f, -0x1.c0b826p-3f),
			Cplx(0x1.f2dc9cp-1f, -0x1.ccf8ccp-3f),
			Cplx(0x1.f2253p-1f, -0x1.d935p-3f),
			Cplx(0x1.f168f6p-1f, -0x1.e56ca4p-3f),
			Cplx(0x1.f0a7fp-1f, -0x1.f19f9ap-3f),
			Cplx(0x1.efe22p-1f, -0x1.fdcdc2p-3f),
			Cplx(0x1.ef178ap-1f, -0x1.04fb82p-2f),
			Cplx(0x1.ee482ep-1f, -0x1.0b0d9ep-2f),
			Cplx(0x1.ed740ep-1f, -0x1.111d28p-2f),
			Cplx(0x1.ec9b2ep-1f, -0x1.172a0ep-2f),
			Cplx(0x1.ebbd8cp-1f, -0x1.1d3444p-2f),
			Cplx(0x1.eadb2ep-1f, -0x1.233bbcp-2f),
			Cplx(0x1.e9f416p-1f, -0x1.294064p-2f),
			Cplx(0x1.e90844p-1f, -0x1.2f422ep-2f),
			Cplx(0x1.e817bap-1f, -0x1.35410cp-2f),
			Cplx(0x1.e7227ep-1f, -0x1.3b3cf2p-2f),
			Cplx(0x1.e6288ep-1f, -0x1.4135cap-2f),
			Cplx(0x1.e529fp-1f, -0x1.472b8ap-2f),
			Cplx(0x1.e426a4p-1f, -0x1.4d1e26p-2f),
			Cplx(0x1.e31eaep-1f, -0x1.530d88p-2f),
			Cplx(0x1.e2121p-1f, -0x1.58f9a8p-2f),
			Cplx(0x1.e100ccp-1f, -0x1.5ee274p-2f),
			Cplx(0x1.dfeae6p-1f, -0x1.64c7dep-2f),
			Cplx(0x1.ded06p-1f, -0x1.6aa9d8p-2f),
			Cplx(0x1.ddb13cp-1f, -0x1.708854p-2f),
			Cplx(0x1.dc8d7cp-1f, -0x1.766342p-2f),
			Cplx(0x1.db6526p-1f, -0x1.7c3a94p-2f),
			Cplx(0x1.da383ap-1f, -0x1.820e3cp-2f),
			Cplx(0x1.d906bcp-1f, -0x1.87de2cp-2f),
			Cplx(0x1.d7d0bp-1f, -0x1.8daa52p-2f),
			Cplx(0x1.d69618p-1f, -0x1.9372a8p-2f),
			Cplx(0x1.d556f6p-1f, -0x1.993716p-2f),
			Cplx(0x1.d4134cp-1f, -0x1.9ef796p-2f),
			Cplx(0x1.d2cb22p-1f, -0x1.a4b414p-2f),
			Cplx(0x1.d17e76p-1f, -0x1.aa6c84p-2f),
			Cplx(0x1.d02d5p-1f, -0x1.b020d8p-2f),
			Cplx(0x1.ced7bp-1f, -0x1.b5d1p-2f),
			Cplx(0x1.cd7d98p-1f, -0x1.bb7cf4p-2f),
			Cplx(0x1.cc1f1p-1f, -0x1.c1249ep-2f),
			Cplx(0x1.cabc16p-1f, -0x1.c6c7f6p-2f),
			Cplx(0x1.c954b2p-1f, -0x1.cc66eap-2f),
			Cplx(0x1.c7e8e6p-1f, -0x1.d2016ep-2f),
			Cplx(0x1.c678b4p-1f, -0x1.d79776p-2f),
			Cplx(0x1.c5042p-1f, -0x1.dd28f2p-2f),
			Cplx(0x1.c38b2ep-1f, -0x1.e2b5d4p-2f),
			Cplx(0x1.c20de4p-1f, -0x1.e83e1p-2f),
			Cplx(0x1.c08c42p-1f, -0x1.edc194p-2f),
			Cplx(0x1.bf064ep-1f, -0x1.f3405ap-2f),
			Cplx(0x1.bd7c0ap-1f, -0x1.f8ba5p-2f),
			Cplx(0x1.bbed7cp-1f, -0x1.fe2f64p-2f),
			Cplx(0x1.ba5aa6p-1f, -0x1.01cfc8p-1f),
			Cplx(0x1.b8c38cp-1f, -0x1.048564p-1f),
			Cplx(0x1.b72834p-1f, -0x1.07387ap-1f),
			Cplx(0x1.b588ap-1f, -0x1.09e908p-1f),
			Cplx(0x1.b3e4d4p-1f, -0x1.0c9706p-1f),
			Cplx(0x1.b23cd4p-1f, -0x1.0f426cp-1f),
			Cplx(0x1.b090a6p-1f, -0x1.11eb34p-1f),
			Cplx(0x1.aee04cp-1f, -0x1.14915cp-1f),
			Cplx(0x1.ad2bcap-1f, -0x1.1734d6p-1f),
			Cplx(0x1.ab7324p-1f, -0x1.19d5a2p-1f),
			Cplx(0x1.a9b662p-1f, -0x1.1c73b4p-1f),
			Cplx(0x1.a7f584p-1f, -0x1.1f0f0ap-1f),
			Cplx(0x1.a63092p-1f, -0x1.21a79ap-1f),
			Cplx(0x1.a4678cp-1f, -0x1.243d6p-1f),
			Cplx(0x1.a29a7ap-1f, -0x1.26d056p-1f),
			Cplx(0x1.a0c95ep-1f, -0x1.296074p-1f),
			Cplx(0x1.9ef43ep-1f, -0x1.2bedb4p-1f),
			Cplx(0x1.9d1b2p-1f, -0x1.2e780ep-1f),
			Cplx(0x1.9b3e04p-1f, -0x1.30ff8p-1f),
			Cplx(0x1.995cf2p-1f, -0x1.338402p-1f),
			Cplx(0x1.9777fp-1f, -0x1.36058ap-1f),
			Cplx(0x1.958efep-1f, -0x1.388418p-1f),
			Cplx(0x1.93a224p-1f, -0x1.3affa4p-1f),
			Cplx(0x1.91b168p-1f, -0x1.3d7824p-1f),
			Cplx(0x1.8fbccap-1f, -0x1.3fed96p-1f),
			Cplx(0x1.8dc452p-1f, -0x1.425ff2p-1f),
			Cplx(0x1.8bc806p-1f, -0x1.44cf34p-1f),
			Cplx(0x1.89c7eap-1f, -0x1.473b52p-1f),
			Cplx(0x1.87c4p-1f, -0x1.49a44ap-1f),
			Cplx(0x1.85bc5p-1f, -0x1.4c0a16p-1f),
			Cplx(0x1.83b0ep-1f, -0x1.4e6cacp-1f),
			Cplx(0x1.81a1b4p-1f, -0x1.50cc0ap-1f),
			Cplx(0x1.7f8ecep-1f, -0x1.53282ap-1f),
			Cplx(0x1.7d7836p-1f, -0x1.558104p-1f),
			Cplx(0x1.7b5df2p-1f, -0x1.57d694p-1f),
			Cplx(0x1.794006p-1f, -0x1.5a28d4p-1f),
			Cplx(0x1.771e76p-1f, -0x1.5c77bcp-1f),
			Cplx(0x1.74f948p-1f, -0x1.5ec34ap-1f),
			Cplx(0x1.72d084p-1f, -0x1.610b76p-1f),
			Cplx(0x1.70a42ap-1f, -0x1.63503ap-1f),
			Cplx(0x1.6e7444p-1f, -0x1.659194p-1f),
			Cplx(0x1.6c40d8p-1f, -0x1.67cf78p-1f),
			Cplx(0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(0x1.67cf78p-1f, -0x1.6c40d8p-1f),
			Cplx(0x1.659192p-1f, -0x1.6e7446p-1f),
			Cplx(0x1.63503ap-1f, -0x1.70a42cp-1f),
			Cplx(0x1.610b74p-1f, -0x1.72d084p-1f),
			Cplx(0x1.5ec348p-1f, -0x1.74f94ap-1f),
			Cplx(0x1.5c77bcp-1f, -0x1.771e76p-1f),
			Cplx(0x1.5a28d2p-1f, -0x1.794006p-1f),
			Cplx(0x1.57d692p-1f, -0x1.7b5df4p-1f),
			Cplx(0x1.558104p-1f, -0x1.7d7836p-1f),
			Cplx(0x1.532828p-1f, -0x1.7f8ecep-1f),
			Cplx(0x1.50cc0ap-1f, -0x1.81a1b4p-1f),
			Cplx(0x1.4e6caap-1f, -0x1.83b0e2p-1f),
			Cplx(0x1.4c0a14p-1f, -0x1.85bc52p-1f),
			Cplx(0x1.49a44ap-1f, -0x1.87c402p-1f),
			Cplx(0x1.473b5p-1f, -0x1.89c7eap-1f),
			Cplx(0x1.44cf32p-1f, -0x1.8bc806p-1f),
			Cplx(0x1.425ff2p-1f, -0x1.8dc454p-1f),
			Cplx(0x1.3fed94p-1f, -0x1.8fbcccp-1f),
			Cplx(0x1.3d7822p-1f, -0x1.91b168p-1f),
			Cplx(0x1.3affa2p-1f, -0x1.93a224p-1f),
			Cplx(0x1.388418p-1f, -0x1.958efep-1f),
			Cplx(0x1.36058ap-1f, -0x1.9777fp-1f),
			Cplx(0x1.3384p-1f, -0x1.995cf4p-1f),
			Cplx(0x1.30ff8p-1f, -0x1.9b3e04p-1f),
			Cplx(0x1.2e780ep-1f, -0x1.9d1b2p-1f),
			Cplx(0x1.2bedb2p-1f, -0x1.9ef43ep-1f),
			Cplx(0x1.296072p-1f, -0x1.a0c95ep-1f),
			Cplx(0x1.26d054p-1f, -0x1.a29a7ap-1f),
			Cplx(0x1.243d5ep-1f, -0x1.a4678ep-1f),
			Cplx(0x1.21a79ap-1f, -0x1.a63092p-1f),
			Cplx(0x1.1f0f08p-1f, -0x1.a7f586p-1f),
			Cplx(0x1.1c73b2p-1f, -0x1.a9b664p-1f),
			Cplx(0x1.19d5ap-1f, -0x1.ab7326p-1f),
			Cplx(0x1.1734d6p-1f, -0x1.ad2bcap-1f),
			Cplx(0x1.14915cp-1f, -0x1.aee04ap-1f),
			Cplx(0x1.11eb36p-1f, -0x1.b090a6p-1f),
			Cplx(0x1.0f426cp-1f, -0x1.b23cd4p-1f),
			Cplx(0x1.0c9704p-1f, -0x1.b3e4d4p-1f),
			Cplx(0x1.09e906p-1f, -0x1.b588ap-1f),
			Cplx(0x1.073878p-1f, -0x1.b72836p-1f),
			Cplx(0x1.04856p-1f, -0x1.b8c38ep-1f),
			Cplx(0x1.01cfcap-1f, -0x1.ba5aa6p-1f),
			Cplx(0x1.fe2f64p-2f, -0x1.bbed7cp-1f),
			Cplx(0x1.f8ba4cp-2f, -0x1.bd7c0ap-1f),
			Cplx(0x1.f34058p-2f, -0x1.bf064ep-1f),
			Cplx(0x1.edc192p-2f, -0x1.c08c44p-1f),
			Cplx(0x1.e83e0ap-2f, -0x1.c20de6p-1f),
			Cplx(0x1.e2b5cep-2f, -0x1.c38b3p-1f),
			Cplx(0x1.dd28f2p-2f, -0x1.c5042p-1f),
			Cplx(0x1.d79776p-2f, -0x1.c678b4p-1f),
			Cplx(0x1.d2016ep-2f, -0x1.c7e8e6p-1f),
			Cplx(0x1.cc66e8p-2f, -0x1.c954b2p-1f),
			Cplx(0x1.c6c7f2p-2f, -0x1.cabc18p-1f),
			Cplx(0x1.c1249ap-2f, -0x1.cc1f1p-1f),
			Cplx(0x1.bb7ceep-2f, -0x1.cd7d9ap-1f),
			Cplx(0x1.b5d102p-2f, -0x1.ced7bp-1f),
			Cplx(0x1.b020d6p-2f, -0x1.d02d5p-1f),
			Cplx(0x1.aa6c82p-2f, -0x1.d17e78p-1f),
			Cplx(0x1.a4b41p-2f, -0x1.d2cb22p-1f),
			Cplx(0x1.9ef792p-2f, -0x1.d4134ep-1f),
			Cplx(0x1.993712p-2f, -0x1.d556f6p-1f),
			Cplx(0x1.9372ap-2f, -0x1.d69618p-1f),
			Cplx(0x1.8daa54p-2f, -0x1.d7d0bp-1f),
			Cplx(0x1.87de2ap-2f, -0x1.d906bcp-1f),
			Cplx(0x1.820e3ap-2f, -0x1.da383ap-1f),
			Cplx(0x1.7c3a9p-2f, -0x1.db6526p-1f),
			Cplx(0x1.76633ep-2f, -0x1.dc8d7ep-1f),
			Cplx(0x1.70884ep-2f, -0x1.ddb13cp-1f),
			Cplx(0x1.6aa9d2p-2f, -0x1.ded06p-1f),
			Cplx(0x1.64c7dep-2f, -0x1.dfeae6p-1f),
			Cplx(0x1.5ee274p-2f, -0x1.e100ccp-1f),
			Cplx(0x1.58f9a6p-2f, -0x1.e2121p-1f),
			Cplx(0x1.530d86p-2f, -0x1.e31eaep-1f),
			Cplx(0x1.4d1e2p-2f, -0x1.e426a6p-1f),
			Cplx(0x1.472b86p-2f, -0x1.e529f2p-1f),
			Cplx(0x1.4135c4p-2f, -0x1.e6289p-1f),
			Cplx(0x1.3b3cfp-2f, -0x1.e7227ep-1f),
			Cplx(0x1.35410cp-2f, -0x1.e817bap-1f),
			Cplx(0x1.2f422cp-2f, -0x1.e90844p-1f),
			Cplx(0x1.29406p-2f, -0x1.e9f416p-1f),
			Cplx(0x1.233bb6p-2f, -0x1.eadb3p-1f),
			Cplx(0x1.1d344p-2f, -0x1.ebbd8ep-1f),
			Cplx(0x1.172a08p-2f, -0x1.ec9b2ep-1f),
			Cplx(0x1.111d26p-2f, -0x1.ed740ep-1f),
			Cplx(0x1.0b0d9cp-2f, -0x1.ee482ep-1f),
			Cplx(0x1.04fb8p-2f, -0x1.ef178ap-1f),
			Cplx(0x1.fdcdbcp-3f, -0x1.efe222p-1f),
			Cplx(0x1.f19f9p-3f, -0x1.f0a7fp-1f),
			Cplx(0x1.e56c98p-3f, -0x1.f168f6p-1f),
			Cplx(0x1.d93502p-3f, -0x1.f2253p-1f),
			Cplx(0x1.ccf8ccp-3f, -0x1.f2dc9cp-1f),
			Cplx(0x1.c0b826p-3f, -0x1.f38f3ap-1f),
			Cplx(0x1.b4732cp-3f, -0x1.f43d08p-1f),
			Cplx(0x1.a829fcp-3f, -0x1.f4e604p-1f),
			Cplx(0x1.9bdcb6p-3f, -0x1.f58a2cp-1f),
			Cplx(0x1.8f8b78p-3f, -0x1.f6297ep-1f),
			Cplx(0x1.833672p-3f, -0x1.f6c3f8p-1f),
			Cplx(0x1.76dd9ep-3f, -0x1.f7599ap-1f),
			Cplx(0x1.6a812ep-3f, -0x1.f7ea62p-1f),
			Cplx(0x1.5e214p-3f, -0x1.f8765p-1f),
			Cplx(0x1.51bdf2p-3f, -0x1.f8fd6p-1f),
			Cplx(0x1.455762p-3f, -0x1.f97f92p-1f),
			Cplx(0x1.38edbp-3f, -0x1.f9fce6p-1f),
			Cplx(0x1.2c810ap-3f, -0x1.fa7558p-1f),
			Cplx(0x1.20116ep-3f, -0x1.fae8e8p-1f),
			Cplx(0x1.139f0ap-3f, -0x1.fb5798p-1f),
			Cplx(0x1.072ap-3f, -0x1.fbc162p-1f),
			Cplx(0x1.f564d8p-4f, -0x1.fc2648p-1f),
			Cplx(0x1.dc70dap-4f, -0x1.fc8648p-1f),
			Cplx(0x1.c37844p-4f, -0x1.fce16p-1f),
			Cplx(0x1.aa7b76p-4f, -0x1.fd3792p-1f),
			Cplx(0x1.917a6ap-4f, -0x1.fd88dap-1f),
			Cplx(0x1.787582p-4f, -0x1.fdd53ap-1f),
			Cplx(0x1.5f6cf6p-4f, -0x1.fe1cbp-1f),
			Cplx(0x1.466108p-4f, -0x1.fe5f3cp-1f),
			Cplx(0x1.2d51f6p-4f, -0x1.fe9cdap-1f),
			Cplx(0x1.143ffcp-4f, -0x1.fed59p-1f),
			Cplx(0x1.f656eep-5f, -0x1.ff0956p-1f),
			Cplx(0x1.c428cep-5f, -0x1.ff383p-1f),
			Cplx(0x1.91f652p-5f, -0x1.ff621ep-1f),
			Cplx(0x1.5fbff8p-5f, -0x1.ff871ep-1f),
			Cplx(0x1.2d8638p-5f, -0x1.ffa73p-1f),
			Cplx(0x1.f69322p-6f, -0x1.ffc252p-1f),
			Cplx(0x1.9214fcp-6f, -0x1.ffd886p-1f),
			Cplx(0x1.2d9374p-6f, -0x1.ffe9ccp-1f),
			Cplx(0x1.921d0cp-7f, -0x1.fff622p-1f),
			Cplx(0x1.921e9ep-8f, -0x1.fffd88p-1f),
			Cplx(-0x1.777a5cp-25f, -0x1p+0f),
			Cplx(-0x1.922016p-8f, -0x1.fffd88p-1f),
			Cplx(-0x1.921dc8p-7f, -0x1.fff622p-1f),
			Cplx(-0x1.2d93d2p-6f, -0x1.ffe9ccp-1f),
			Cplx(-0x1.92155ap-6f, -0x1.ffd886p-1f),
			Cplx(-0x1.f6938p-6f, -0x1.ffc252p-1f),
			Cplx(-0x1.2d8666p-5f, -0x1.ffa72ep-1f),
			Cplx(-0x1.5fc026p-5f, -0x1.ff871ep-1f),
			Cplx(-0x1.91f682p-5f, -0x1.ff621ep-1f),
			Cplx(-0x1.c428fcp-5f, -0x1.ff383p-1f),
			Cplx(-0x1.f6571cp-5f, -0x1.ff0956p-1f),
			Cplx(-0x1.144012p-4f, -0x1.fed58ep-1f),
			Cplx(-0x1.2d520cp-4f, -0x1.fe9cdap-1f),
			Cplx(-0x1.46612p-4f, -0x1.fe5f3ap-1f),
			Cplx(-0x1.5f6d0ep-4f, -0x1.fe1cbp-1f),
			Cplx(-0x1.787598p-4f, -0x1.fdd53ap-1f),
			Cplx(-0x1.917a82p-4f, -0x1.fd88dap-1f),
			Cplx(-0x1.aa7b8ep-4f, -0x1.fd379p-1f),
			Cplx(-0x1.c3785cp-4f, -0x1.fce16p-1f),
			Cplx(-0x1.dc70f2p-4f, -0x1.fc8646p-1f),
			Cplx(-0x1.f564eep-4f, -0x1.fc2646p-1f),
			Cplx(-0x1.072a0cp-3f, -0x1.fbc162p-1f),
			Cplx(-0x1.139f16p-3f, -0x1.fb5796p-1f),
			Cplx(-0x1.201178p-3f, -0x1.fae8e8p-1f),
			Cplx(-0x1.2c8114p-3f, -0x1.fa7558p-1f),
			Cplx(-0x1.38edbcp-3f, -0x1.f9fce6p-1f),
			Cplx(-0x1.45576ep-3f, -0x1.f97f92p-1f),
			Cplx(-0x1.51bdfep-3f, -0x1.f8fd6p-1f),
			Cplx(-0x1.5e214cp-3f, -0x1.f8765p-1f),
			Cplx(-0x1.6a813ap-3f, -0x1.f7ea62p-1f),
			Cplx(-0x1.76ddaap-3f, -0x1.f7599ap-1f),
			Cplx(-0x1.83367cp-3f, -0x1.f6c3f8p-1f),
			Cplx(-0x1.8f8b84p-3f, -0x1.f6297cp-1f),
			Cplx(-0x1.9bdcc2p-3f, -0x1.f58a2ap-1f),
			Cplx(-0x1.a82a08p-3f, -0x1.f4e604p-1f),
			Cplx(-0x1.b47336p-3f, -0x1.f43d08p-1f),
			Cplx(-0x1.c0b83p-3f, -0x1.f38f3ap-1f),
			Cplx(-0x1.ccf8d8p-3f, -0x1.f2dc9cp-1f),
			Cplx(-0x1.d9350cp-3f, -0x1.f2252ep-1f),
			Cplx(-0x1.e56ca2p-3f, -0x1.f168f6p-1f),
			Cplx(-0x1.f19f9ap-3f, -0x1.f0a7fp-1f),
			Cplx(-0x1.fdcdc8p-3f, -0x1.efe22p-1f),
			Cplx(-0x1.04fb84p-2f, -0x1.ef178ap-1f),
			Cplx(-0x1.0b0da2p-2f, -0x1.ee482ep-1f),
			Cplx(-0x1.111d2cp-2f, -0x1.ed740ep-1f),
			Cplx(-0x1.172a0ep-2f, -0x1.ec9b2ep-1f),
			Cplx(-0x1.1d3444p-2f, -0x1.ebbd8cp-1f),
			Cplx(-0x1.233bbcp-2f, -0x1.eadb2ep-1f),
			Cplx(-0x1.294066p-2f, -0x1.e9f414p-1f),
			Cplx(-0x1.2f4232p-2f, -0x1.e90842p-1f),
			Cplx(-0x1.354112p-2f, -0x1.e817bap-1f),
			Cplx(-0x1.3b3cf6p-2f, -0x1.e7227cp-1f),
			Cplx(-0x1.4135c8p-2f, -0x1.e6288ep-1f),
			Cplx(-0x1.472b8cp-2f, -0x1.e529fp-1f),
			Cplx(-0x1.4d1e26p-2f, -0x1.e426a4p-1f),
			Cplx(-0x1.530d8cp-2f, -0x1.e31eaep-1f),
			Cplx(-0x1.58f9acp-2f, -0x1.e2121p-1f),
			Cplx(-0x1.5ee278p-2f, -0x1.e100ccp-1f),
			Cplx(-0x1.64c7e4p-2f, -0x1.dfeae4p-1f),
			Cplx(-0x1.6aa9d8p-2f, -0x1.ded06p-1f),
			Cplx(-0x1.708854p-2f, -0x1.ddb13cp-1f),
			Cplx(-0x1.766344p-2f, -0x1.dc8d7cp-1f),
			Cplx(-0x1.7c3a96p-2f, -0x1.db6526p-1f),
			Cplx(-0x1.820e4p-2f, -0x1.da383ap-1f),
			Cplx(-0x1.87de3p-2f, -0x1.d906bcp-1f),
			Cplx(-0x1.8daa5ap-2f, -0x1.d7d0aep-1f),
			Cplx(-0x1.9372a6p-2f, -0x1.d69618p-1f),
			Cplx(-0x1.993718p-2f, -0x1.d556f4p-1f),
			Cplx(-0x1.9ef796p-2f, -0x1.d4134cp-1f),
			Cplx(-0x1.a4b416p-2f, -0x1.d2cb22p-1f),
			Cplx(-0x1.aa6c8p-2f, -0x1.d17e78p-1f),
			Cplx(-0x1.b020d4p-2f, -0x1.d02d5p-1f),
			Cplx(-0x1.b5d1p-2f, -0x1.ced7bp-1f),
			Cplx(-0x1.bb7cf2p-2f, -0x1.cd7d98p-1f),
			Cplx(-0x1.c1249ep-2f, -0x1.cc1f0ep-1f),
			Cplx(-0x1.c6c7f6p-2f, -0x1.cabc16p-1f),
			Cplx(-0x1.cc66ecp-2f, -0x1.c954b2p-1f),
			Cplx(-0x1.d20172p-2f, -0x1.c7e8e4p-1f),
			Cplx(-0x1.d7977ap-2f, -0x1.c678b2p-1f),
			Cplx(-0x1.dd28f8p-2f, -0x1.c5041ep-1f),
			Cplx(-0x1.e2b5dap-2f, -0x1.c38b2ep-1f),
			Cplx(-0x1.e83e16p-2f, -0x1.c20de2p-1f),
			Cplx(-0x1.edc19ep-2f, -0x1.c08c4p-1f),
			Cplx(-0x1.f34064p-2f, -0x1.bf064cp-1f),
			Cplx(-0x1.f8ba4cp-2f, -0x1.bd7c0cp-1f),
			Cplx(-0x1.fe2f64p-2f, -0x1.bbed7cp-1f),
			Cplx(-0x1.01cfc8p-1f, -0x1.ba5aa6p-1f),
			Cplx(-0x1.048562p-1f, -0x1.b8c38ep-1f),
			Cplx(-0x1.07387ap-1f, -0x1.b72834p-1f),
			Cplx(-0x1.09e908p-1f, -0x1.b588ap-1f),
			Cplx(-0x1.0c9706p-1f, -0x1.b3e4d2p-1f),
			Cplx(-0x1.0f426ep-1f, -0x1.b23cd4p-1f),
			Cplx(-0x1.11eb38p-1f, -0x1.b090a4p-1f),
			Cplx(-0x1.14915ep-1f, -0x1.aee04ap-1f),
			Cplx(-0x1.1734dap-1f, -0x1.ad2bc8p-1f),
			Cplx(-0x1.19d5a4p-1f, -0x1.ab7322p-1f),
			Cplx(-0x1.1c73b8p-1f, -0x1.a9b66p-1f),
			Cplx(-0x1.1f0f08p-1f, -0x1.a7f586p-1f),
			Cplx(-0x1.21a798p-1f, -0x1.a63092p-1f),
			Cplx(-0x1.243d6p-1f, -0x1.a4678cp-1f),
			Cplx(-0x1.26d054p-1f, -0x1.a29a7ap-1f),
			Cplx(-0x1.296072p-1f, -0x1.a0c95ep-1f),
			Cplx(-0x1.2bedb4p-1f, -0x1.9ef43ep-1f),
			Cplx(-0x1.2e781p-1f, -0x1.9d1b1ep-1f),
			Cplx(-0x1.30ff82p-1f, -0x1.9b3e04p-1f),
			Cplx(-0x1.338404p-1f, -0x1.995cf2p-1f),
			Cplx(-0x1.36058ep-1f, -0x1.9777eep-1f),
			Cplx(-0x1.38841cp-1f, -0x1.958efcp-1f),
			Cplx(-0x1.3affa6p-1f, -0x1.93a222p-1f),
			Cplx(-0x1.3d7828p-1f, -0x1.91b164p-1f),
			Cplx(-0x1.3fed9ap-1f, -0x1.8fbcc6p-1f),
			Cplx(-0x1.425ffp-1f, -0x1.8dc454p-1f),
			Cplx(-0x1.44cf32p-1f, -0x1.8bc808p-1f),
			Cplx(-0x1.473b52p-1f, -0x1.89c7eap-1f),
			Cplx(-0x1.49a44ap-1f, -0x1.87c4p-1f),
			Cplx(-0x1.4c0a14p-1f, -0x1.85bc52p-1f),
			Cplx(-0x1.4e6cacp-1f, -0x1.83b0ep-1f),
			Cplx(-0x1.50cc0cp-1f, -0x1.81a1b2p-1f),
			Cplx(-0x1.53282cp-1f, -0x1.7f8eccp-1f),
			Cplx(-0x1.558106p-1f, -0x1.7d7834p-1f),
			Cplx(-0x1.57d696p-1f, -0x1.7b5dfp-1f),
			Cplx(-0x1.5a28d6p-1f, -0x1.794002p-1f),
			Cplx(-0x1.5c77cp-1f, -0x1.771e72p-1f),
			Cplx(-0x1.5ec34ep-1f, -0x1.74f946p-1f),
			Cplx(-0x1.610b7ap-1f, -0x1.72d08p-1f),
			Cplx(-0x1.63503ap-1f, -0x1.70a42cp-1f),
			Cplx(-0x1.659192p-1f, -0x1.6e7446p-1f),
			Cplx(-0x1.67cf78p-1f, -0x1.6c40d8p-1f),
			Cplx(-0x1.6a09e6p-1f, -0x1.6a09e6p-1f),
			Cplx(-0x1.6c40d8p-1f, -0x1.67cf78p-1f),
			Cplx(-0x1.6e7446p-1f, -0x1.659192p-1f),
			Cplx(-0x1.70a42cp-1f, -0x1.635038p-1f),
			Cplx(-0x1.72d086p-1f, -0x1.610b74p-1f),
			Cplx(-0x1.74f94ap-1f, -0x1.5ec348p-1f),
			Cplx(-0x1.771e78p-1f, -0x1.5c77bap-1f),
			Cplx(-0x1.794008p-1f, -0x1.5a28dp-1f),
			Cplx(-0x1.7b5df6p-1f, -0x1.57d69p-1f),
			Cplx(-0x1.7d783ap-1f, -0x1.5581p-1f),
			Cplx(-0x1.7f8ed2p-1f, -0x1.532824p-1f),
			Cplx(-0x1.81a1b2p-1f, -0x1.50cc0cp-1f),
			Cplx(-0x1.83b0ep-1f, -0x1.4e6cacp-1f),
			Cplx(-0x1.85bc52p-1f, -0x1.4c0a14p-1f),
			Cplx(-0x1.87c402p-1f, -0x1.49a44ap-1f),
			Cplx(-0x1.89c7eap-1f, -0x1.473b52p-1f),
			Cplx(-0x1.8bc808p-1f, -0x1.44cf32p-1f),
			Cplx(-0x1.8dc454p-1f, -0x1.425ffp-1f),
			Cplx(-0x1.8fbcccp-1f, -0x1.3fed94p-1f),
			Cplx(-0x1.91b16ap-1f, -0x1.3d782p-1f),
			Cplx(-0x1.93a226p-1f, -0x1.3affap-1f),
			Cplx(-0x1.958f02p-1f, -0x1.388414p-1f),
			Cplx(-0x1.9777f2p-1f, -0x1.360588p-1f),
			Cplx(-0x1.995cf6p-1f, -0x1.3383fcp-1f),
			Cplx(-0x1.9b3e08p-1f, -0x1.30ff7ap-1f),
			Cplx(-0x1.9d1b1ep-1f, -0x1.2e781p-1f),
			Cplx(-0x1.9ef43ep-1f, -0x1.2bedb2p-1f),
			Cplx(-0x1.a0c95ep-1f, -0x1.296072p-1f),
			Cplx(-0x1.a29a7ap-1f, -0x1.26d054p-1f),
			Cplx(-0x1.a4678ep-1f, -0x1.243d5ep-1f),
			Cplx(-0x1.a63092p-1f, -0x1.21a798p-1f),
			Cplx(-0x1.a7f586p-1f, -0x1.1f0f06p-1f),
			Cplx(-0x1.a9b664p-1f, -0x1.1c73b2p-1f),
			Cplx(-0x1.ab7328p-1f, -0x1.19d59ep-1f),
			Cplx(-0x1.ad2bccp-1f, -0x1.1734d2p-1f),
			Cplx(-0x1.aee04ep-1f, -0x1.149156p-1f),
			Cplx(-0x1.b090a8p-1f, -0x1.11eb3p-1f),
			Cplx(-0x1.b23cd8p-1f, -0x1.0f4266p-1f),
			Cplx(-0x1.b3e4d8p-1f, -0x1.0c97p-1f),
			Cplx(-0x1.b588ap-1f, -0x1.09e908p-1f),
			Cplx(-0x1.b72834p-1f, -0x1.07387ap-1f),
			Cplx(-0x1.b8c38ep-1f, -0x1.048562p-1f),
			Cplx(-0x1.ba5aa6p-1f, -0x1.01cfc8p-1f),
			Cplx(-0x1.bbed7cp-1f, -0x1.fe2f62p-2f),
			Cplx(-0x1.bd7c0cp-1f, -0x1.f8ba4ap-2f),
			Cplx(-0x1.bf065p-1f, -0x1.f34054p-2f),
			Cplx(-0x1.c08c44p-1f, -0x1.edc19p-2f),
			Cplx(-0x1.c20de6p-1f, -0x1.e83e08p-2f),
			Cplx(-0x1.c38b32p-1f, -0x1.e2b5ccp-2f),
			Cplx(-0x1.c50422p-1f, -0x1.dd28e8p-2f),
			Cplx(-0x1.c678b6p-1f, -0x1.d7976cp-2f),
			Cplx(-0x1.c7e8e8p-1f, -0x1.d20164p-2f),
			Cplx(-0x1.c954b2p-1f, -0x1.cc66ecp-2f),
			Cplx(-0x1.cabc16p-1f, -0x1.c6c7f6p-2f),
			Cplx(-0x1.cc1f1p-1f, -0x1.c1249ep-2f),
			Cplx(-0x1.cd7d98p-1f, -0x1.bb7cf2p-2f),
			Cplx(-0x1.ced7bp-1f, -0x1.b5d0fep-2f),
			Cplx(-0x1.d02d5p-1f, -0x1.b020d4p-2f),
			Cplx(-0x1.d17e78p-1f, -0x1.aa6c7ep-2f),
			Cplx(-0x1.d2cb24p-1f, -0x1.a4b40ep-2f),
			Cplx(-0x1.d4134ep-1f, -0x1.9ef78ep-2f),
			Cplx(-0x1.d556f6p-1f, -0x1.99371p-2f),
			Cplx(-0x1.d69618p-1f, -0x1.93729ep-2f),
			Cplx(-0x1.d7d0b2p-1f, -0x1.8daa4ap-2f),
			Cplx(-0x1.d906cp-1f, -0x1.87de2p-2f),
			Cplx(-0x1.da383cp-1f, -0x1.820e3p-2f),
			Cplx(-0x1.db6526p-1f, -0x1.7c3a96p-2f),
			Cplx(-0x1.dc8d7cp-1f, -0x1.766342p-2f),
			Cplx(-0x1.ddb13cp-1f, -0x1.708854p-2f),
			Cplx(-0x1.ded06p-1f, -0x1.6aa9d8p-2f),
			Cplx(-0x1.dfeae6p-1f, -0x1.64c7dcp-2f),
			Cplx(-0x1.e100cep-1f, -0x1.5ee27p-2f),
			Cplx(-0x1.e21212p-1f, -0x1.58f9a4p-2f),
			Cplx(-0x1.e31ebp-1f, -0x1.530d82p-2f),
			Cplx(-0x1.e426a6p-1f, -0x1.4d1e1ep-2f),
			Cplx(-0x1.e529f2p-1f, -0x1.472b82p-2f),
			Cplx(-0x1.e6289p-1f, -0x1.4135cp-2f),
			Cplx(-0x1.e7228p-1f, -0x1.3b3ce6p-2f),
			Cplx(-0x1.e817bcp-1f, -0x1.354102p-2f),
			Cplx(-0x1.e90846p-1f, -0x1.2f4222p-2f),
			Cplx(-0x1.e9f416p-1f, -0x1.294066p-2f),
			Cplx(-0x1.eadb2ep-1f, -0x1.233bbcp-2f),
			Cplx(-0x1.ebbd8cp-1f, -0x1.1d3444p-2f),
			Cplx(-0x1.ec9b2ep-1f, -0x1.172a0cp-2f),
			Cplx(-0x1.ed740ep-1f, -0x1.111d24p-2f),
			Cplx(-0x1.ee482ep-1f, -0x1.0b0d9ap-2f),
			Cplx(-0x1.ef178ap-1f, -0x1.04fb7cp-2f),
			Cplx(-0x1.efe222p-1f, -0x1.fdcdb6p-3f),
			Cplx(-0x1.f0a7fp-1f, -0x1.f19f8ap-3f),
			Cplx(-0x1.f168f6p-1f, -0x1.e56c92p-3f),
			Cplx(-0x1.f2253p-1f, -0x1.d934ecp-3f),
			Cplx(-0x1.f2dc9ep-1f, -0x1.ccf8b6p-3f),
			Cplx(-0x1.f38f3cp-1f, -0x1.c0b81p-3f),
			Cplx(-0x1.f43d0ap-1f, -0x1.b47316p-3f),
			Cplx(-0x1.f4e604p-1f, -0x1.a82a06p-3f),
			Cplx(-0x1.f58a2cp-1f, -0x1.9bdccp-3f),
			Cplx(-0x1.f6297ep-1f, -0x1.8f8b82p-3f),
			Cplx(-0x1.f6c3f8p-1f, -0x1.83366cp-3f),
			Cplx(-0x1.f7599ap-1f, -0x1.76dd98p-3f),
			Cplx(-0x1.f7ea62p-1f, -0x1.6a8128p-3f),
			Cplx(-0x1.f8765p-1f, -0x1.5e213ap-3f),
			Cplx(-0x1.f8fd6p-1f, -0x1.51bdecp-3f),
			Cplx(-0x1.f97f92p-1f, -0x1.45575cp-3f),
			Cplx(-0x1.f9fce6p-1f, -0x1.38edaap-3f),
			Cplx(-0x1.fa7558p-1f, -0x1.2c80f4p-3f),
			Cplx(-0x1.fae8eap-1f, -0x1.201158p-3f),
			Cplx(-0x1.fb5798p-1f, -0x1.139ef4p-3f),
			Cplx(-0x1.fbc162p-1f, -0x1.0729eap-3f),
			Cplx(-0x1.fc2646p-1f, -0x1.f564ecp-4f),
			Cplx(-0x1.fc8646p-1f, -0x1.dc70eep-4f),
			Cplx(-0x1.fce16p-1f, -0x1.c3785ap-4f),
			Cplx(-0x1.fd3792p-1f, -0x1.aa7b6ap-4f),
			Cplx(-0x1.fd88dap-1f, -0x1.917a6p-4f),
			Cplx(-0x1.fdd53ap-1f, -0x1.787576p-4f),
			Cplx(-0x1.fe1cbp-1f, -0x1.5f6ceap-4f),
			Cplx(-0x1.fe5f3cp-1f, -0x1.4660fcp-4f),
			Cplx(-0x1.fe9cdcp-1f, -0x1.2d51eap-4f),
			Cplx(-0x1.fed59p-1f, -0x1.143ffp-4f),
			Cplx(-0x1.ff0956p-1f, -0x1.f65696p-5f),
			Cplx(-0x1.ff3832p-1f, -0x1.c42876p-5f),
			Cplx(-0x1.ff621ep-1f, -0x1.91f5fap-5f),
			Cplx(-0x1.ff871ep-1f, -0x1.5fbfap-5f),
			Cplx(-0x1.ffa72ep-1f, -0x1.2d866p-5f),
			Cplx(-0x1.ffc252p-1f, -0x1.f69372p-6f),
			Cplx(-0x1.ffd886p-1f, -0x1.92154cp-6f),
			Cplx(-0x1.ffe9ccp-1f, -0x1.2d9346p-6f),
			Cplx(-0x1.fff622p-1f, -0x1.921caep-7f),
			Cplx(-0x1.fffd88p-1f, -0x1.921de4p-8f)};
	stage_10_counter :for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
	#pragma HLS LOOP_TRIPCOUNT avg= 5
		stage_10_0 : for (ap_uint<11> i = 0 ; i < 1024 ; i ++){
		#pragma HLS PIPELINE II=1
			In[i] = in.read();
		}

		stage_10_1 :for (ap_uint<10> ii = 0 ; ii < 512 ; ii++){
		#pragma HLS PIPELINE II=2
			Cplx a = In[ii];
			Cplx b = In[ii + 512] * Twiddle[ii];
			out.write(a+b);
			OutBuff[ii] = a-b ;
		}

		stage_10_2 :for (ap_uint<10> ii = 0 ; ii < 512 ; ii ++){
		#pragma HLS PIPELINE II=1
			out.write(OutBuff[ii]);
		}
	}
}

void retrieve_nfft_value(stream_cplx &in, axi_cplx* out, ap_uint<32> numFFT){
#pragma HLS INLINE off

	retrieve_counter :for (ap_uint<32> counter = 0 ; counter < numFFT ; counter ++){
		retrieve_total :for (ap_uint<32> i = 0 ; i < nFFT ; i++){
		#pragma HLS PIPELINE II=1
			Cplx temp = in.read();
			axi_cplx retour;
            ap_uint<32> re_uint = bitcast_float_32(temp.real());
            ap_uint<32> im_uint = bitcast_float_32(temp.imag());
            retour.range(31, 0) = re_uint;
            retour.range(63,32) = im_uint;
			out[i+counter*nFFT] = retour;
		}
	}
}






