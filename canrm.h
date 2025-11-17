#pragma once


#ifdef __QNX__
#include <devctl.h>
#else // __QNX__
#define _POSIX_DEVDIR_TO 0
#endif // __QNX__


//==============================================================================

enum ECanResMgr_DCMD
{
    EDCMD_UNDEFINED     = 0,
    EDCMD_SET_MASK      = 1 + _POSIX_DEVDIR_TO,
};

//==============================================================================

struct CanMessageFilter
{
    enum EType
    {
        ET_DISABLED     = 0,
        ET_AMASK        = 1,
        ET_RANGE        = 2,
    } type_;

    union
    {
        std::uint32_t acceptanceMask_;
        std::uint32_t lower_;
    };

    union
    {
        std::uint32_t acceptancePattern_;
        std::uint32_t upper_;
    };

    CanMessageFilter()
     : type_(ET_DISABLED)
     , lower_(0)
     , upper_(0)
    { }

    CanMessageFilter(EType type, std::uint32_t first, std::uint32_t second)
     : type_(type)
     , lower_(first)
     , upper_(second)
    { }

    CanMessageFilter(const CanMessageFilter &tcmf)
     : type_(tcmf.type_)
     , lower_(tcmf.lower_)
     , upper_(tcmf.upper_)
    { }


};

//==============================================================================

