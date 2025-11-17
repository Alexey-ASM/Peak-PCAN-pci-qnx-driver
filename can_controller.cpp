
#include "can_controller.h"

//#include <robotools/log.h>
#include "controller_factory.h"

//------------------------------------------------------------------------------------------------

CanController::CanController(std::unique_ptr<ChipMapperBase> chipMapper, ECanBaudRate baudRate)
 : inited_(false)
 , baudRate_(baudRate)
 , chipMapper_(std::move(chipMapper))
{
//    LOG(debug);
}

//------------------------------------------------------------------------------------------------

void CanController::PutByte(const tPort8* byteAddr, std::uint8_t value)
{
    chipMapper_->PutByte(byteAddr, value);
}

//------------------------------------------------------------------------------------------------

std::uint8_t CanController::GetByte(const tPort8* byteAddr)
{
    return chipMapper_->GetByte(byteAddr);
}

//------------------------------------------------------------------------------------------------

void CanController::PutWord(const tPort16* byteAddr, std::uint16_t value)
{
    chipMapper_->PutWord(byteAddr, value);
}

//------------------------------------------------------------------------------------------------

std::uint16_t CanController::GetWord(const tPort16* byteAddr)
{
    return chipMapper_->GetWord(byteAddr);
}

//------------------------------------------------------------------------------------------------

bool CanController::InitController()
{
//    LOG(debug) ;

    interruptHandleTh_ = std::thread(&CanController::InterruptHandleTh, this);
    
//    LOG(debug) << "Done";
    
    return true;
}

//------------------------------------------------------------------------------------------------

void CanController::CloseController()
{
//    LOG(debug);

//    LOG(debug) << "Done";
}

//------------------------------------------------------------------------------------------------

std::uint64_t CanController::GetNsec() const
{
    return (ClockCycles() * 1000000000) / SYSPAGE_ENTRY(qtime)->cycles_per_sec;
}

//------------------------------------------------------------------------------------------------

