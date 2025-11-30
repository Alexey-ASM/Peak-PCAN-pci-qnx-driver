#include <chrono>
#include <cstring>
#include <iomanip>

#include "sja1000_can_controller.h"

#include "log.h"
#include "controller_factory.h"

//------------------------------------------------------------------------------------------------

SJA1000CanController::SJA1000CanController(std::unique_ptr<ChipMapperBase> chipMapper, ECanBaudRate baudRate)
 : CanController(std::move(chipMapper), baudRate)
 , sja1000Map_(0)
 , receiveMessageBufHead_(0)
 , receiveMessageBufTail_(0)
 , errorBufHead_(0)
 , errorBufTail_(0)
 , interruptChannel_(_NTO_CHF_FIXED_PRIORITY)
 , transmitBufferFree_(true)
{
    memset(&interruptSpinLock_, 0, sizeof(interruptSpinLock_));

    SIGEV_PULSE_INIT(&intSignal_, interruptChannel_.coid,
                     SIGEV_PULSE_PRIO_INHERIT, INTERRUPT_PULSE, 0);
}

//------------------------------------------------------------------------------------------------

SJA1000CanController::~SJA1000CanController()
{
    if(inited_ == true) 
    {
        CloseController();
    }

    LOG(info) << "closed";
}

//------------------------------------------------------------------------------------------------

bool SJA1000CanController::InitController()
{
    // Tables of precomputed Bit Timing Register values
    const unsigned char BitTiming0[] =
    {
        0x00,      // CAN_NOT_PRESENT (should not be used)
        0x00,      // 1M      BRP=1 SJW=1
        0x40,      // 800K    BRP=0 SJW=2
        0x80,      // 500K    BRP=0 SJW=3
        0x81,      // 250K    BRP=2 SJW=3
        0x83,      // 125K    BRP=4 SJW=3
        0x84,      // 100K    BRP=5 SJW=3       not tested
        0xC7,      // 50K     BRP=8 SJW=4
        0x67,      // 20K     BRP=40 SJW=2
        0xE7       // 10K     BRP=40 SJW=4
    };

    const unsigned char BitTiming1[] =
    {
        0x00,      // CAN_NOT_PRESENT (should not be used)
        0x14,      // 1M      TSEG1=5   TSEG2=2  75%
        0x25,      // 800K    TSEG1=6   TSEG2=3  75%
        0x58,      // 500K    TSEG1=9   TSEG2=6  75%
        0x58,      // 250K    TSEG1=9   TSEG2=6  75%
        0x58,      // 125K    TSEG1=9   TSEG2=6  75%
        0x58,      // 100K    TSEG1=9   TSEG2=6  75%        not tested
        0x7A,      // 50K     TSEG1=11  TSEG2=8  75%
        0x25,      // 20K     TSEG1=6   TSEG2=3  75%
        0x7A       // 10K     TSEG1=11  TSEG2=8  75%
    };

    if(CanController::InitController() == false) 
    {
        LOG(error) << "Controller init error";

        return false;
    }

    if (IsThereDevice() == false) 
    {
        LOG(error) << "No SJA1000 controller";

        return false;
    }

    PutByte(&sja1000Map_->clkDiv, CAN_CDR_CANM | CAN_CDR_CLKOFF | 0x00); // PeliCAN mode, bypass CAN input,fosc
    //  PutByte(&sja1000Map_->clkDiv, CAN_CDR_CANM | CAN_CDR_CBP | CAN_CDR_CLKOFF | 0x07); // PeliCAN mode, bypass CAN input,fosc
    PutByte(&sja1000Map_->ModeReg, CAN_MR_AFM | CAN_MR_RM); // single acceptance filter option is enable
    PutByte(&sja1000Map_->cmndReg, CAN_CM_AT | CAN_CM_COS | CAN_CM_RRB);
    PutByte(&sja1000Map_->RxTxFrInf, 0xff); // set acceptance code0 0xff

    for(unsigned i = 0; i < 7; ++i)
    {
        PutByte(&sja1000Map_->RxTxIdData[i], 0xff) ; // set acceptance code and mask 0xff
    }

    PutByte(&sja1000Map_->busTim0, BitTiming0[baudRate_]);
    PutByte(&sja1000Map_->busTim1, BitTiming1[baudRate_]);

    // Configure Output mode
    PutByte(&sja1000Map_->outCtrl, 0x1A);  // for KONTRON KBOX A-101 Board
//  PutByte(&sja1000Map_->outCtrl, 0xC2);
//  PutByte(&sja1000Map_->outCtrl, 0x5e); 

    GetByte(&sja1000Map_->intrReg);         // clear interrupt
    PutByte(&sja1000Map_->intrEnReg, 0xbf); // enable all but arbitration lost interrupts
//    PutByte(&sja1000Map_->intrEnReg, 0x00); // disable all interrupts
    PutByte(&sja1000Map_->ErrWarLim, 96); // set error warning limit
    PutByte(&sja1000Map_->TxErrCount, 0); // reset Tx error counter
    PutByte(&sja1000Map_->RxErrCount, 0); // reset Rx error counter
    PutByte(&sja1000Map_->ModeReg, (GetByte(&sja1000Map_->ModeReg) &~ CAN_MR_RM)); //normal mode
    
    const std::uint64_t nTimer = GetNsec();
    
    while (GetByte(&sja1000Map_->ModeReg) & 1) 
    {
        if ((GetNsec() - nTimer) > 1000000000ULL)
        {
            LOG(error) << "Timeout elapsed";

            return false;
        }
    }
    
    GetByte(&sja1000Map_->ArbLostCap); // arbitration lost capture
    GetByte(&sja1000Map_->ErrCodeCap); // error code capture
    GetByte(&sja1000Map_->RxErrCount); // RX error counter
    GetByte(&sja1000Map_->TxErrCount); // TX error counter

    inited_ = true;

    LOG(info) << "Controller inited";

    return true;
}

//------------------------------------------------------------------------------------------------

bool SJA1000CanController::IsThereDevice()
{
    LOG(debug);

    GetByte(&sja1000Map_->statusReg);
    GetByte(&sja1000Map_->ArbLostCap);    // arbitration lost capture
    GetByte(&sja1000Map_->ErrCodeCap);    // error code capture
    GetByte(&sja1000Map_->RxErrCount);    // RX error counter
    GetByte(&sja1000Map_->TxErrCount);    // TX error counter

    PutByte(&sja1000Map_->ModeReg, CAN_MR_RM); //enter to reset mode

    const std::uint64_t nTimer = GetNsec();

    while ((GetByte(&sja1000Map_->ModeReg) & CAN_MR_RM) == 0) 
    {
        if ((GetNsec() - nTimer) > 100000000ULL) 
        {
            LOG(error) << "Timeout elapsed";

            return false;
        }
    }

    PutByte(&sja1000Map_->busTim0, 0x55);

    std::uint8_t nResult = GetByte(&sja1000Map_->busTim0);

    if(nResult != 0x55) 
    {
        LOG(error) << "0x55 signature is missed: " << std::hex
                   << std::setw(2) << std::setfill('0') << nResult;

        return false;
    }

    PutByte(&sja1000Map_->busTim0, 0xAA);

    nResult = GetByte(&sja1000Map_->busTim0);

    if(nResult != 0xAA) 
    {
        LOG(error) << "0xAA signature is missed: " << std::hex
                   << std::setw(2) << std::setfill('0') << nResult;

        return false;
    }

    LOG(debug) << "Done";

    return true;
}
    
//------------------------------------------------------------------------------------------------

void SJA1000CanController::CloseController()
{  
    LOG(info);

    PutByte(&sja1000Map_->ModeReg, 0);

    inited_ = false;

    MsgSendPulse(interruptChannel_.coid, SIGEV_PULSE_PRIO_INHERIT, TERMINATE_PULSE, 0);
   
    CanController::CloseController();

    LOG(info) << "Done";
}

//------------------------------------------------------------------------------------------------

bool SJA1000CanController::WriteMessage(const can_frame& canFrame)
{
    std::unique_lock<std::mutex> lock(transmitMutex_);

    if(transmitDataQueue_.empty() && transmitBufferFree_)
    {
        TransmitMessage(canFrame);
    }
    else
    {
        transmitDataQueue_.push(canFrame);
    }

    return true;
}

//------------------------------------------------------------------------------------------------

void SJA1000CanController::ReceiveMessage()
{
    unsigned messages = MAX_RECEIVED_MESSAGES;

    do
    {
        const tPort8 messageCfg = GetByte(&sja1000Map_->RxTxFrInf);

        receiveMessageBuf_[receiveMessageBufHead_].can_id = (messageCfg & (EXTENDED_FRAME_FORMAT | REMOTE_REQUEST)) << 24;
        receiveMessageBuf_[receiveMessageBufHead_].len = messageCfg & DATA_LENGTH_MASK;

        size_t dataOffset = 2;

        if(messageCfg & EXTENDED_FRAME_FORMAT)
        {
            const auto canId = std::uint32_t((GetByte(&sja1000Map_->RxTxIdData[0]) << 21) |
            		                         (GetByte(&sja1000Map_->RxTxIdData[1]) << 13) |
											 (GetByte(&sja1000Map_->RxTxIdData[2]) << 5) |
											 (GetByte(&sja1000Map_->RxTxIdData[3]) >> 3));

            receiveMessageBuf_[receiveMessageBufHead_].can_id += canId;

            dataOffset = 4;
        }
        else
        {
        	const auto canId = std::uint32_t((GetByte(&sja1000Map_->RxTxIdData[0]) << 3) |
        			                         (GetByte(&sja1000Map_->RxTxIdData[1]) >> 5));

        	receiveMessageBuf_[receiveMessageBufHead_].can_id += canId;
        }

        for(size_t i = 0; i < receiveMessageBuf_[receiveMessageBufHead_].len; ++i)
        {
            receiveMessageBuf_[receiveMessageBufHead_].data[i] = GetByte(&sja1000Map_->RxTxIdData[i + dataOffset]);
        }

        PutByte(&sja1000Map_->cmndReg, CAN_CM_RRB);
        GetByte(&sja1000Map_->statusReg);

        ++receiveMessageBufHead_;

        if(receiveMessageBufHead_ == RECEIVE_BUFFER_SIZE)
        {
            receiveMessageBufHead_ = 0;
        }

    } while (((GetByte(&GetBasePtr()->statusReg) & CAN_SR_RBS) != 0) &&
            (messages--));
}

//------------------------------------------------------------------------------------------------

void SJA1000CanController::InterruptServiceRoutine()
{
    EnterCmdRegWriteCriticalSection();

    bool hit = false;

    for (;;)
    {
        const tPort8 ireg = GetByte(&(GetBasePtr())->intrReg);

//        ControllerFactory::Instance().FinializeInterrupt();

        if ((ireg & 0xF) == 0)
            break;

        if (ireg & CAN_IR_RX)
        {
            ReceiveMessage();
            hit = true;
        }

        if (ireg & CAN_IR_TX)
        {
            TransmitBufferFree();
            hit = true;
        }

        if(ireg & (CAN_IR_BEI | CAN_IR_ALI | CAN_IR_EPI | CAN_IR_WUI | CAN_IR_OVERRUN | CAN_IR_ERRINT))
        {
            AddError(ireg);
            hit = true;

            if (ireg & CAN_IR_OVERRUN)
            {
                //Clear overrun status
                PutByte(&(GetBasePtr())->cmndReg, CAN_CM_COS | CAN_CM_RRB);
            }
        }
    }

    LeaveCmdRegWriteCriticalSection();

    if(hit)
    {
        MsgSendPulse(interruptChannel_.coid, SIGEV_PULSE_PRIO_INHERIT, INTERRUPT_PULSE, 0);
    }
}

//------------------------------------------------------------------------------------------------

inline void SJA1000CanController::AddError(std::uint8_t error)
{
    errorMessageBuf_[errorBufHead_++] = error;

    if(errorBufHead_ == ERROR_BUFFER_SIZE)
    {
        errorBufHead_ = 0;
    }
}

//------------------------------------------------------------------------------------------------

void SJA1000CanController::ProcessErrorBuffer()
{
    while(errorBufHead_ != errorBufTail_)
    {
        if (errorMessageBuf_[errorBufTail_] & CAN_IR_BEI)
        {
            auto capturedErrorCode = GetByte(&sja1000Map_->ErrCodeCap);
            
            std::string capturedError;
            
            switch (capturedErrorCode & 0xC0)
            {
                case 0x00 : capturedError + "bit error in "           ; break;
                case 0x40 : capturedError + "form error in "          ; break;
                case 0x80 : capturedError + "stuff error in "         ; break;
                case 0xC0 : capturedError + "other type of error in " ; break;
            }

            switch (capturedErrorCode & 0x1F)
            {
                case 0x03 : capturedError + "start of frame"         ; break;
                case 0x02 : capturedError + "ID.28 to ID.21"         ; break;
                case 0x06 : capturedError + "ID.20 to ID.18"         ; break;
                case 0x04 : capturedError + "bit SRTR"               ; break;
                case 0x05 : capturedError + "bit IDE"                ; break;
                case 0x07 : capturedError + "ID.17 to ID.13"         ; break;
                case 0x0F : capturedError + "ID.12 to ID.5"          ; break;
                case 0x0E : capturedError + "ID.4 to ID.0"           ; break;
                case 0x0C : capturedError + "bit RTR"                ; break;
                case 0x0D : capturedError + "reserved bit 1"         ; break;
                case 0x09 : capturedError + "reserved bit 0"         ; break;
                case 0x0B : capturedError + "data length code"       ; break;
                case 0x0A : capturedError + "data ﬁeld"              ; break;
                case 0x08 : capturedError + "CRC sequence"           ; break;
                case 0x18 : capturedError + "CRC delimiter"          ; break;
                case 0x19 : capturedError + "ackno wledge slot"      ; break;
                case 0x1B : capturedError + "ackno wledge delimiter" ; break;
                case 0x1A : capturedError + "end of frame"           ; break;
                case 0x12 : capturedError + "intermission"           ; break;
                case 0x11 : capturedError + "active error ﬂag"       ; break;
                case 0x16 : capturedError + "passive error ﬂag"      ; break;
                case 0x13 : capturedError + "tolerate dominant bits" ; break;
                case 0x17 : capturedError + "error delimiter"        ; break;
                case 0x1C : capturedError + "overload flag"          ; break;
                default:    capturedError + "undefined position";
            }
    
            LOG(error) << "Bus error: " << capturedError;
        }
        if (errorMessageBuf_[errorBufTail_] & CAN_IR_ALI)
        {
            auto arbLostBit = GetByte(&sja1000Map_->ArbLostCap);
            LOG(error) << "Arbitration lost position: " << arbLostBit;
        }
        if (errorMessageBuf_[errorBufTail_] & CAN_IR_EPI)
        {
            LOG(error) << "Error passive";
        }
        if (errorMessageBuf_[errorBufTail_] & CAN_IR_WUI)
        {
            LOG(error) << "Wake-up";
        }
        if (errorMessageBuf_[errorBufTail_] & CAN_IR_OVERRUN)
        {
            LOG(error) << "Overrun";
        }
        if (errorMessageBuf_[errorBufTail_] & CAN_IR_ERRINT)
        {
            LOG(error) << "Error warning: "
                       << " ErrWarLim: "  << std::dec << int(GetByte(&GetBasePtr()->ErrWarLim))
                       << " RxErrCount: " << std::dec << int(GetByte(&GetBasePtr()->RxErrCount))
                       << " TxErrCount: " << std::dec << int(GetByte(&GetBasePtr()->TxErrCount))
                       << " RxMsgCount: " << std::dec << int(GetByte(&GetBasePtr()->RxMsgCount));
        }

        errorBufTail_++;

        if(errorBufTail_ == ERROR_BUFFER_SIZE)
        {
            errorBufTail_ = 0;
        }
    }
}

//------------------------------------------------------------------------------------------------

bool SJA1000CanController::ReadMessage(can_frame& canFrame)
{
    while(receiveMessageBufHead_ == receiveMessageBufTail_ && inited_)
    {
        std::unique_lock<std::mutex> lock(receiveMutex_);
        receiveCond_.wait_for(lock, std::chrono::milliseconds(2));
    }

    if(!inited_)
    {
    	return false;
    }

    canFrame = receiveMessageBuf_[receiveMessageBufTail_++];

    if(receiveMessageBufTail_ == RECEIVE_BUFFER_SIZE)
    {
        receiveMessageBufTail_ = 0;
    }

    return true;
}

//------------------------------------------------------------------------------------------------

void SJA1000CanController::ProcessMessageBuffer()
{
    if(receiveMessageBufHead_ != receiveMessageBufTail_)
    {
        std::unique_lock<std::mutex> lock(receiveMutex_);
        receiveCond_.notify_all();
    }
}

//------------------------------------------------------------------------------------------------

void SJA1000CanController::InterruptHandleTh()
{
    const std::uint64_t timeout = 100'000'000ULL;

    while(1)
    {
        sigevent event;
        event.sigev_notify = SIGEV_UNBLOCK;
        _pulse incomePulse;

        TimerTimeout(CLOCK_REALTIME, _NTO_TIMEOUT_RECEIVE, &event, &timeout, 0);

        MsgReceivePulse(interruptChannel_.chid,
                &incomePulse,
                sizeof(incomePulse),
                0);

        switch(incomePulse.code)
        {
            case TERMINATE_PULSE:
                LOG(info) << "ISR thread stopped";
                return;

            case INTERRUPT_PULSE:
                ProcessMessageBuffer();
                ProcessErrorBuffer();
                ProcessTransmitFlag();
                break;

            default:
                break;
        }
    }
}

//------------------------------------------------------------------------------------------------

void SJA1000CanController::ProcessTransmitFlag()
{
    std::unique_lock<std::mutex> lock(transmitMutex_);

    if(transmitBufferFree_ && !transmitDataQueue_.empty())
    {
        can_frame canFrame = transmitDataQueue_.top();
        transmitDataQueue_.pop();
        TransmitMessage(canFrame);
    }
}

//------------------------------------------------------------------------------------------------

std::uint8_t SJA1000CanController::TransmitMessage(const can_frame& canFrame)
{
    std::uint8_t value = 0;

    EnterCmdRegWriteCriticalSection();

    transmitBufferFree_ = false;

    const std::uint8_t rxTxFrInf = (((canFrame.can_id >> 24) & (EXTENDED_FRAME_FORMAT | REMOTE_REQUEST)) |
            (canFrame.len & DATA_LENGTH_MASK));

    PutByte(&sja1000Map_->RxTxFrInf, rxTxFrInf);
    
    size_t dataOffset = 2;

    if(canFrame.can_id & CAN_EFF_FLAG)
    {
    	const std::uint32_t arbitration = (canFrame.can_id & CAN_EFF_MASK) << 3;

        PutByte(&sja1000Map_->RxTxIdData[0], (arbitration >> 24) & 0xFF);
        PutByte(&sja1000Map_->RxTxIdData[1], (arbitration >> 16) & 0xFF);
        PutByte(&sja1000Map_->RxTxIdData[2], (arbitration >> 8) & 0xFF);
        PutByte(&sja1000Map_->RxTxIdData[3], arbitration & 0xFF);

        dataOffset = 4;
    }
    else
    {
    	const std::uint32_t arbitration = (canFrame.can_id & CAN_SFF_MASK) << 5;

        PutByte(&sja1000Map_->RxTxIdData[0], (arbitration >> 8) & 0xFF);
        PutByte(&sja1000Map_->RxTxIdData[1], arbitration & 0xFF);
    }

    for(size_t i = 0; i < canFrame.len; ++i)
    {
        PutByte(&sja1000Map_->RxTxIdData[i + dataOffset], canFrame.data[i]);
    }

    PutByte(&sja1000Map_->cmndReg, CAN_CM_TR);
    value = GetByte(&sja1000Map_->statusReg);

    LeaveCmdRegWriteCriticalSection();

    return value;
}

//------------------------------------------------------------------------------------------------
