
//#define BOOST_NO_CXX11_STD_ALIGN

#include <iostream>

#include <sys/mman.h>
#include <chrono>

#include "can_controller.h"
#include "chip_mapper_io.h"
#include "chip_mapper_memory.h"
#include "sja1000_can_controller.h"
#include "controller_factory.h"
#include "log.h"

//------------------------------------------------------------------------------

std::shared_ptr<CanController>ControllerFactory::CreateController(const unsigned bitRate)
{
    // Enable I/O privileges
    ThreadCtl(_NTO_TCTL_IO, 0);

	if (PCIAttachDevice(VENDOR_PEAK_CAN, DEVICE_MINI_PCEe) == false)
	{
		throw std::runtime_error("Could not attach PCI device");
	}

	if (PCIGetPCANPorts() == false)
	{
		throw std::runtime_error("Incorrect PCI PCAN device ports config");
	}

	PCIInitPCAN();

	auto chipMapper = std::make_unique<ChipMapperMemory>(chipAddr_, chipSize_, PCAN_SHIFT);
    
    LOG(info) << " Base address: " << std:: hex << chipAddr_ << std::dec
              << " Irq: " << irq_
			  << " Bitrate: " << bitRate << " kbit/s";

    ECanBaudRate eCanBaudRate = ECBR_NONE;
        
    switch(bitRate)
    {
        case 1000:
            eCanBaudRate = ECBR_1MB;
            break;
    
        case 800:
            eCanBaudRate = ECBR_800KB;
            break;
    
        case 500:
            eCanBaudRate = ECBR_500KB;
            break;
    
        case 250:
            eCanBaudRate = ECBR_250KB;
            break;
    
        case 125:
            eCanBaudRate = ECBR_125KB;
            break;
    
        case 100:
            eCanBaudRate = ECBR_100KB;
            break;
    
        case 50:
            eCanBaudRate = ECBR_50KB;
            break;
    
        case 20:
            eCanBaudRate = ECBR_20KB;
            break;
    
        case 10:
            eCanBaudRate = ECBR_10KB;
            break;
    
        default:
            throw std::runtime_error("Incompatible bit rate");
    }


    interruptID_ = InterruptAttachEvent(irq_, &interruptSignal_,
                                          _NTO_INTR_FLAGS_PROCESS | _NTO_INTR_FLAGS_TRK_MSK);

    interruptHandleTh_ = std::thread(&ControllerFactory::InterruptHandleTh, this);

	canController_ = std::make_shared<SJA1000CanController>(std::move(chipMapper), eCanBaudRate);
        
    return canController_;
}

//------------------------------------------------------------------------------

void ControllerFactory::InterruptHandleTh()
{
    ThreadCtl (_NTO_TCTL_IO, NULL);
    ThreadCtl (_NTO_TCTL_LOW_LATENCY, NULL );
    
    pthread_setschedprio(pthread_self(), 30);

    while(1)
    {
        const std::uint64_t timeout = 100'000'000ULL;

        sigevent event;
        event.sigev_notify = SIGEV_UNBLOCK;
        _pulse incomePulse;
        int nGetPulseRes;

        TimerTimeout(CLOCK_REALTIME, _NTO_TIMEOUT_RECEIVE, &event, &timeout, 0);

        nGetPulseRes = MsgReceivePulse(interruptChannel_.chid,
                                       &incomePulse,
                                       sizeof(incomePulse),
                                       0);

        if(nGetPulseRes)
        {
            InterruptServiceRoutine();
            continue;
        }

        switch(incomePulse.code) {

            case TERMINATE_PULSE:
                LOG(info) << "ISR thread stopped";
                return;

            case INTERRUPT_PULSE:
                InterruptServiceRoutine();

            default:
                InterruptUnmask(irq_, interruptID_);
                break;
        }

    }

}

//------------------------------------------------------------------------------

void ControllerFactory::InterruptServiceRoutine()
{
    if (configAddr_ != 0)
    {
        std::uint16_t interruptMask = *(std::uint16_t*)(configAddr_ + PCAN_ICR);

        while ((interruptMask & PCAN_ICR_MASK) != 0)
        {
            *(std::uint16_t*)(configAddr_ + PCAN_ICR) = PCAN_ICR_MASK;

            canController_->InterruptServiceRoutine();

            interruptMask = *(std::uint16_t*)(configAddr_ + PCAN_ICR);
        }
    }
}

//------------------------------------------------------------------------------

void ControllerFactory::DeleteController()
{
    if (configAddr_ != 0)
    {
        PCIFreePCAN();

        munmap_device_memory((void*)configAddr_, configSize_);

        configAddr_ = 0;
    }
    
    PCIDetachDevice();

    chipAddr_ = 0;
    configSize_ = 0;
    chipSize_ = 0;

    canController_.reset();

	MsgSendPulse(interruptChannel_.coid, SIGEV_PULSE_PRIO_INHERIT, TERMINATE_PULSE, 0);

	if(interruptHandleTh_.joinable())
	{
		interruptHandleTh_.join();
	}

    LOG(info) << "Done";
}

//------------------------------------------------------------------------------

bool ControllerFactory::PCIAttachDevice(const pci_vid_t nVendorID, const pci_did_t nDeviceID)
{
    LOG(info) << "PCI VendorID: " << std::hex << nVendorID
              << " DeviceID: " << nDeviceID;

    bool result = true;

    bdf_ = pci_device_find(0, nVendorID, nDeviceID, PCI_CCODE_ANY);

    if(bdf_ == PCI_BDF_NONE)
    {
        LOG(error) << "Can't find CAN controller";
        result = false;
    }
    else
    {
        LOG(info) << "Found CAN pci device bdf: " << std::hex << bdf_;

        pci_err_t err;

        pci_dev_hdl_ = pci_device_attach(bdf_, pci_attachFlags_EXCLUSIVE_OWNER, &err);

        if(pci_dev_hdl_ == 0)
        {
            LOG(error) << "Can't attach CAN controller err code: " << pci_strerror(err);
            result = false;
        }
        else
        {
            int_t msiCapId = pci_device_find_capid(bdf_, 0x05);

            if(msiCapId != -1)
            {
                pci_cap_t msiCap = nullptr;
                pci_err_t err;
                err = pci_device_read_cap(bdf_, &msiCap, msiCapId);

                if(err != PCI_ERR_OK)
                {
                    LOG(error) << "Can't read device capability err code: " << pci_strerror(err);
                    result = false;
                }
                else
                {
                    uint_t irq_num = cap_msi_get_nirq(msiCap);

                    LOG(info) << "available " << irq_num << " interrupt(s)";

                    if(irq_num > 0)
                    {
                        err = cap_msi_set_nirq(pci_dev_hdl_, msiCap, 1);

                        if(err != PCI_ERR_OK)
                        {
                            LOG(error) << "Can't reserv irq. err code: " << pci_strerror(err);
                            result = false;
                        }
                    }

                    err = pci_device_cfg_cap_enable(pci_dev_hdl_, pci_reqType_e_MANDATORY, msiCap);

                    if(err != PCI_ERR_OK)
                    {
                        LOG(error) << "Can't enable msi irq. err code: " << pci_strerror(err);
                        result = false;
                    }
                }

                if(msiCap)
                {
                    free(msiCap);
                }
            }
        }
    }

    LOG(info) << "Done: " << result;

    return result;
}

//------------------------------------------------------------------------------

void ControllerFactory::PCIDetachDevice()
{
    if (pci_dev_hdl_ != 0)
    {
        pci_device_detach(pci_dev_hdl_);

        pci_dev_hdl_ = 0;

        LOG(info) << "Device was detached";
    }
}

//------------------------------------------------------------------------------

bool ControllerFactory::PCIGetPCANPorts()
{
    bool result = true;

    pci_ba_t pciBa[2];
    int_t baNum = NELEMENTS(pciBa);
    pci_irq_t pciIrq;
    pci_err_t err;

    err = pci_device_read_ba(pci_dev_hdl_, &baNum, pciBa, pci_reqType_e_UNSPECIFIED);

    if(err != PCI_ERR_OK )
    {
        LOG(error) << "Can't read base addr. err code: " << pci_strerror(err);
        result = false;
    }
    else
    {
        err = pci_device_read_irq(pci_dev_hdl_, nullptr, &pciIrq);
        if(err != PCI_ERR_OK)
        {
            LOG(error) << "Can't get irq num. err code: " << pci_strerror(err);
            result = false;
        }
        else
        {
            irq_ = pciIrq;
            LOG(info) << "CtrlBaseAddress: " << std::hex << pciBa[0].addr
                      << " BaseAddressSize: " << pciBa[0].size
                      << " type: " << pciBa[0].type
                      << " attr: " << pciBa[0].attr ;

            configSize_ = pciBa[0].size;

            configAddr_ = (std::uint8_t*)mmap_device_memory(
                    0,
                    configSize_,
                    PROT_READ | PROT_WRITE | PROT_NOCACHE,
                    0,
                    pciBa[0].addr);

            if (configAddr_ == MAP_FAILED)
            {
                LOG(error) << "Can't map device";
                result = false;
            }

            LOG(info) << "ChipBaseAddress: " << std::hex << pciBa[1].addr
                      << " BaseAddressSize: " << pciBa[1].size
                      << " type: " << pciBa[1].type
                      << " attr: " << pciBa[1].attr ;

            if(pciBa[1].type != pci_asType_e_MEM)
            {
                LOG(error) << "device can't be attached as a memmory";
            }
            else
            {
                chipSize_ = pciBa[1].size;
                chipAddr_ = pciBa[1].addr;
            }
        }
    }

    LOG(info) << "Done: " << result;

    return result;
}


//------------------------------------------------------------------------------

void ControllerFactory::PCIInitPCAN()
{
    pci_err_t err;

    err = pci_device_write_cmd(pci_dev_hdl_, 0x0002, nullptr);

    if(err != PCI_ERR_OK )
    {
        LOG(error) << pci_strerror(err);
        throw std::runtime_error("Can't configure PCAN PCI device");
    }

    err = pci_device_cfg_wr16(pci_dev_hdl_, 0x44, 0, nullptr);


    if(err != PCI_ERR_OK )
    {
        LOG(error) << pci_strerror(err);
        throw std::runtime_error("Can't configure PCAN PCI device");
    }

    // Set GPIO control register 
    *(std::uint16_t*)(configAddr_ + PCAN_GPIOICR + 2) = 0x0005;

    // Enable all channels
    *(configAddr_ + PCAN_GPIOICR) = 0x00;

    // Toggle reset
    *(configAddr_ + PCAN_MISC + 3) = 0x05;

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Leave parport mux mode
    *(configAddr_ + PCAN_MISC + 3) = 0x04;

    // Enable PCAN interrupts
    const std::uint16_t nInterruptMask = *(std::uint16_t*)(configAddr_ + PCAN_ICR + 2);

    *(std::uint16_t*)(configAddr_ + PCAN_ICR + 2) = (nInterruptMask | PCAN_ICR_MASK);
}

//------------------------------------------------------------------------------

void ControllerFactory::PCIFreePCAN()
{
    // Disable PCAN interrupt
    const std::uint16_t nInterruptMask = *(std::uint16_t*)(configAddr_ + PCAN_ICR + 2);

    *(std::uint16_t*)(configAddr_ + PCAN_ICR + 2) = (nInterruptMask & ~PCAN_ICR_MASK);
}

//------------------------------------------------------------------------------

void ControllerFactory::FinializeInterrupt()
{
    if (configAddr_ != 0)
    {
        *(std::uint16_t*)(configAddr_ + PCAN_ICR) = PCAN_ICR_MASK;
    }
}

//------------------------------------------------------------------------------

