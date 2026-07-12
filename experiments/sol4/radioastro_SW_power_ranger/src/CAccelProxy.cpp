#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include "CAccelProxy.hpp"
extern "C" {
#include <libxlnk_cma.h>  // Required for memory-mapping functions from Xilinx
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////// CAccelProxy() ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CAccelProxy::CAccelProxy(bool Logging)
  : accelRegs(NULL), baseAddr(0), mappingSize(0), logging(Logging)
{
  if (logging)
    printf("CAccelProxy::CAccelProxy()\n");
}


///////////////////////////////////////////////////////////////////////////////
/////////////////////////// ~CAccelProxy() ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CAccelProxy::~CAccelProxy()
{
  if (logging)
    printf("CAccelProxy::~CAccelProxy()\n");

  if (accelRegs != NULL) {
    // Unmap the physical address of the peripheral registers
    cma_munmap((void*)accelRegs, mappingSize);
    if (logging)
      printf("Mapping undone for peripheral physical address 0x%08X mapped at 0x%08X\n",
          baseAddr, (uint32_t)accelRegs);
  }
  accelRegs = NULL;

  // DMA memory is a system-wide resource. If the user forgets to free the allocated
  // blocks, the memory is lost and the system will eventually require a reboot. To 
  // prevent this, let's ensure all the DMA allocations have been freed.
  InternalEmptyDMAAllocs();
}


///////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Open() ////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint32_t CAccelProxy::Open(uint32_t BaseAddr, uint32_t MappingSize, volatile void ** AccelRegsPointer)
{
  if (logging)
    printf("CAccelProxy::Open(BaseAddr = 0x%08X, Size = %u)\n", BaseAddr, MappingSize);

  if (accelRegs != NULL)
    return DEVICE_ALREADY_INITIALIZED;

  mappingSize = MappingSize;
  baseAddr = BaseAddr;
  
  // Map the physical address of the accelerator into this app virtual address space
  accelRegs = (void *)cma_mmap(baseAddr, mappingSize);
  if ((int32_t)accelRegs == -1) {
    if (logging)
      printf("Error mapping the peripheral address (0x%08X)!\n", baseAddr);
    accelRegs = NULL;
    return ERROR_MAPPING_BASE_ADDR;
  }

  if (logging)
    printf("Address mapping done. Peripheral physical address 0x%08X mapped at 0x%08X\n",
          baseAddr, (uint32_t)accelRegs);

  if (AccelRegsPointer != NULL)
    *AccelRegsPointer = accelRegs;
  return OK;
}


///////////////////////////////////////////////////////////////////////////////
//////////////////////// AllocDMACompatible() /////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void * CAccelProxy::AllocDMACompatible(uint32_t Size, uint32_t Cacheable)
{
  void * virtualAddr = NULL;
  uint32_t physicalAddr = 0;

  if (logging)
    printf("CAccelProxy::AllocDMACompatible(Size = %u, Cacheable = %u)\n", Size, Cacheable);

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

bool CAccelProxy::FreeDMACompatible(void * VirtAddr)
{
  if (logging)
    printf("CAccelProxy::FreeDMACompatible(Addr = 0x%08X)\n", (uint32_t)VirtAddr);

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

uint32_t CAccelProxy::GetDMAPhysicalAddr(void * VirtAddr)
{
  if (logging)
    printf("CAccelProxy::GetDMAPhysicalAddr(Addr = 0x%08X)\n", (uint32_t)VirtAddr);

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
void CAccelProxy::InternalEmptyDMAAllocs()
{
  uint32_t numMappings = dmaMappings.size();

  if (logging)
    printf("CAccelProxy::InternalEmptyDMAAllocs(DMA dict size = %u)\n", numMappings);

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


