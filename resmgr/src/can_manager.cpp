#include <devctl.h>
#include <cstring>
#include "log.h"
#include "can_manager.h"

#include <iostream>

//----------------------------------------------------------------------

uint32_t CanManager::queueSize_;
uint32_t CanManager::queueBottom_;
uint32_t CanManager::queueHead_;

std::mutex CanManager::queueMutex_;

can_frame* CanManager::canMessageQueue_ = 0;

std::shared_ptr<CanController> CanManager::canController_;

std::vector<CanManager::DelayElement> CanManager::delayedQueue_;

iofunc_notify_t CanManager::notify_[3];

//----------------------------------------------------------------------

CanManager::CanManager(std::shared_ptr<CanController> canController, uint32_t nQueueSize)
 : terminate_(false)
 , filling_(true)
{
    IOFUNC_NOTIFY_INIT(notify_);

    canController_ = canController;

    queueBottom_ = 0;
    queueHead_ = 0;

    if(nQueueSize > 24) 
    {
        throw std::runtime_error("Error buffer size");
    }

    queueSize_ = 0xFFFFFFFF >> (32 - nQueueSize);

    LOG(info) << "Message queue Size: " << queueSize_ + 1;

    canMessageQueue_ = new can_frame[queueSize_ + 1];

    
    if(canController_->InitController() == false) 
    {
        throw std::runtime_error("Controller initialization Error");
    }

    // Starting Data Receive Thread
    dataReceiveThread_ = std::thread(&CanManager::DataReceiveThread, this);
}

//----------------------------------------------------------------------

CanManager::~CanManager()
{
    terminate_ = true;

    canController_.reset();

    LOG(info) << "Data receive thread stopped";

    if(dataReceiveThread_.joinable())
    {
    	dataReceiveThread_.join();
    }

    if (0 != canMessageQueue_)
    {
        delete[] canMessageQueue_;
    }
}

//----------------------------------------------------------------------

void* CanManager::DataReceiveThread()
{
    bool erase = false;
    
    pthread_setschedprio(pthread_self(), 30);

    while(1)
    {
        if(terminate_)
        {
            return 0;
        }

        if(canController_->ReadMessage(canMessageQueue_[queueHead_ & queueSize_]))
        {
            std::lock_guard<std::mutex> lock(queueMutex_);

            DelayedQueueIterator tdqi = delayedQueue_.begin();

            while(tdqi != delayedQueue_.end()) 
            {

                erase = false;

                //check Data pointer equivalence
                if(queueHead_ == tdqi->ocb_->defaultOCB_.offset) 
                {

                    //check acceptance filter
                    const bool bAccept = CheckFilter(canMessageQueue_[queueHead_ & queueSize_], tdqi->ocb_->canMessageFilter_);

                    if(bAccept == true) 
                    {

                        switch (tdqi->type_) 
                        {
                            case DelayElement::ET_REPLY:
                                //send delayed data
                                MsgReply(tdqi->rcvId_, sizeof(can_frame), &canMessageQueue_[queueHead_ & queueSize_], sizeof(can_frame));
                                erase = true;

                                break;
                            case DelayElement::ET_NOTIFY:

                                if(SIGEV_NONE != tdqi->ocb_->notifyEvent_.ev32.sigev_notify) 
                                {

                                    tdqi->ocb_->notifyEvent_.ev32.sigev_value.sival_int |= _NOTIFY_COND_INPUT;

                                    MsgDeliverEvent(tdqi->rcvId_, &tdqi->ocb_->notifyEvent_.ev);
                                    tdqi->ocb_->notifyEvent_.ev32.sigev_notify = SIGEV_NONE;

                                }
                                erase = true;
                                break;
                            default:
                                break;
                        }
                    }

                    if((DelayElement::ET_REPLY == tdqi->type_) || (false == erase)) 
                    {
                        //advance the offset by the number of messages returned to the client.
                        ++(tdqi->ocb_->defaultOCB_.offset);
                    }
                }

                //delete or advise delayed pointer
                if(erase) 
                {
                    tdqi = delayedQueue_.erase(tdqi);
                } 
                else 
                {
                    ++tdqi;
                }
            }

            if(queueSize_ < ++queueHead_) 
            {
                filling_ = false;
            }

            if(!filling_)
            {
                ++queueBottom_;
            }
        }
    }

    return 0;
}

//----------------------------------------------------------------------

int CanManager::io_read (resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb)
{
    int         status;
    int         nBlock = 0;

    if ((status = iofunc_read_verify (ctp, msg, &(ocb->defaultOCB_), &nBlock)) != EOK)
        return (status);

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return (ENOSYS);

    /*
     *  On all reads (first and subsequent), calculate
     *  how many bytes we can return to the client,
     *  based upon the number of bytes available (nleft)
     *  and the client's buffer size
     */

    if(sizeof(can_frame) != msg->i.nbytes)
        return (EINVAL);

    std::unique_lock<std::mutex> lock(queueMutex_);
    //check data pointer maybe we miss some messages

    if((ocb->defaultOCB_.offset > queueHead_) || (ocb->defaultOCB_.offset < queueBottom_))
    {
        ocb->defaultOCB_.offset = queueBottom_;
    }

    //try to find new message
    while(ocb->defaultOCB_.offset != queueHead_)
    {
        if(CheckFilter(canMessageQueue_[ocb->defaultOCB_.offset & queueSize_], ocb->canMessageFilter_)) 
        {

            MsgReply(ctp->rcvid, sizeof(can_frame), &canMessageQueue_[ocb->defaultOCB_.offset & queueSize_], sizeof(can_frame));

            //advance the offset by the number of messages returned to the client.
            ++ocb->defaultOCB_.offset;

            lock.unlock();

            /* mark the access time as invalid (we just accessed it) */
            if (msg->i.nbytes > 0)
                ocb->defaultOCB_.attr->flags |= IOFUNC_ATTR_ATIME | IOFUNC_ATTR_DIRTY_TIME;

            return (_RESMGR_NOREPLY);
        }
        //advance the offset
        ++ocb->defaultOCB_.offset;
    }

    //haven't new message, check O_NONBLOCK
    if(nBlock) 
    {
        //nonblock reply databuffer is empty
        MsgReply(ctp->rcvid, 0, 0, 0);
    } 
    else 
    {
        //push to queue for wait new data
        delayedQueue_.push_back(DelayElement(DelayElement::ET_REPLY, ctp->rcvid, ocb));
    }

    return (_RESMGR_NOREPLY);
}

//----------------------------------------------------------------------

int CanManager::io_open (resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle, void *extra)
{
    //call default open function
    int nRetval = iofunc_open_default(ctp, msg, handle, extra);

    //get pointer to OCB by CTP
    RESMGR_OCB_T *ocb = (RESMGR_OCB_T*)(resmgr_ocb(ctp));

    //set ocb->offset pointer to bottom of data queue
    if(0 != ocb)
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if(O_APPEND & msg->connect.ioflag) 
        {
            ocb->defaultOCB_.offset = queueHead_;
        } 
        else 
        {
            ocb->defaultOCB_.offset = queueBottom_;
        }
    }

    return (_RESMGR_ERRNO(nRetval)); //obsolete
}

//----------------------------------------------------------------------

int CanManager::io_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb)
{
    can_frame canFrame;

    // verify that the device is opened for write
    if(0 == (ocb->defaultOCB_.ioflag & 0x02)) 
    {
        return (EINVAL);
    }

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return (ENOSYS);

    if(sizeof(can_frame) != msg->i.nbytes)
        return (EINVAL);

    // read the data from the client
    if(resmgr_msgread(ctp, &canFrame, sizeof(can_frame), sizeof(msg->i)) == -1)
    {
        return (errno);
    }

    // Put data to the send buffer
    canController_->WriteMessage(canFrame);

    // set up the number of bytes for the client's "write"
    // function to return
    _IO_SET_WRITE_NBYTES (ctp, sizeof(can_frame));

    /* mark the access time as invalid (we just accessed it) */

    if (msg->i.nbytes > 0)
        ocb->defaultOCB_.attr->flags |= IOFUNC_ATTR_ATIME | IOFUNC_ATTR_DIRTY_TIME;

    // tell the resource manager library to do the reply, and that it was okay
    return (EOK);
}

//----------------------------------------------------------------------

int CanManager::io_notify(resmgr_context_t *ctp, io_notify_t *msg, RESMGR_OCB_T *ocb)
{
    int trig = 0;

    /*
     * 'trig' will tell iofunc_notify() which conditions are currently
     * satisfied.
    */

    trig = _NOTIFY_COND_OUTPUT;         /* clients can always give us data */

    std::lock_guard<std::mutex> lock(queueMutex_);

    //advance message pointer if out of range
    if((ocb->defaultOCB_.offset > queueHead_) || (ocb->defaultOCB_.offset < queueBottom_))
    {
        ocb->defaultOCB_.offset = queueBottom_;
    }

    //check presence of new message in buffer
    while(ocb->defaultOCB_.offset != queueHead_)
    {
        if(CheckFilter(canMessageQueue_[ocb->defaultOCB_.offset & queueSize_], ocb->canMessageFilter_)) 
        {
            //we have new data in buffer
            trig |= _NOTIFY_COND_INPUT;      /* we have some data available */

            break;
        }
        ++ocb->defaultOCB_.offset;
    }


    //check notify request and put it to queue
    if((_NOTIFY_ACTION_POLLARM == msg->i.action) && (_NOTIFY_COND_INPUT & msg->i.flags)) 
    {
        msg->o.flags = trig & msg->i.flags;

        if(0 == msg->o.flags) 
        {

            //push to queue for wait new data
            ocb->notifyEvent_.ev32 = msg->i.event;

            delayedQueue_.push_back(DelayElement(DelayElement::ET_NOTIFY, ctp->rcvid, ocb));
        }
    }

    return (EOK);
}

//----------------------------------------------------------------------

int CanManager::io_close_dup(resmgr_context_t *ctp, io_close_t *msg, RESMGR_OCB_T *ocb)
{
    return iofunc_close_dup_default(ctp, msg, &(ocb->defaultOCB_));
}

//----------------------------------------------------------------------

int CanManager::io_close_ocb (resmgr_context_t *ctp, void *msg, RESMGR_OCB_T *ocb)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);

        DelayedQueueIterator tdqi = delayedQueue_.begin();

        while(tdqi != delayedQueue_.end()) 
        {
            if(ocb == tdqi->ocb_) 
            {
                MsgReply(tdqi->rcvId_, EOK, NULL, 0);
                tdqi = delayedQueue_.erase(tdqi);
            }
            else 
            {
                ++tdqi;
            }
        }
    }
    return iofunc_close_ocb_default(ctp, msg, &(ocb->defaultOCB_));
}

//----------------------------------------------------------------------

int CanManager::io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb)
{
    int32_t nRetVal = iofunc_devctl_default(ctp, msg, &(ocb->defaultOCB_));

    if(_RESMGR_DEFAULT != nRetVal) 
    {
        return nRetVal;
    }

    uint32_t* data;

    switch(msg->i.dcmd){
    case EDCMD_SET_MASK :
        //set mask

        if(sizeof(CanMessageFilter) != msg->i.nbytes) 
        {
            return ENOSYS;
        }

        data = (uint32_t*)(_DEVCTL_DATA(msg->i));

        memcpy(&ocb->canMessageFilter_, data, sizeof(CanMessageFilter));

        break;
    default :
        return ENOSYS;
    }


    /*
    memset(&msg->o, 0, sizeof(msg->o));
    SETIOV(ctp->iov, &msg->o, sizeof(msg->o));
    return (_RESMGR_NPARTS(1));
    */

    return (EOK);
}

//----------------------------------------------------------------------

IOFUNC_OCB_T* CanManager::ocb_calloc (resmgr_context_t */*ctp*/, IOFUNC_ATTR_T */*device*/)
{
    IOFUNC_OCB_T *ocb;

    ocb = (IOFUNC_OCB_T*)(calloc (1, sizeof(IOFUNC_OCB_T)));

    if (0 == ocb) 
    {
        return 0;
    }

    ocb->notifyEvent_.ev32.sigev_notify = SIGEV_NONE;

    return ocb;
}

//----------------------------------------------------------------------

void CanManager::ocb_free (IOFUNC_OCB_T *ocb)
{
    free (ocb);
}

//----------------------------------------------------------------------

bool CanManager::CheckFilter(const can_frame& canFrame, const CanMessageFilter& filter)
{
    const uint32_t nArb = canFrame.can_id & CAN_EFF_MASK;

    switch (filter.type_) {
    case CanMessageFilter::ET_AMASK:
        if(( nArb & filter.acceptanceMask_) ==
            (filter.acceptancePattern_ & filter.acceptanceMask_)) 
        {

            return true;
        }
        break;

    case CanMessageFilter::ET_RANGE:
        if(( nArb >= filter.lower_) && (nArb <= filter.upper_)) 
        {

            return true;
        }

        break;
    default:
        return true;
        break;
    }

    return false;
}

//----------------------------------------------------------------------

int CanManager::io_unblock (resmgr_context_t *ctp, io_pulse_t *msg, RESMGR_OCB_T *ocb)
{
    //unblock read return -1

    MsgReply(ctp->rcvid, -1 , 0, 0);

    return iofunc_unblock_default(ctp, msg, &ocb->defaultOCB_);
}

//----------------------------------------------------------------------
