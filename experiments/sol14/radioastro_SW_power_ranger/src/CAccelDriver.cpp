#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include <fcntl.h>
#include <string.h>
#include "CAccelDriver.hpp"
extern "C" {
#include <libxlnk_cma.h>  // Required for memory-mapping functions from Xilinx
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////// CAccelDriver() ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CAccelDriver::CAccelDriver(bool Logging)
  : driver(0), logging(Logging)
{
  if (logging)
    printf("CAccelDriver::CAccelDriver()\n");
}


///////////////////////////////////////////////////////////////////////////////
/////////////////////////// ~CAccelDriver() ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CAccelDriver::~CAccelDriver()
{
  if (logging)
    printf("CAccelDriver::~CAccelDriver()\n");

  if (driver > 0) {
    // Unmap the physical address of the peripheral registers
    close(driver);
    driver = 0;
  }

  // DMA memory is a system-wide resource. If the user forgets to free the allocated
  // blocks, the memory is lost and the system will eventually require a reboot. To 
  // prevent this, let's ensure all the DMA allocations have been freed.
  InternalEmptyDMAAllocs();
}


///////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Open() ////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint32_t CAccelDriver::Open(const char * driver_name)
{
  if (logging)
    printf("CAccelDriver::Open(driver_name = %s)\n", driver_name);

  if (driver != 0)
    return DEVICE_ALREADY_INITIALIZED;
  
  // Open the device driver
  driver = open(driver_name, O_RDWR);
  if (driver == -1) {
    printf("ERR: cannot open driver %s\n", driver_name);
    return ERROR_OPENING_DRIVER;
  }

  return OK;
}


///////////////////////////////////////////////////////////////////////////////
//////////////////////// AllocDMACompatible() /////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void * CAccelDriver::AllocDMACompatible(uint32_t Size, uint32_t Cacheable)
{
  void * virtualAddr = NULL;
  uint32_t physicalAddr = 0;

  if (logging)
    printf("CAccelDriver::AllocDMACompatible(Size = %u, Cacheable = %u)\n", Size, Cacheable);

  virtualAddr = cma_alloc(Size, Cacheable);
  if ( (int32_t)virtualAddr == -1) {
    if (logging)
      printf("Error allocating DMA memory for %u bytes.\n", Size);
    return NULL;
  }

  physicalAddr = cma_get_phy_addr(virtualAddr);
  if (physicalAddr == 0) {
    if (logging)
      printf("Error obtaining physical addr for virtual address 0x%08X (%u).\n", (uint32_t)virtualAddr, (uint32_t)virtualAddr);
    cma_free(virtualAddr);
    return NULL;
  }

  dmaMappings[(uint32_t)virtualAddr] = physicalAddr;

  if (logging)
    printf("DMA memory allocated - Virtual addr: 0x%08X (%u) // Physical addr: 0x%08X (%u)\n",
            (uint32_t)virtualAddr, (uint32_t)virtualAddr, physicalAddr, physicalAddr);

  return virtualAddr;
}


///////////////////////////////////////////////////////////////////////////////
////////////////////////// FreeDMACompatible() ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool CAccelDriver::FreeDMACompatible(void * VirtAddr)
{
  if (logging)
    printf("CAccelDriver::FreeDMACompatible(Addr = 0x%08X)\n", (uint32_t)VirtAddr);

  if (logging) {
    if (dmaMappings.count((uint32_t)VirtAddr) == 0)
      printf("No virtual address 0x%08X present in the dictionary of mappings.\n", (uint32_t)VirtAddr);
  }

  dmaMappings.erase((uint32_t)VirtAddr);
  cma_free(VirtAddr);

  return true;
}


///////////////////////////////////////////////////////////////////////////////
////////////////////////// GetDMAPhysicalAddr() ///////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint32_t CAccelDriver::GetDMAPhysicalAddr(void * VirtAddr)
{
  if (logging)
    printf("CAccelDriver::GetDMAPhysicalAddr(Addr = 0x%08X)\n", (uint32_t)VirtAddr);

  if (dmaMappings.count((uint32_t)VirtAddr) == 0) {
    if (logging)
      printf("No virtual address 0x%08X present in the dictionary of mappings.\n", (uint32_t)VirtAddr);
    return 0;
  }
  
  return dmaMappings[(uint32_t)VirtAddr];
}


///////////////////////////////////////////////////////////////////////////////
////////////////////// InternalEmptyDMAAllocs() ///////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Called by the destructor to free any dangling DMA allocations.
void CAccelDriver::InternalEmptyDMAAllocs()
{
  uint32_t numMappings = dmaMappings.size();

  if (logging)
    printf("CAccelDriver::InternalEmptyDMAAllocs(DMA dict size = %u)\n", numMappings);

  if (numMappings > 0)
    printf("DMA MEMORY WAS NOT CORRECTLY FREED. PERFORMING EMERGENCY RELEASE OF KERNEL DMA MEMORY IN DESTRUCTOR. PLEASE, FIX THIS ISSUE.\n");

  for (auto it = dmaMappings.begin(); it != dmaMappings.end(); ++ it) {
    uint32_t virtAddr = it->first;
    if (logging)
      printf("Releasing DMA (virtual) pointer 0x%08X\n", virtAddr);
    cma_free((void*)virtAddr);
  }

  dmaMappings.clear();
}


