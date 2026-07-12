#ifndef CXADCPROXY_HPP
#define CXADCPROXY_HPP

// Requires <pthread.h>, <map>, <stdint.h>. Linking with -lpthread

// This class imlements functionality to access the XADC device of the Pynq Power Ranger.
// The user has to call StartMeasurements(). Then, GetPower() can be used to retrieve a 
// monotonically increasing value of energy consumed since the call to StartMeasurements().
// The values are given in Joules and can be subtracted.
//
// The class creates a secondary thread using pthreads that polls the ADC.
// The ADC acquires 256 samples and averages them to deliver one sample at ~123 Hz.
// The worker thread waits 8130 us between sample readings (polling), which roughly matches the 
// sample production rate of the ADC.
//
// The worker thread is stopped when StopMeasurements() is called. The user cannot read power
// values if the thread is not active.
//
// Calling GetPower() once StartMeasurements() has been called IS NOT THREAD-SAFE and can lead
// to the ADC HW crashing! Use GetEnergy with a valid argument instead.

class CXADCProxy : public CAccelProxy {
  public:
    CXADCProxy(bool Logging = false, const char * ThreadName = NULL, uint32_t CPU = UINT32_MAX);
    ~CXADCProxy();
    pthread_t GetThreadID()
    { return threadID; }

    bool StartMeasurements();
    bool StopMeasurements();

    // Returns accumulated energy since start of measurements. If Power is not NULL, returns also
    // last measured power.
    double GetEnergy(float * power = NULL)
    {
      if (power) *power = lastPower;
      return accEnergy;
    }

    double GetPower(); // Returns latest power measurement
    double GetMaxPower() // Maximum power that can be measured
    { return MAX_POWER; }


  protected:
    enum {
      R_SW_RESET = 0, R_STATUS = 4/4, 
      R_VPVN_DATA = 0x20C/4
    };
    static constexpr uint32_t SAMPLING_PERIOD_us = 8130; // (1 s / 123 Hz) * 1e6 = 8130 us
    static constexpr double VCC = 5; // Typical voltage received through the USB port
    static constexpr double ADC_GAIN_FACTOR = (double)0.000305;
    static constexpr double MAX_POWER = 4095 * ADC_GAIN_FACTOR * VCC;

  protected:
    void ResetXADC();

    virtual bool Main();
	  // Entry point for new threads. Casts to CXADCProxy * and
	  // calls Main() in the new thread.
	  static void * NewThread(void * objectPointer);

  protected: // Thread control 
	  volatile pthread_t threadID;
    volatile uint32_t cpuNum;
	  volatile bool stopping;
	  volatile bool stoppingACK;
	  char * threadName;

  protected: // XADC measurements
    volatile double accEnergy;
    volatile float lastPower; // Float is atomic.
};

#endif  // CXADCPROXY_HPP

