#include "log.h"

#include <iostream>

//------------------------------------------------------------------------------------------------

Log::Log(LogMessageLevel level)
: logLevel_(level)
{
    /* You should use the name of your process to name the buffer set. */
    bufferConfig_.buffer_set_name = __progname;

    /* This buffer is configured below. */
    bufferConfig_.num_buffers = 1;

    /* Request an initial verbosity level. */

    bufferConfig_.verbosity_level = level;

    bufferConfig_.buffer_config[0].buffer_name = "main_buffer";
    bufferConfig_.buffer_config[0].num_pages = 7;

    /* Register the buffer set. */

    if( -1 == slog2_register( &bufferConfig_, bufferHandle_, 0 ) ) {
        std::cerr << "Error registering slogger2 buffer!" << std::endl;
    }
}

//------------------------------------------------------------------------------------------------

Log::~Log()
{
}

//------------------------------------------------------------------------------------------------

LogWrapper::~LogWrapper()
{
    if (logMessage_)
    {
        owner_.Write(std::move(logMessage_));
    }
}

//------------------------------------------------------------------------------------------------
