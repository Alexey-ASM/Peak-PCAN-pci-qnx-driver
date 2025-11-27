/*
 * CanManager.h
 *
 *  Created on: 01.02.2010
 *      Author: malishev_a
 */

#pragma once

#define IOFUNC_OCB_T struct CanExtendedOCB

#include <thread>
#include <mutex>

#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/procmgr.h>

#include <vector>

#include <canrm.h>
#include <can_controller.h>

#include <can.h>

struct CanExtendedOCB
{
    iofunc_ocb_t defaultOCB_;

    CanMessageFilter canMessageFilter_;

    union
    {
        struct sigevent ev;
        struct __sigevent32 ev32;
        struct __sigevent64 ev64;
    } notifyEvent_;

};

class CanManager
{
public:
    CanManager(std::shared_ptr<CanController> canController, uint32_t nQueueSize = 3);
    virtual ~CanManager();

    static int io_read  (resmgr_context_t *ctp, io_read_t   *msg, RESMGR_OCB_T *ocb);
    static int io_open  (resmgr_context_t *ctp, io_open_t   *msg, RESMGR_HANDLE_T *handle, void *extra);
    static int io_write (resmgr_context_t *ctp, io_write_t  *msg, RESMGR_OCB_T *ocb);
    static int io_notify(resmgr_context_t *ctp, io_notify_t *msg, RESMGR_OCB_T *ocb);
    static int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb);
    static int io_close_dup (resmgr_context_t *ctp, io_close_t *msg, RESMGR_OCB_T *ocb);
    static int io_close_ocb (resmgr_context_t *ctp, void *msg, RESMGR_OCB_T *ocb);
    static int io_unblock (resmgr_context_t *ctp, io_pulse_t *msg, RESMGR_OCB_T *ocb);

    static IOFUNC_OCB_T* ocb_calloc (resmgr_context_t *ctp, IOFUNC_ATTR_T *device);
    static void ocb_free (IOFUNC_OCB_T *ocb);

private:

    static iofunc_notify_t notify_[3];  /* notification list used by iofunc_notify*() */

    static std::shared_ptr<CanController> canController_;

    static can_frame* canMessageQueue_;

    static uint32_t queueSize_;

    static uint32_t queueBottom_;
    static uint32_t queueHead_;

    static std::mutex queueMutex_;

    bool terminate_;
    bool filling_;

    // Receive Data Thread
    std::thread dataReceiveThread_;
    void* DataReceiveThread();

    // Queue for delayed data request
    struct DelayElement 
    {
        enum EType
        {
            ET_UNDEFINED,
            ET_REPLY,
            ET_NOTIFY
        } type_;

        int rcvId_;
        RESMGR_OCB_T *ocb_;

        DelayElement(EType type, int rcvId, RESMGR_OCB_T *ocb)
         : type_(type)
         , rcvId_(rcvId)
         , ocb_(ocb)
         {}
    };

    static bool CheckFilter(const can_frame& canFrame, const CanMessageFilter& filter);

    static std::vector<DelayElement> delayedQueue_;
    typedef std::vector<DelayElement>::iterator DelayedQueueIterator;
};

