#include <stdexcept>

#include <hw/inout.h>
#include <sys/mman.h>

#include "log.h"
#include "chip_mapper_io.h"

//------------------------------------------------------------------------------------------------

ChipMapperIO::ChipMapperIO(std::uint64_t baseAddr, std::uint64_t size)
  : size_(size)
{
    LOG(info) << "baseAddr: " << std::hex << baseAddr
              << " size: " << size;

    baseAddr_ = mmap_device_io(size, baseAddr);
        
    if (baseAddr_ == MAP_DEVICE_FAILED) {
        
         throw std::runtime_error("Could not map device IO region");
    }

    LOG(info) << "Done: " << std::hex << int(baseAddr_);
}

//------------------------------------------------------------------------------------------------

ChipMapperIO::~ChipMapperIO()
{
    munmap_device_io(baseAddr_, size_);
}

//------------------------------------------------------------------------------------------------

void ChipMapperIO::PutByte(const tPort8* byteAddr, std::uint8_t value) const
{
    out8(baseAddr_ + (size_t)byteAddr, value);
}

//------------------------------------------------------------------------------------------------

std::uint8_t ChipMapperIO::GetByte(const tPort8* byteAddr) const
{
    const std::uint8_t value = in8(baseAddr_ + (size_t)byteAddr);

    return value;
}

//------------------------------------------------------------------------------------------------

void ChipMapperIO::PutWord(const tPort16* byteAddr, std::uint16_t value) const
{
    out16(baseAddr_ + (size_t)byteAddr, value);
}

//------------------------------------------------------------------------------------------------

std::uint16_t ChipMapperIO::GetWord(const tPort16* byteAddr) const
{
    const std::uint16_t value = in16(baseAddr_ + (size_t)byteAddr);

    return value;
}

//------------------------------------------------------------------------------------------------

