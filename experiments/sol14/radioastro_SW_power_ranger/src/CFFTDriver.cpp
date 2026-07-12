#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include <stdio.h>
#include <sys/mman.h>


#include <string.h>

#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "utils.h"
#include "CAccelDriver.hpp"
#include "CFFTDriver.hpp"

uint32_t CFFT::FFT_HW(void *input, void * output, uint32_t numFFT)
{
  
  uint32_t phyInput, phyOutput ;
  uint32_t status;

  if (logging)
    printf("CFFT::FFT_HW(Input=0x%08X, Output=0x%08X, numFFT=0x%08X\n", 
          (uint32_t)input, (uint32_t)output, (uint32_t)numFFT);

  if (driver == 0) {
    if (logging)
      printf("Error: Calling Add() on a non-initialized accelerator.\n");
    return DEVICE_NOT_INITIALIZED;
  }

  // We need to obtain the physical addresses corresponding to each of the virtual addresses passed by the application.
  // The accelerator uses only the physical addresses (and only contiguous memory).
  phyInput = GetDMAPhysicalAddr(input);
  if (phyInput == 0) {
    if (logging)
      printf("Error: No physical address found for virtual address 0x%08X\n", (uint32_t)input);
    return VIRT_ADDR_NOT_FOUND;
  }
  phyOutput = GetDMAPhysicalAddr(output);
  if (phyOutput == 0) {
    if (logging)
      printf("Error: No physical address found for virtual address 0x%08X\n", (uint32_t)output);
    return VIRT_ADDR_NOT_FOUND;
  }

  struct user_message message = {(uint32_t)phyInput, (uint32_t)phyOutput, numFFT};



  if (logging)
    printf("\nStarting accel...\n");


  int32_t readBytes = read(driver, (void *)&message, sizeof(message));
  if (readBytes != 0)
    printf("Warning! Read %d bytes instead than %d\n", readBytes, 0);


  return OK;
}

