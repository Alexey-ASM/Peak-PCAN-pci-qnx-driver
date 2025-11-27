#pragma once

//------------------------------------------------------------------------------------------------

#include <memory>
#include <sstream>

#include <sys/slog2.h>

//================================================================================================

#ifndef LOG_LEVEL
    #ifdef NDEBUG
        #define LOG_LEVEL SLOG2_INFO
    #else
        #define LOG_LEVEL SLOG2_DEBUG1
    #endif
#endif

//================================================================================================

extern char* __progname;

//================================================================================================

class LogInstance;

enum LogMessageLevel
{
    fatal	= SLOG2_CRITICAL,      // A fatal severity message
    error	= SLOG2_ERROR,         // An error severity message
    warning = SLOG2_WARNING,       // A warning severity message
    info	= SLOG2_INFO,          // An informational severity message
    debug	= SLOG2_DEBUG1,        // A debug severity message
    trace	= SLOG2_DEBUG2         // A trace severity message
};

//================================================================================================

struct Message
{
	LogMessageLevel level_;
    std::ostringstream message_;

    Message(LogMessageLevel level)
     : level_(level)
    {
        switch (level)
        {
            case trace :
            	message_ << "<T> ";
                break;
            case debug :
            	message_ << "<D> ";
                break;
            case info :
            	message_ << "<I> ";
                break;
            case warning :
            	message_ << "<W> ";
                break;
            case error :
            	message_ << "<E> ";
                break;
            case fatal :
            	message_ << "<F> ";
                break;
            default:
            	message_ << level ;
        }

        message_ << std::showbase << std::uppercase;
    }
};

//================================================================================================

class Log;

//================================================================================================

class LogWrapper
{
private:

	Log& owner_;

	std::unique_ptr<Message> logMessage_;

	LogWrapper(const LogWrapper&) = delete;
	LogWrapper& operator=(const LogWrapper&) = delete;

	LogWrapper& operator=(LogWrapper&&) = delete;

public:
    LogWrapper(Log& owner, LogMessageLevel logLevel)
    : owner_(owner)
    {
    	logMessage_ = std::make_unique<Message>(logLevel);
    }

    LogWrapper(Log& owner, LogMessageLevel logLevel, const std::string& sFunctionName)
    : owner_(owner)
    {
		logMessage_ = std::make_unique<Message>(logLevel);
		logMessage_->message_ << sFunctionName << ": ";
    }

	LogWrapper(LogWrapper&&) = default;

    ~LogWrapper();

    template <typename T>
    std::ostream& operator<< (const T& message)
    {
		logMessage_->message_ << message;
		return logMessage_->message_;
    }
};

//================================================================================================

class Log
{
public:
	Log(LogMessageLevel level = info);
	virtual ~Log();

	void SetLogLevel(LogMessageLevel level);

    LogWrapper Write(const LogMessageLevel level, const std::string& functionName)
    {
        LogWrapper logWrapper(*this, level, functionName);
        return logWrapper;
    };

    inline void Write(std::unique_ptr<Message>&& message)
    {
        slog2c(bufferHandle_[0], 0, message->level_, message->message_.str().c_str());
    }

private:

    LogMessageLevel 			logLevel_;

    slog2_buffer_set_config_t   bufferConfig_;
    slog2_buffer_t              bufferHandle_[1];
};

//------------------------------------------------------------------------------------------------

class LogInstance
{
public:

    static Log& Instance()
    {
        static Log emInstance_(LogMessageLevel(LOG_LEVEL));
        return emInstance_;
    }
};

//------------------------------------------------------------------------------------------------

#ifndef LOG
#define LOG(VALUE) LogInstance::Instance().Write(VALUE, __func__)
#endif // LOG

//------------------------------------------------------------------------------------------------
