// Copyright Enva Division

#include "LogThreadId.h"

#include <string>
#include <sstream>
#include <thread>


DEFINE_LOG_CATEGORY(LogThreadIdentifier);

/**
 * @brief Since std::this_thread::get_id() returns an opaque value, this
 * function uses its standard stream behavior (the << operator) to serialize
 * the value into a human readable form.
 * @param Message A message to include before the thread ID
 */
void LogThreadId(const FString Message)
{
    std::stringstream stream;
    stream << std::this_thread::get_id();
    FString Id(stream.str().c_str());
    UE_LOG(LogThreadIdentifier, Log,
        TEXT("%s - thread ID %s"), *Message, *Id);
}

