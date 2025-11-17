/*
 * unit_cthread.cpp
 *
 */

//------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "unit_cthread.h"

//------------------------------------------------------------------------------------------------

void CChannel::ChidCreate(unsigned flag)
{
/*
    Flags that can be used to request notification pulses from the kernel or 
    request other changes in behavior; a combination of the following: 
        _NTO_CHF_COID_DISCONNECT 
        _NTO_CHF_DISCONNECT 
        _NTO_CHF_FIXED_PRIORITY 
        _NTO_CHF_NET_MSG 
        _NTO_CHF_REPLY_LEN 
        _NTO_CHF_SENDER_LEN 
        _NTO_CHF_THREAD_DEATH 
        _NTO_CHF_UNBLOCK   
*/  
    
    chid=ChannelCreate(flag); //  required system #include <sys/neutrino.h>

    if (chid == -1)
    {
#ifdef CONSOLE_DEBUG
        printf("channel not created ! \n");
#endif  
        perror (NULL);
        fflush(NULL);
        exit (EXIT_FAILURE);
    }
}

//------------------------------------------------------------------------------------------------

void CChannel::CoidCreate(void)
{
    coid=ConnectAttach (0, 0, chid, 0, 0); // required system include <sys/neutrino.h>

#ifdef CONSOLE_DEBUG
    if (coid==-1)
    {
        printf ("error of ConnectAttach \n");
        perror(NULL);
        fflush(NULL);
        exit (EXIT_FAILURE);
    }
#endif
}

//------------------------------------------------------------------------------------------------

CChannel::CChannel(unsigned flag)
{
    ChidCreate(flag);
    CoidCreate();
}

//------------------------------------------------------------------------------------------------

CChannel::~CChannel()
{
    if(chid != 0)
    {
        ConnectDetach(coid);
    }
    
    if(coid != 0)
    {
        ChannelDestroy(chid);
    }
}

//------------------------------------------------------------------------------------------------


