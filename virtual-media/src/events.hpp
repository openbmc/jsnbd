#pragma once

#include "interfaces/mount_point_state_machine.hpp"
#include "system.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <variant>

struct BasicEvent
{
    explicit BasicEvent(const char* eventName) : eventName(eventName) {}

    const char* eventName;
};

struct RegisterDbusEvent : public BasicEvent
{
    RegisterDbusEvent(
        std::shared_ptr<sdbusplus::asio::connection> bus,
        std::shared_ptr<sdbusplus::asio::object_server> objServer) :
        BasicEvent(__func__), bus(std::move(bus)),
        objServer(std::move(objServer))
    {}

    std::shared_ptr<sdbusplus::asio::connection> bus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
};

struct MountEvent : public BasicEvent
{
    explicit MountEvent(
        std::optional<interfaces::MountPointStateMachine::Target> target) :
        BasicEvent(__func__), target(std::move(target))
    {}

    MountEvent(const MountEvent&) = delete;
    MountEvent(MountEvent&& other) noexcept :
        BasicEvent(__func__), target(std::move(other.target))
    {
        other.target = std::nullopt;
    }

    MountEvent& operator=(const MountEvent&) = delete;
    MountEvent& operator=(MountEvent&& other) noexcept
    {
        target = std::nullopt;
        std::swap(target, other.target);
        return *this;
    }

    ~MountEvent() = default;

    std::optional<interfaces::MountPointStateMachine::Target> target;
};

struct UnmountEvent : public BasicEvent
{
    UnmountEvent() : BasicEvent(__func__) {}
};

struct SubprocessStoppedEvent : public BasicEvent
{
    SubprocessStoppedEvent() : BasicEvent(__func__) {}
};

struct UdevStateChangeEvent : public BasicEvent
{
    explicit UdevStateChangeEvent(const StateChange& devState) :
        BasicEvent(__func__), devState{devState}
    {}

    StateChange devState;
};

using Event = std::variant<RegisterDbusEvent, MountEvent, UnmountEvent,
                           SubprocessStoppedEvent, UdevStateChangeEvent>;
