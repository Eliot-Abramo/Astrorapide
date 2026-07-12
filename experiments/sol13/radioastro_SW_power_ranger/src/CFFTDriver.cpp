#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include "utils.h"
#include "CAccelProxy.hpp"
#include "CFFTDriver.hpp"

uint32_t CFFT::FFT_HW(void *input, void * output, uint32_t numFFT)
{
  volatile TRegs * regs = (TRegs*)accelRegs;
  uint32_t phyInput, phyOutput ;
  uint32_t status;

  if (logging)
    printf("CFFT::FFT_HW(Input=0x%08X, Output=0x%08X, numFFT=0x%08X\n", 
          (uint32_t)input, (uint32_t)output, (uint32_t)numFFT);

  if (accelRegs == NULL) {
    if (logging)
      printf("Error: Calling FFT_HW() on a non-initialized accelerator.\n");
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


  // Write to registers (todo)
  //printf("You were supposed to add your code here...\n"); /* Program accel regs @todo */
  regs->input = (uint32_t)phyInput;
  regs->output = (uint32_t)phyOutput;
  regs->numFFT = numFFT;
  //printf("Before launchin accelerator %d \n",test);


  if (logging)
    printf("\nStarting accel...\n");

  // Send start command to the accel
  //printf("You were supposed to add your code here...\n"); /* Send start command @todo */
  status = regs->control;
  status |= 1;  // Set to 1 ap_start
  regs->control = status;
  // Wait for done signal from the accel
  do {
    //printf("During %d \n",test);
    status = regs->control;
  } while ( ( (status & 2) != 2) ); // wait until ap_done==1
  //printf("You were supposed to add your code here...\n"); /* Wait for done signal @todo */

  return OK;
}

