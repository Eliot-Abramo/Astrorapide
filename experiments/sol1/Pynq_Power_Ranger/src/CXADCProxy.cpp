#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <map>
#include "util.hpp"
#include "CAccelProxy.hpp"
#include "CXADCProxy.hpp"

///////////////////////////////////////////////////////////////////////////////
//         Threading control                                                  /
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
CXADCProxy::CXADCProxy(bool Logging, const char * ThreadName, uint32_t CPU)
  : CAccelProxy(Logging),
  threadID(-1), cpuNum(CPU), stopping(false), stoppingACK(true),
  accEnergy(0.0), lastPower(0.0)
{
  uint32_t nameLength;

  if (ThreadName != NULL) {
    nameLength = strlen(ThreadName) + 1;
    threadName = (char*)malloc(nameLength);
    if (threadName != NULL)
      strncpy(threadName, ThreadName, nameLength);
  }
  else {
    threadName = (char*)malloc(256);
    if (threadName != NULL)
      strncpy(threadName, "XADC_THREAD", 255);
  }
}


///////////////////////////////////////////////////////////////////////////////
CXADCProxy::~CXADCProxy()
{
  if (!stoppingACK)
    StopMeasurements();
  if (threadName != NULL) {
    free(threadName);
    threadName = NULL;
  }
}


///////////////////////////////////////////////////////////////////////////////
// This function is the entry point for new threads. It does not receive a this pointer.
// Therefore, we use the only argument as a pointer to an object of our class and call
// its Main() function from this new thread.
void * CXADCProxy::NewThread(void * objectPointer)
{
	((CXADCProxy *)objectPointer)->Main();
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//         XADC interaction                                                   /
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Start measuring (accumulating) energy in a new thread.
bool CXADCProxy::StartMeasurements()
{
	int res;
	pthread_attr_t attributes;

  stoppingACK = false;
  accEnergy = 0.0;

	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE);
	res = pthread_create((pthread_t*)&threadID, &attributes, NewThread, (void *)this);
	// The new thread goes to static NewThread() and then to
	// the Main() of the derived class. Here, just exit
	// and return to the caller.

	pthread_attr_destroy(&attributes);

  if (cpuNum != UINT32_MAX) {
  	cpu_set_t cpuset;
	  CPU_ZERO(&cpuset);
    CPU_SET(cpuNum, &cpuset);
	  pthread_setaffinity_np(threadID, sizeof(cpuset), &cpuset);
  }

	if (threadName != NULL)
		pthread_setname_np(threadID, threadName);

	return !res;
}

///////////////////////////////////////////////////////////////////////////////
// Stop acquiring power/energy samples. Exits the worker thread.
// This function blocks (sleeps) until the worker thread executes and reads
// the stopping flag.
bool CXADCProxy::StopMeasurements()
{
	int res = -1;

	if (!stoppingACK) {
		stopping = true;
		res = pthread_join(threadID, NULL); // Blocking!
	}
	return (res == 0);
}


///////////////////////////////////////////////////////////////////////////////
// This function runs in a separate thread, polling the XADC every ~10 ms.
bool CXADCProxy::Main()
{
  struct timespec before, now;

  if (logging)
	  printf("CXADCProxy entering Thread: %lu - \"%s\"\n", threadID, threadName);

  ResetXADC();
  clock_gettime(CLOCK_MONOTONIC_RAW, &before);
  usleep(SAMPLING_PERIOD_us);

	while (!stopping) {
    double power;

    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    power = GetPower();
    lastPower = (float)power; // Update it here so users can get also the power.

    accEnergy += power * CalcTimeDiff(now, before) / 1e9;
    before = now;
    usleep(SAMPLING_PERIOD_us);
	}

	stoppingACK = true;
  if (logging)
	  printf("CXADCProxy exiting Thread: %lu - \"%s\"\n", threadID, threadName);
	return true;
}


///////////////////////////////////////////////////////////////////////////////
void CXADCProxy::ResetXADC()
{
  volatile uint32_t * regs = (volatile uint32_t *)accelRegs;

  if (logging)
    printf("CXADCProxy::ResetXADC()\n");
  *(regs + R_SW_RESET) = 0x0000000A;
  usleep(1000);
}


///////////////////////////////////////////////////////////////////////////////
// This function should not be called by the client once StartMeasurements() is called
// because it is not thread-safe and can lead to the XADC interface or the HW ADC itself crashing.
double CXADCProxy::GetPower()
{
  volatile uint32_t * regs = (volatile uint32_t *)accelRegs;
  uint32_t adc;

  // Wait EOC
  while ( ! (*(regs+R_STATUS) & 32) );

  adc = *(regs+R_VPVN_DATA);
  if (logging)
    printf("CXADCProxy::GetPower() Read: %u\n", adc);

  // Remove first the JTAG status bits on AXI bits 17 & 16 
  // (we assume there are no JTAG transactions in this system).
  // ADC gives 12 bits, left aligned on 15-4.
  adc = (adc & 0xFFFF) >> 4;

  return adc * ADC_GAIN_FACTOR * VCC;
}

