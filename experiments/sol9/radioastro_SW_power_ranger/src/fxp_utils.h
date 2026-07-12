#ifndef FXP_UTILS_H
#define FXP_UTILS_H

#include <inttypes.h>
#include <stdint.h>
#include <iostream>
#include <algorithm>
#include <math.h> // for roundf()

const uint32_t DECIMALS = 8;
typedef int16_t TFXP;     // Parameters and activations
typedef int32_t TFXP_MULT;// Intermmediate results of multiplications
typedef int32_t TFXP_ACC; // Moving average accumulator

template<typename T>
inline void PrintDynamicRange(T* vec, size_t size) {
  if (vec == nullptr) {
    std::cout << "Vector is empty. No dynamic range to display." << std::endl;
    return;
  }

  T minVal = vec[0];
  T maxVal = vec[0];
  for (size_t i = 0; i < size; ++i) {
    if (vec[i] < minVal) minVal = vec[i];
    if (vec[i] > maxVal) maxVal = vec[i];
  }
  std::cout << "Dynamic Range: Min = " << minVal << ", Max = " << maxVal << ", Range = " << maxVal - minVal << std::endl;
}

inline TFXP Float2Fxp(float value, uint32_t decimalBits = DECIMALS)
{
  //return value;
  int32_t scaled = (int32_t)roundf(value * ((uint64_t)1 << decimalBits));
    
  // Clamp to int16_t range
  if (scaled > INT16_MAX) scaled = INT16_MAX;
  if (scaled < INT16_MIN) scaled = INT16_MIN;

  return (int16_t)scaled;
}

inline float Fxp2Float(TFXP value, uint32_t decimalBits = DECIMALS)
{
  //return value;
  return ((value) / (float)((uint64_t)1 << (decimalBits)));
}

inline TFXP FXP_Mult(TFXP a, TFXP b, uint32_t decimalBits = DECIMALS)
{
  //return a*b;
  TFXP_MULT res = (TFXP_MULT)a * (TFXP_MULT)b;
  res = res >> decimalBits;
  return res;
}

#endif