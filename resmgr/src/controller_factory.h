#pragma once

#include <memory>
#include "non_copyable.h"

extern "C"
{
    #include <pci/cap_pcie.h>
    #include <pci/cap_msi.h>
    #include <pci/pci.h>
}

#include <sys/neutrino.h>
#include "unit_cthread.h"

#include "can_controller.h"

//------------------------------------------------------------------------------

class CanController;

class ControllerFactory : NonCopyable
{
public:

    enum {
        INTERRUPT_PULSE    = _PULSE_CODE_MINAVAIL,
        TERMINATE_PULSE
    };

    static ControllerFactory& Instance() 
    {
        static ControllerFactory instance_;
        return instance_;
    }

    std::shared_ptr<CanController> CreateController(const unsigned bitRate);
    void DeleteController(void);

    void FinializeInterrupt(void);

    ControllerFactory()
        : interruptChannel_(_NTO_CHF_FIXED_PRIORITY)
        , interruptID_(-1)
        , bdf_(0)
        , configAddr_(0)
        , configSize_(0)
        , chipAddr_(0)
        , chipSize_(0)
        , pci_dev_hdl_(0)
        , irq_(-1)
    {
        SIGEV_PULSE_INIT(&interruptSignal_, interruptChannel_.coid,
                SIGEV_PULSE_PRIO_INHERIT, INTERRUPT_PULSE, 0);
    }

private:

    bool PCIAttachDevice(const pci_vid_t nVendorID, const pci_did_t nDeviceID);
    void PCIDetachDevice(void);

    bool PCIGetPCANPorts(void);

    void PCIInitPCAN(void);
    void PCIFreePCAN(void);

    // Only Peak PCAN-miniPCIe SJA1000 PCI controller is supported
    static const pci_vid_t VENDOR_PEAK_CAN  = 0x001C;
//    static const std::uint16_t DEVICE_PCI       = 0x0001;
    static const pci_did_t DEVICE_MINI_PCEe = 0x0008;

    // Interrupt control register
    static const std::uint8_t PCAN_ICR = 0x00;
    
    // General purpose IO interface control register
    static const std::uint8_t PCAN_GPIOICR = 0x18;
    
    // Miscellaneous register
    static const std::uint8_t PCAN_MISC = 0x1C;
    
    // Interrupt mask (channel 0)
    static const std::uint16_t PCAN_ICR_MASK = 0x0002;

    // Mapping address shift 
    const std::uint8_t PCAN_SHIFT = 2;

//////

    void InterruptHandleTh();
    std::thread interruptHandleTh_;

    void InterruptServiceRoutine();

    sigevent interruptSignal_;
    CChannel interruptChannel_;
    int interruptID_;

    std::shared_ptr<CanController> canController_;

//////

    pci_bdf_t bdf_;

    volatile std::uint8_t* configAddr_;
    std::uint64_t configSize_;

    std::uint64_t chipAddr_;
    std::uint64_t chipSize_;

    pci_devhdl_t pci_dev_hdl_;

    pci_irq_t irq_;
};

//------------------------------------------------------------------------------

