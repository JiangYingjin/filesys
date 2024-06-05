#pragma once

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Appenders/ConsoleAppender.h>
// #include <plog/Formatters/TxtFormatter.h>
// #include <plog/Formatters/MessageOnlyFormatter.h>

namespace plog
{
    class PlainFomatter
    {
    public:
        // This method returns a header for a new file. In our case it is empty.
        static util::nstring header()
        {
            return util::nstring();
        }

        // This method returns a string from a record.
        static util::nstring format(const Record &record)
        {
            util::nostringstream ss;
            ss << record.getMessage();
            return ss.str();
        }
    };
}

// static plog::ConsoleAppender<plog::MessageOnlyFormatter> consoleAppender;
// plog::init<Console>(plog::info, &consoleAppender);