#ifndef CFFT_HPP
#define CFFT_HPP

#include "CAccelDriver.hpp"

class CFFT : public CAccelDriver {
  protected:
    // Structure that mimics the layout of the peripheral registers.
    // Vitis HLS skips some addresses in the register file. We introduce
    // padding fields to create the right mapping to registers with our structure,
    /** @ */
  struct user_message {
    uint32_t input;
    uint32_t output;
  
    uint32_t numFFT;

  };

  public:
    CFFT(bool Logging = false)
      : CAccelDriver(Logging) {}

    ~CFFT() {}

    uint32_t FFT_HW(void *input, void * output, uint32_t numFFT);
};

#endif  // CCONV2D_HPP

