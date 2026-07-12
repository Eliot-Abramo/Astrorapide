#ifndef CACCELPROXY_HPP
#define CACCELPROXY_HPP

// Requires <map>, <stdint.h>

//  This class takes care of the low-level configuration of addresses.
// The class stores internally the address of the device registers in the application virtual space,
// and a map of DMA-compatible memory allocations that relates virtual with physical addresses.

class CAccelProxy {
  protected:
    volatile void * accelRegs;
    uint32_t baseAddr, mappingSize;
    bool logging;

    // Map of virtual addresses to physical addresses
    std::map<uint32_t, uint32_t> dmaMappings;

    // Called by the destructor to free any dangling DMA allocations.
    void InternalEmptyDMAAllocs();

  public:
    typedef enum {OK = 0, DEVICE_ALREADY_INITIALIZED = 1, DEVICE_NOT_INITIALIZED = 2, ERROR_MAPPING_BASE_ADDR = 3,
                VIRT_ADDR_NOT_FOUND = 4} TErrors;

  public:
    CAccelProxy(bool Logging = false);
    virtual ~CAccelProxy();

    // Maps the address of the peripheral registers in the physical address space into the application virtual address space.
    uint32_t Open(uint32_t BaseAddr, uint32_t MappingSize = 65536, volatile void ** AccelRegsPointer = NULL);

    // Allocates a block of DMA-compatible memory and returns the corresponding address in this application virtual address space.
    // The class keeps an internal map of virtual to physical addresses, so that derived classes can translate the virtual 
    // addresses supplied by the applications.
    void * AllocDMACompatible(uint32_t Size, uint32_t Cacheable = 0);
    bool FreeDMACompatible(void * VirtAddr);
    // The application should never use the physical address. This is just for debugging purposes.
    uint32_t GetDMAPhysicalAddr(void * VirtAddr);
};


#endif  // CACCELPROXY_HPP
