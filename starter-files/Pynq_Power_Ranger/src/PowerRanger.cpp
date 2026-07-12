// Test of the Pynq Power Ranger.
// Load the bitstream, load this application, and start any workload on the ARM cores.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <locale.h>
#include <unistd.h>
#include <pthread.h>
#include <map>
#include "util.hpp"

#include "CAccelProxy.hpp"
#include "CXADCProxy.hpp"

///////////////////////////////////////
// Address constants
const uint32_t MAP_SIZE = 64*1024; // Size of address range mapped to the adder registers
const uint32_t XADC_HW_ADDR = 0x43C00000; // From Vivado's address editor


///////////////////////////////////////////////////////////////////////////////
bool InitDevice(CXADCProxy & xadc, bool log=true)
{
  printf("\n\nThis program requires that the bitstream is loaded in the FPGA.\n");
  printf("Press ENTER to confirm that the bitstream is loaded (proceeding without it can crash the board).\n\n");
  getchar(); // Commented to allow execution in batch

  if ( xadc.Open(XADC_HW_ADDR, MAP_SIZE) != CAccelProxy::OK ) {
    printf("Error mapping device at physical address 0x%08X\n", XADC_HW_ADDR);
    return false;
  }
  if (log)
    printf("Device at physical address 0x%08X successfully mapped into the application virtual address space\n\n",
            XADC_HW_ADDR);

  return true;
}


///////////////////////////////////////////////////////////////////////////////
int main(int argc, char ** argv)
{
  double prevEnergy = 0.0, currentEnergy = 0.0;
  
  // Verify that we are running with sudo.
  if (geteuid()) {
    printf("\n\n###### This program has to be run with sudo!!! ######\n\n\n");
    return -1;
  }

  // Obtain arguments from command line.
  setlocale(LC_NUMERIC, "en_US.utf8");  // Enables printing human-readable numbers with %'

  CXADCProxy xadc(false); // Deactivate logging
  if (!InitDevice(xadc))
    return -1;

  xadc.StartMeasurements();

  printf("Press CTRL-C to stop...\n");

  while (1) {
    const uint32_t barLength = 40;
    char bar[barLength+1];
    float power;

    currentEnergy = xadc.GetEnergy(&power);
    uint32_t level = (power / xadc.GetMaxPower()) * barLength;
    if (level > barLength) level = barLength;
    for (uint32_t ii = 0; ii < level; ++ ii)
      bar[ii] = '#';
    for (uint32_t ii = level; ii < barLength; ++ ii)
      bar[ii] = '.';
    bar[barLength] = 0;

    printf("%s %0.9lf W --> %0.9lf J (+ %0.9lf J)\n", bar, power, currentEnergy, currentEnergy - prevEnergy);
    prevEnergy = currentEnergy;
    usleep(1000000); // 1 second
  }

  xadc.StopMeasurements();
  return 0;
}

