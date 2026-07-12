#ifndef CFFT_HPP
#define CFFT_HPP

#include "CAccelProxy.hpp"

class CFFT : public CAccelProxy {
  protected:
    // Structure that mimics the layout of the peripheral registers.
    // Vitis HLS skips some addresses in the register file. We introduce
    // padding fields to create the right mapping to registers with our structure,
    /** @todo */
    struct TRegs {
      uint32_t control; // 0x00
      uint32_t gier, ier, isr; // 0x04, 0x08, 0x0C
      uint32_t input ; // 0x10
      uint32_t input_h; // 0x14
      uint32_t padding0; // 0x18
      uint32_t output; // 0x1C
      uint32_t output_h; // 0x20
      uint32_t padding1; // 0x24

      uint32_t numFFT; // 0x28
      uint32_t padding2; // 0x2C
    };

  public:
    CFFT(bool Logging = false)
      : CAccelProxy(Logging) {}

    ~CFFT() {}

    uint32_t FFT_HW(void *input, void * output, uint32_t numFFT);
};

#endif  // CCONV2D_HPP

