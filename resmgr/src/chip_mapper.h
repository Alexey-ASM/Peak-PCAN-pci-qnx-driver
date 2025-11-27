#pragma once

#include <cstdint>

#include "non_copyable.h"
#include "chip_ports.h"

//------------------------------------------------------------------------------

class ChipMapperBase : NonCopyable
{
public:
    
    ChipMapperBase(void) = default;
    virtual ~ChipMapperBase(void) = default;
    
    virtual void PutByte(const tPort8* byteAddr, std::uint8_t value) const = 0;
    virtual std::uint8_t GetByte(const tPort8* byteAddr) const = 0;

    virtual void PutWord(const tPort16* byteAddr, std::uint16_t value) const = 0;
    virtual std::uint16_t GetWord(const tPort16* byteAddr) const = 0;
};
