#pragma once

#include <atomic>
#include <thread>
#include <queue>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>

#include <sys/neutrino.h>

#include "can_controller.h"

//------------------------------------------------------------------------------------------------

class SJA1000CanController : public CanController
{
public:
    
    SJA1000CanController(std::unique_ptr<ChipMapperBase> chipMapper, ECanBaudRate baudRate);

    virtual ~SJA1000CanController();
    
    virtual bool InitController();
    virtual void CloseController();

    virtual bool WriteMessage(const can_frame& canFrame);

    virtual bool ReadMessage(can_frame& canFrame);

private:

    enum ModeRegister
    {
        CAN_MR_SM          = 0x10, // Sleep mode 
        CAN_MR_AFM         = 0x08, // Acceptance filter mode
        CAN_MR_STM         = 0x04, // Self test mode
        CAN_MR_LOM         = 0x02, // Listen only mode
        CAN_MR_RM          = 0x01  // Reset mode
    };

    enum ClkDivideRegister
    {
        CAN_CDR_CANM       = 0x80, // Set for PeliCAN mode
        CAN_CDR_CBP        = 0x40, // Bypass CAN input
        CAN_CDR_RXINTEN    = 0x20, // TX1 output to be used as a dedicated receive interrupt output
        CAN_CDR_CLKOFF     = 0x08  // Clock off
    };

    enum StatusRegister
    {
        CAN_SR_BOS         = 0x80, // Bus off
        CAN_SR_ES          = 0x40, // Error status
        CAN_SR_TS          = 0x20, // Transmitting a message
        CAN_SR_RS          = 0x10, // Receiving a message
        CAN_SR_TCS         = 0x08, // Transmission complete
        CAN_SR_TBS         = 0x04, // Transmit buffer status
        CAN_SR_DOS         = 0x02, // Data overrun status
        CAN_SR_RBS         = 0x01  // Receive buffer status
    };

    enum ComandRegister
    {
        CAN_CM_COS         = 0x08, // Clear overrun status
        CAN_CM_RRB         = 0x04, // Release receive buffer
        CAN_CM_AT          = 0x02, // Abort transmission
        CAN_CM_TR          = 0x01  // Transmission request
    };

    enum InterruptRegister
    {
        CAN_IR_BEI         = 0x80, // Bus error interrupt
        CAN_IR_ALI         = 0x40, // Arbitration lost interrupt
        CAN_IR_EPI         = 0x20, // Error Passive interrupt
        CAN_IR_WUI         = 0x10, // Wake-up interrupt
        CAN_IR_OVERRUN     = 0x08, // Overrun interrupt
        CAN_IR_ERRINT      = 0x04, // Error interrupt
        CAN_IR_TX          = 0x02, // TX interrupt
        CAN_IR_RX          = 0x01  // RX interrupt
    };

    enum MessageConfigurationRegister
	{
    	EXTENDED_FRAME_FORMAT	= 0x80,
    	REMOTE_REQUEST			= 0x40,
		DATA_LENGTH_MASK 		= 0x0F
	};

    #pragma pack(push,1)
    struct SJA1000Map
    {
        tPort8 ModeReg;            // 00 Mode register
        tPort8 cmndReg;            // 01 command register
        tPort8 statusReg;          // 02 status register
        tPort8 intrReg;            // 03 interrupt register
        tPort8 intrEnReg;          // 04 interrupt enable register
        tPort8 Reserved;           // 05 Reserved
        tPort8 busTim0;            // 06 bus timing register 0
        tPort8 busTim1;            // 07 bus timing register 1
        tPort8 outCtrl;            // 08 output Control
        tPort8 testReg;            // 09 test register
        tPort8 Reserved1;          // 0A Reserved
        tPort8 ArbLostCap;         // 0B arbitration lost capture
        tPort8 ErrCodeCap;         // 0C error code capture
        tPort8 ErrWarLim;          // 0D error warning limit
        tPort8 RxErrCount;         // 0E RX error counter
        tPort8 TxErrCount;         // 0F TX error counter
        tPort8 RxTxFrInf;          // 10 for Read RX frame information, for Write TX frame information(Operation mode)
                                   // acceptence code 0 (Reset mode)
        tPort8 RxTxIdData[12];     // 11-1C for Read RX identifier and data, for Read TX identifier and data
                                   // 1D acceptence code 1-3 and acceptence mask 0-3 (Reset mode)
        tPort8 RxMsgCount;         // 1E RX message counter
        tPort8 RxBufStartAddr;     // 1F RX buffer start address
        tPort8 clkDiv;             // 20 clock divider
        tPort8 IntrlRamAddr[64];   // 21-... internal RAM address
        tPort8 IntrlRamAddrTx[13]; // internal RAM address TX buffer
    } ;
    #pragma pack(pop) 

    SJA1000Map* sja1000Map_;

    static const unsigned  RECEIVE_BUFFER_SIZE = 1024;
    static const unsigned  ERROR_BUFFER_SIZE = 1024;
    static const unsigned  MAX_RECEIVED_MESSAGES = 8;

    std::uint8_t TransmitMessage(const can_frame& canFrame);

    virtual void InterruptServiceRoutine();

    virtual void InterruptHandleTh();

    void ProcessErrorBuffer();
    void ProcessMessageBuffer();
    void ProcessTransmitFlag();

    can_frame receiveMessageBuf_[RECEIVE_BUFFER_SIZE];
    std::atomic_uint receiveMessageBufHead_;
    std::atomic_uint receiveMessageBufTail_;

    std::uint8_t errorMessageBuf_[ERROR_BUFFER_SIZE];
    std::atomic_uint errorBufHead_;
    std::atomic_uint errorBufTail_;

    sigevent intSignal_;
    CChannel interruptChannel_;

    std::mutex receiveMutex_;
    std::condition_variable receiveCond_;

    std::mutex transmitMutex_;
    std::condition_variable transmitCond_;

    struct Comp
    {
    	bool operator() (const can_frame& lhs, const can_frame& rhs)
		{
			return (lhs.can_id & CAN_EFF_MASK) > (rhs.can_id & CAN_EFF_MASK);
		};
    };

    std::priority_queue<can_frame, std::vector<can_frame>, Comp> transmitDataQueue_;

    intrspin_t interruptSpinLock_;

    std::atomic_bool transmitBufferFree_;

protected:

    enum 
    {
        INTERRUPT_PULSE    = _PULSE_CODE_MINAVAIL,
        TERMINATE_PULSE
    };

    virtual bool IsThereDevice();

    inline void ReceiveMessage();

    inline const SJA1000Map* GetBasePtr() { return sja1000Map_; }

    inline void TransmitBufferFree() { transmitBufferFree_ = true; }

    inline void AddError(std::uint8_t error);

    inline void EnterCmdRegWriteCriticalSection() { InterruptLock(&interruptSpinLock_); }
    inline void LeaveCmdRegWriteCriticalSection() { InterruptUnlock(&interruptSpinLock_); }

};

//------------------------------------------------------------------------------------------------

