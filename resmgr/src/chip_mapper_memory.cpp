#include <stdexcept>
#include <sys/mman.h>

#include "log.h"
#include "chip_mapper_memory.h"

//------------------------------------------------------------------------------------------------

ChipMapperMemory::ChipMapperMemory(std::uint64_t baseAddr, std::uint64_t size, std::uint8_t shift)
  : size_(size)
  , shift_(shift)
{
    LOG(info) << "pBaseAddr: " << std::hex << baseAddr
              << " size: " << size
              << " shift: " <<std::dec << int(shift);

    if (shift > 8) 
    {
        
        throw std::runtime_error("Invalid shift value");
    }

    baseAddr_ = (std::uint8_t*)mmap_device_memory(0, size, PROT_READ | PROT_WRITE | PROT_NOCACHE, 0, baseAddr);

    if (baseAddr_ == MAP_FAILED) 
    {
        
        throw std::runtime_error("Could not map device memory region");
    }

    LOG(info) << "Done: baseAddr_: " << std::hex << baseAddr_;
}

//------------------------------------------------------------------------------------------------

ChipMapperMemory::~ChipMapperMemory()
{
    munmap_device_memory((void*)baseAddr_, size_);
}

//------------------------------------------------------------------------------------------------

void ChipMapperMemory::PutByte(const tPort8* byteAddr, std::uint8_t value) const
{
    const std::uint64_t shiftedAddr = (std::uint64_t)byteAddr << shift_;

    tPort8* ptReg = (tPort8*)(baseAddr_ + shiftedAddr);

    *ptReg = value;
}

//------------------------------------------------------------------------------------------------

std::uint8_t ChipMapperMemory::GetByte(const tPort8* byteAddr) const
{
    const std::uint64_t shiftedAddr = (std::uint64_t)byteAddr << shift_;

    const tPort8* ptReg = (tPort8*)(baseAddr_ + shiftedAddr);

    const std::uint8_t value = *ptReg;

    return value;
}

//------------------------------------------------------------------------------------------------

void ChipMapperMemory::PutWord(const tPort16* byteAddr, std::uint16_t value) const
{
    const std::uint64_t shiftedAddr = (std::uint64_t)byteAddr << shift_;

    tPort16* ptReg = (tPort16*)(baseAddr_ + shiftedAddr);

    *ptReg = value;
}

//------------------------------------------------------------------------------------------------

std::uint16_t ChipMapperMemory::GetWord(const tPort16* byteAddr) const
{
    const std::uint64_t shiftedAddr = (std::uint64_t)byteAddr << shift_;

    const tPort16* ptReg = (tPort16*)(baseAddr_ + shiftedAddr);

    const std::uint16_t value = *ptReg;

    return value;
}

//------------------------------------------------------------------------------------------------
