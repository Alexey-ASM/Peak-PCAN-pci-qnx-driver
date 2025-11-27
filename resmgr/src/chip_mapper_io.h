#pragma once

#include "chip_mapper.h"

//----------------------------------------------------------------------------

class ChipMapperIO : public ChipMapperBase
{
public:
    
    ChipMapperIO(std::uint64_t baseAddr, std::uint64_t size);
    virtual ~ChipMapperIO(void);
    
    virtual void PutByte(const tPort8* byteAddr, std::uint8_t value) const;   
    virtual std::uint8_t GetByte(const tPort8* byteAddr) const;

    virtual void PutWord(const tPort16* byteAddr, std::uint16_t value) const;   
    virtual std::uint16_t GetWord(const tPort16* byteAddr) const;

private:

    volatile uintptr_t baseAddr_;
    std::uint64_t  size_;
};

