/*
 * unit_cthread.h
 *
 */

//------------------------------------------------------------------------------------------------

#pragma once

//------------------------------------------------------------------------------------------------

#include <cstdint>

#include <sys/neutrino.h>
#include <sys/syspage.h>

// this class responsible for creation of communication channels

class CChannel
{
public:

    int chid;  // Channel ID - global - ID of the communication channel
    int coid;  // Connection ID of communication channel and thraed

	CChannel (unsigned flag);
    ~CChannel();

private:

    void ChidCreate(unsigned flag);
    void CoidCreate();
};

//------------------------------------------------------------------------------------------------

#define ABS_TIME    TimeoutToAbsTime() 

class TimeoutToAbsTime
{
private:

    timespec ts_;
    
    void operator=(TimeoutToAbsTime&);
    TimeoutToAbsTime(TimeoutToAbsTime&);

public:

    TimeoutToAbsTime()
    {
        clock_gettime(CLOCK_REALTIME, &ts_);
    };
    
    ~TimeoutToAbsTime() 
    {
        ;
    };
    
    timespec* operator()(std::uint64_t nsec)
    {
        nsec += ts_.tv_nsec;
        
        ts_.tv_nsec = (nsec % 1000000000ULL);
        ts_.tv_sec += (nsec / 1000000000ULL);
        
        return &ts_;
    };
};

//------------------------------------------------------------------------------------------------

