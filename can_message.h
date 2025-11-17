#pragma once

#include <cstring>
#include <cinttypes>
#include <ostream>
#include <iomanip>

#include <memory>

//------------------------------------------------------------------------------------------------

static const size_t CAN_MAX_DATA_SIZE = 8;

// Structure of base CAN message
#pragma pack(push, 1)
struct CanMessage
{
    std::uint8_t        extFrameFormat_;
    std::uint8_t        remoteRequest_;
/*
    // arbitration field
    // standart frame:
    // frameId_[0] bit    7    6    5    4    3    2    1    0
    // arbitration bit   id10 id09 id08 id07 id06 id05 id04 id03
    // frameId_[1] bit    7    6    5    4    3    2    1    0
    // arbitration bit   id02 id01 id00

    // extended frame:
    // frameId_[0] bit    7    6    5    4    3    2    1    0
    // arbitration bit   id28 id27 id26 id25 id24 id23 id22 id21
    // frameId_[1] bit    7    6    5    4    3    2    1    0
    // arbitration bit   id20 id19 id18 id17 id16 id15 id14 id13
    // frameId_[2] bit    7    6    5    4    3    2    1    0
    // arbitration bit   id12 id11 id10 id09 id08 id07 id06 id05
    // frameId_[3] bit    7    6    5    4    3    2    1    0
    // arbitration bit   id04 id03 id02 id01 id00
*/
    std::uint8_t        frameId_[4];
    std::uint8_t        dataSize_;
    std::uint8_t        data_[CAN_MAX_DATA_SIZE];

    CanMessage()
    {
        memset(this, 0, sizeof(CanMessage));
    }

    CanMessage(const CanMessage *p)
    {
        if(0 != p)
        memcpy(this, p, sizeof(CanMessage));
    }

    CanMessage(std::uint32_t arbitration, std::uint8_t extFrameFormat, std::uint8_t remoteRequest, std::uint8_t dataSize, const std::uint8_t* data = 0)
     : extFrameFormat_(extFrameFormat)
     , remoteRequest_(remoteRequest)
     , dataSize_(dataSize)
    {
        if(extFrameFormat) 
        {
            frameId_[0] = static_cast<std::uint8_t>(arbitration >> 21);
            frameId_[1] = static_cast<std::uint8_t>(arbitration >> 13);
            frameId_[2] = static_cast<std::uint8_t>(arbitration >> 5);
            frameId_[3] = static_cast<std::uint8_t>(arbitration << 3);
        } 
        else 
        {
            frameId_[0] = static_cast<std::uint8_t>(arbitration >> 3);
            frameId_[1] = static_cast<std::uint8_t>(arbitration << 5);
            frameId_[2] = 0u;
            frameId_[3] = 0u;
        }

        if(dataSize && data)
        {
            memcpy(data_, data, dataSize_);
        }
    }

    unsigned Arbitration() const
    {
        if(extFrameFormat_) 
        {
            return std::uint32_t((frameId_[0] << 21) | (frameId_[1] << 13) | (frameId_[2] << 5) | (frameId_[3] >> 3));
        }

        return std::uint32_t((frameId_[0] << 3) | (frameId_[1] >> 5));
    }

    bool operator<(const CanMessage& a) const
    {
        if(frameId_[0] < a.frameId_[0])
        {
            return true;
        }

        if(frameId_[0] == a.frameId_[0])
        {
            if(frameId_[1] < a.frameId_[1])
            {
                return true;
            }

            if(frameId_[1] == a.frameId_[1])
            {
                if(frameId_[2] < a.frameId_[2])
                {
                    return true;
                }

                if(frameId_[2] == a.frameId_[2])
                {
                    return (frameId_[3] < a.frameId_[3]);
                }
            }
        }

        return false;
    }
    
    bool operator>(const CanMessage& a) const
    {
        if(frameId_[0] > a.frameId_[0])
        {
            return true;
        }

        if(frameId_[0] == a.frameId_[0])
        {
            if(frameId_[1] > a.frameId_[1])
            {
                return true;
            }

            if(frameId_[1] == a.frameId_[1])
            {
                if(frameId_[2] > a.frameId_[2])
                {
                    return true;
                }

                if(frameId_[2] == a.frameId_[2])
                {
                    return (frameId_[3] > a.frameId_[3]);
                }
            }
        }

        return false;
    }

} /*__attribute__ ((packed))*/;
#pragma pack(pop)

//------------------------------------------------------------------------------------------------

template <class ostream_type>
ostream_type& operator<< (ostream_type& stream, const CanMessage& canMessage)
{
    stream << "Arb: 0x" << std::hex << std::setw(3) << std::setfill('0') << canMessage.Arbitration()
           << " size: " << std::dec << int(canMessage.dataSize_);

    if (canMessage.dataSize_ != 0)
    {
        stream << std::hex << " data:";
    }

    for (int ii = 0; ii < canMessage.dataSize_; ++ii)
    {
        stream << " 0x" << std::setw(2) << std::setfill('0') << int(canMessage.data_[ii]);
    }

    return stream;
}


//------------------------------------------------------------------------------------------------

