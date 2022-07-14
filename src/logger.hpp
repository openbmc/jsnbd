#pragma once

#include <iostream>
#include <vector>

#define LOG_LEVEL Debug

namespace Logger
{

struct Struct
{
    constexpr static const int32_t value = 6;
    constexpr static const char* name = "Struct  ";
};

struct Debug
{
    constexpr static const int32_t value = 5;
    constexpr static const char* name = "Debug   ";
};

struct Info
{
    constexpr static const int32_t value = 4;
    constexpr static const char* name = "Info    ";
};

struct Warning
{
    constexpr static const int32_t value = 3;
    constexpr static const char* name = "Warning ";
};

struct Error
{
    constexpr static const int32_t value = 2;
    constexpr static const char* name = "Error   ";
};

struct Critical
{
    constexpr static const int32_t value = 1;
    constexpr static const char* name = "Critical";
};

template <std::size_t Len>
constexpr const char* baseNameImpl(const char (&str)[Len], std::size_t pos)
{
    return pos == 0                                ? str
           : (str[pos] == '/' || str[pos] == '\\') ? str + pos + 1
                                                   : baseNameImpl(str, --pos);
}

template <std::size_t Len>
constexpr const char* baseName(const char (&str)[Len])
{
    return baseNameImpl(str, Len - 1);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    for (const auto& i : v)
    {
        os << i << " ";
    }
    return os;
}

template <typename DefinedLogLevel, typename LogLevel, typename... Args>
constexpr void logImpl(const char* file, int32_t line, const char* fname,
                       Args&&... args)
{
    if constexpr (LogLevel::value <= DefinedLogLevel::value)
    {
        std::cout << "[" << LogLevel::name << "] [" << file << ":" << line
                  << "] " << fname << "(): ";
        (std::cout << ... << args) << std::endl;
    }
}

template <typename LogLevel, typename... Args>
constexpr void log(const char* file, int32_t line, const char* fname,
                   Args&&... args)
{
    logImpl<LOG_LEVEL, LogLevel>(file, line, fname, args...);
}

#define LogMsg(level, ...)                                                     \
    Logger::log<level>(Logger::baseName(__FILE__), __LINE__, __FUNCTION__,     \
                       __VA_ARGS__)

} // namespace Logger
