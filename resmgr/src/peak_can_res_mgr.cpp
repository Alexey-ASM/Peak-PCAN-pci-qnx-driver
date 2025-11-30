#include <cstdlib>
#include <iostream>

#include <string.h>

#include "log.h"

#include "can_manager.h"
#include "controller_factory.h"

//------------------------------------------------------------------------------

#define MAJOR_VERSION     1
#define MINOR_VERSION     0
#define RELEASE       0

//------------------------------------------------------------------------------

void PrintUsage()
{
    std::cout <<
    "Usage: " << " options\n"
    " -h            Display this usage information\n"
	" -s baudrate   Set bus baudrate (125 (K))\n"
    " -t            Test variant, not daemon mode\n"
    " -a            After\n"
    " -b            Before\n"
    " -d name       Alternate registration name\n"
    " -B size       Buffer size bufsize=2^size\n";
}

//------------------------------------------------------------------------------------------------

CanManager*       canManager = 0;
resmgr_context_t* ctp = 0;
char* __progname;

std::string drvRegPrefix("/dev/");

//------------------------------------------------------------------------------------------------

void Finalize(void)
{
    std::cout << "Stopping " << drvRegPrefix;
    LOG(info) << "Stopping " << drvRegPrefix;

    if (ctp != 0)
    {
        resmgr_context_free(ctp);
        ctp = 0;
    }

    ControllerFactory::Instance().DeleteController();

    if (0 != canManager)
    {
        delete canManager;
    }

    LOG(info) << drvRegPrefix << " Stopped.";
}

//------------------------------------------------------------------------------------------------
//Abort handler

void SigTermHandler(int nSigno, siginfo_t* /*info*/, void* /*other*/)
{
	LOG(info) << "Signal '" << strsignal(nSigno) << "' was caught" << std::endl;

    Finalize();

    exit(nSigno);
}

//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    std::cout << __progname << " Version: " << MAJOR_VERSION << "." << MINOR_VERSION << "." << RELEASE << std::endl;

    const int nUid = geteuid();
    if(nUid != 0)
    {
        if( seteuid( 0 ) == -1 )
        {
        	LOG(error) << "Can't set eUID: " <<  geteuid() << " old: " << nUid << std::endl;
            std::cerr << "Can't set eUID: " <<  geteuid() << " old: " << nUid << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    std::string drvRegName("can0");

    resmgr_connect_funcs_t  connect_funcs;
    resmgr_io_funcs_t       io_funcs;
    iofunc_attr_t           attr;

    dispatch_t              *dpp = 0;

    int                     option = 0;
    unsigned                bufSize = 8;
    bool                    testMode = false;

    unsigned 				bitRate = 125;

    //The flags argument specifies additional information to control the pathname resolution.
    unsigned int resourceFlag = 0;

    while((option = getopt(argc, argv, "abr:B:d:htVs:")) != -1)
    {
        switch (option)
        {

        case 'a':

            if(0 != resourceFlag)
            {
                std::cout << "Before and After flag can't be stated together" << std::endl;
                exit(EXIT_FAILURE);
            }
            resourceFlag = _RESMGR_FLAG_AFTER;
            break;

        case 'b':

            if(0 != resourceFlag)
            {
                std::cout << "Before and After flag can't be stated together" << std::endl;
                exit(EXIT_FAILURE);
            }
            resourceFlag = _RESMGR_FLAG_BEFORE;
            break;

        case 'B':

            bufSize = atoi(optarg);
            if(bufSize > 24)
            {
                std::cout << "Error size of buffer" << std::endl;
                exit(EXIT_FAILURE);
            }
            std::cout << "BufferSize: " << bufSize << " 0x" << std::hex << (0xFFFFFFFF >> (32 - bufSize)) + 1 << std::endl;
            LOG(info) << "BufferSize: " << bufSize << " 0x" << std::hex << (0xFFFFFFFF >> (32 - bufSize)) + 1;
            break;


        case 'd':

            drvRegName = optarg;
            break;

        case 'h':

            PrintUsage();
            exit(EXIT_SUCCESS);
            break;

        case 's':

            bitRate = atoi(optarg);
            break;

        case 't':

            testMode = true;
            break;

        case 'r':

            if (chdir(optarg))
            {
                std::cerr << "chroot can't change path!" << std::endl;
                LOG(error) << "chroot can't change path!" << std::endl;
                exit(EXIT_FAILURE);
            }
            else
            {
                std::cout << "dir: " << optarg << std::endl;
            }
            break;

        case 'V':
            {
                exit(EXIT_SUCCESS);
            }
        }
    }

    LOG(info) << "Starting";

    int      exitStatus = EXIT_SUCCESS;

    struct   sigaction act;
    sigset_t set;

    sigemptyset(&set);

    act.sa_flags = 0;
    act.sa_mask = set;
    act.sa_sigaction = &SigTermHandler;

    sigaction(SIGTERM, &act, 0);
    sigaction(SIGINT,  &act, 0);
    sigaction(SIGABRT, &act, 0);
    sigaction(SIGBUS,  &act, 0);
    sigaction(SIGSEGV, &act, 0);
    sigaction(SIGKILL, &act, 0);
    sigaction(SIGFPE,  &act, 0);
    sigaction(SIGILL,  &act, 0);

    try {
        canManager = new CanManager(ControllerFactory::Instance().CreateController(bitRate), bufSize);

        if(testMode == false)
        {
            procmgr_daemon( EXIT_SUCCESS, PROCMGR_DAEMON_NOCLOSE | PROCMGR_DAEMON_NOCHDIR);
        }

        /* initialize functions for handling messages */
        iofunc_func_init( _RESMGR_CONNECT_NFUNCS, &connect_funcs,
                          _RESMGR_IO_NFUNCS, &io_funcs );

        /* initialize attribute structure */
        iofunc_attr_init( &attr, S_IFNAM | 0666, 0, 0 );
        attr.inode = 1;
        attr.nbytes = 0;

        // Replace io_read function
        io_funcs.read = canManager->io_read;

        //replace write functions
        io_funcs.write = canManager->io_write;

        //replace open functions
        connect_funcs.open = canManager->io_open;

        //replace notify functions
        io_funcs.notify = canManager->io_notify;

        //replace devctl functions
        io_funcs.devctl = canManager->io_devctl;

        //replace close functions
        io_funcs.close_dup = canManager->io_close_dup;

        //replace close ocb functions
        io_funcs.close_ocb = canManager->io_close_ocb;

        io_funcs.unblock = canManager->io_unblock;

        iofunc_mount_t  mount = {};
        iofunc_funcs_t  mount_funcs = {};

        mount_funcs.nfuncs = _IOFUNC_NFUNCS;
        mount_funcs.ocb_calloc = canManager->ocb_calloc;
        mount_funcs.ocb_free = canManager->ocb_free;

        attr.mount = &mount;
        attr.mount->funcs = &mount_funcs;

        /* initialize dispatch interface */
        if ((dpp = dispatch_create()) == 0)
        {

            std::cerr << "Unable to allocate dispatch handle: " << argv[0] << std::endl;
            LOG(error) << "Unable to allocate dispatch handle: " << argv[0];
            exitStatus = EXIT_FAILURE;
        }
        else
        {

            /*  attach our device name (passing in the POSIX defaults
                      the iofunc_func_init and iofunc_attr_init functions)
                    */

            drvRegPrefix += drvRegName;

            resmgr_attr_t resmgr_attr = {};

            /* initialize resource manager attributes */
            resmgr_attr.nparts_max = 1;
            resmgr_attr.msg_max_size = 2048;

            if(resmgr_attach(dpp, &resmgr_attr, drvRegPrefix.c_str(), _FTYPE_ANY, resourceFlag,
                             &connect_funcs, &io_funcs, &attr) == -1)
            {
                std::cerr << "Unable to attach name: " << argv[0] << std::endl;
                LOG(error) << "Unable to attach name: " << argv[0];
                exitStatus = EXIT_FAILURE;
            }
            else
            {
                std::cout << "Resource manager is registered as: " << drvRegPrefix << std::endl;
                LOG(info) << "Resource manager is registered as: " << drvRegPrefix;

                /* allocate a context structure */
                ctp = resmgr_context_alloc(dpp);

                /* start the resource manager message loop */
                while (1)
                {
                    if ((ctp = resmgr_block(ctp)) == 0)
                    {
                        std::cerr << "Block error" << std::endl;
                        LOG(error) << "Block error";

                        exitStatus = EXIT_FAILURE;
                        break;
                    }
                    resmgr_handler(ctp);
                }
            }
        }
    }

    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        LOG(error) << e.what();
        exitStatus = EXIT_FAILURE;
    }

    catch (...)
    {
        std::cerr << "unexpected can manager error!" << std::endl;
        LOG(error) << "Unexpected error";
        exitStatus = EXIT_FAILURE;
    }

    Finalize();

    return exitStatus;
}
