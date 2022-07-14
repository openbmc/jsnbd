/*
 Copyright 2022 Intel Corporation.

 This software and the related documents are Intel copyrighted materials,
 and your use of them is governed by the express license under which they
 were provided to you ("License"). Unless the License provides otherwise,
 you may not use, modify, copy, publish, distribute, disclose or transmit
 this software or the related documents without Intel's prior written
 permission.

 This software and the related documents are provided as is, with
 no express or implied warranties, other than those that are expressly
 stated in the License.
*/

#pragma once

#include <phosphor-logging/log.hpp>

#include <filesystem>
#include <sstream>

static constexpr const auto kCurrentLogLevel = phosphor::logging::level::DEBUG;

template <phosphor::logging::level L>
class LoggerWrapper
{
  public:
    template <typename T>
    LoggerWrapper& operator<<(const std::vector<T>& vec)
    {
        for (const auto& item : vec)
        {
            stringstream << item << " ";
        }
        return *this;
    }

    template <typename T>
    LoggerWrapper& operator<<([[maybe_unused]] T const& value)
    {
        if constexpr (L <= kCurrentLogLevel)
        {
            stringstream << value;
        }
        return *this;
    }

    ~LoggerWrapper()
    {
        if constexpr (L <= kCurrentLogLevel)
        {
            stringstream << std::endl;
            phosphor::logging::log<L>(stringstream.str().c_str());
        }
    }

  private:
    std::ostringstream stringstream;
};

#define LOGGER_CRITICAL                                                        \
    if constexpr (phosphor::logging::level::CRIT <= kCurrentLogLevel)          \
    LoggerWrapper<phosphor::logging::level::CRIT>()                            \
        << std::filesystem::path(__FILE__).filename().c_str() << ":"           \
        << __LINE__ << " "
#define LOGGER_ERROR                                                           \
    if constexpr (phosphor::logging::level::ERR <= kCurrentLogLevel)           \
    LoggerWrapper<phosphor::logging::level::ERR>()                             \
        << std::filesystem::path(__FILE__).filename().c_str() << ":"           \
        << __LINE__ << " "
#define LOGGER_WARNING                                                         \
    if constexpr (phosphor::logging::level::WARNING <= kCurrentLogLevel)       \
    LoggerWrapper<phosphor::logging::level::WARNING>()                         \
        << std::filesystem::path(__FILE__).filename().c_str() << ":"           \
        << __LINE__ << " "
#define LOGGER_INFO                                                            \
    if constexpr (phosphor::logging::level::INFO <= kCurrentLogLevel)          \
    LoggerWrapper<phosphor::logging::level::INFO>()                            \
        << std::filesystem::path(__FILE__).filename().c_str() << ":"           \
        << __LINE__ << " "
#define LOGGER_DEBUG                                                           \
    if constexpr (phosphor::logging::level::DEBUG <= kCurrentLogLevel)         \
    LoggerWrapper<phosphor::logging::level::DEBUG>()                           \
        << std::filesystem::path(__FILE__).filename().c_str() << ":"           \
        << __LINE__ << " "
