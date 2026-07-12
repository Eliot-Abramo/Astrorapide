#ifndef FXP_UTILS_H
#define FXP_UTILS_H

#include <inttypes.h>
#include <stdint.h>
#include <iostream>
#include <algorithm>
#include <math.h> // for roundf()

const uint32_t DECIMALS = 22;
typedef int32_t TFXP;     // Parameters and activations
typedef int64_t TFXP_MULT;// Intermmediate results of multiplications
typedef int64_t TFXP_ACC; // Moving average accumulator

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

inline TFXP Float2Fxp(float value, uint32_t decimalBits = DECIMALS) {
  // Échelle en int64 pour éviter l'overflow lors de la conversion
  int64_t scaled = static_cast<int64_t>(std::roundf(value * (1ULL << decimalBits)));
  // Clamp à la plage int32
  if (scaled > INT32_MAX) scaled = INT32_MAX;
  if (scaled < INT32_MIN) scaled = INT32_MIN;
  return static_cast<TFXP>(scaled);
}

inline float Fxp2Float(TFXP value, uint32_t decimalBits = DECIMALS) {
  return static_cast<float>(value) / static_cast<float>(1ULL << decimalBits);
}

inline TFXP FXP_Mult(TFXP a, TFXP b, uint32_t decimalBits = DECIMALS)
{
  //return a*b;
  TFXP_MULT res = (TFXP_MULT)a * (TFXP_MULT)b;
  res = res >> decimalBits;
  return res;
}

#endif