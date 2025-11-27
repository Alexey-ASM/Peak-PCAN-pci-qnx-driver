#pragma once

#include <queue>
#include <mqueue.h>
#include <cstdint>
#include <thread>

#include "non_copyable.h"

#include "chip_ports.h"
#include "chip_mapper.h"

#include "unit_cthread.h"
#include "../common/include/can.h"

#include <iostream>

//------------------------------------------------------------------------------------------------

enum ECanBaudRate
{
    ECBR_NONE   = 0,
    ECBR_1MB    = 1,
    ECBR_800KB  = 2,
    ECBR_500KB  = 3,
    ECBR_250KB  = 4,
    ECBR_125KB  = 5,
    ECBR_100KB  = 6,
    ECBR_50KB   = 7,
    ECBR_20KB   = 8,
    ECBR_10KB   = 9,
};
 
//------------------------------------------------------------------------------------------------


class CanController : NonCopyable
{
public:
    CanController(std::unique_ptr<ChipMapperBase> chipMapper, ECanBaudRate baudRate);

    virtual ~CanController() = default;
    
    virtual bool InitController();

    virtual void CloseController();
    
    virtual bool WriteMessage(const can_frame& canFrame) =0;

    virtual bool ReadMessage(can_frame& canFrame) =0;

    virtual void InterruptServiceRoutine() = 0;

protected:

    std::uint64_t GetNsec() const;

    virtual bool IsThereDevice() = 0;
    virtual void InterruptHandleTh() = 0;


    void PutByte(const tPort8* byteAddr, std::uint8_t value);   

    std::uint8_t GetByte(const tPort8* byteAddr);

    void PutWord(const tPort16* byteAddr, std::uint16_t value);   

    std::uint16_t GetWord(const tPort16* byteAddr);

    bool inited_;

    ECanBaudRate baudRate_;
    
    std::unique_ptr<ChipMapperBase> chipMapper_;

private:
    
    std::thread interruptHandleTh_;
};


//------------------------------------------------------------------------------------------------

