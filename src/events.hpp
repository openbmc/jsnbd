#pragma once

#include "interfaces/mount_point_state_machine.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

struct BasicEvent
{
    BasicEvent(const char* eventName) : eventName(eventName)
    {}

    virtual ~BasicEvent() = default;

    const char* eventName;
};

struct RegisterDbusEvent : public BasicEvent
{
    RegisterDbusEvent(
        std::shared_ptr<sdbusplus::asio::connection> bus,
        std::shared_ptr<sdbusplus::asio::object_server> objServer) :
        BasicEvent(__FUNCTION__),
        bus(bus), objServer(objServer)
    {
    }

    std::shared_ptr<sdbusplus::asio::connection> bus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::function<void(void)> emitMountEvent;
};

struct MountEvent : public BasicEvent
{
    explicit MountEvent(
        std::optional<interfaces::MountPointStateMachine::Target> target) :
        BasicEvent(__FUNCTION__),
        target(std::move(target))
    {
    }

    MountEvent(const MountEvent&) = delete;
    MountEvent(MountEvent&& other) :
        BasicEvent(__FUNCTION__), target(std::move(other.target))
    {
        other.target = std::nullopt;
    }

    MountEvent& operator=(const MountEvent&) = delete;
    MountEvent& operator=(MountEvent&& other)
    {
        target = std::nullopt;
        std::swap(target, other.target);
        return *this;
    }

    std::optional<interfaces::MountPointStateMachine::Target> target;
};

struct UnmountEvent : public BasicEvent
{
    UnmountEvent() : BasicEvent(__FUNCTION__)
    {
    }
};

struct SubprocessStoppedEvent : public BasicEvent
{
    SubprocessStoppedEvent() : BasicEvent(__FUNCTION__)
    {
    }
};

struct UdevStateChangeEvent : public BasicEvent
{
    explicit UdevStateChangeEvent(const StateChange& devState) :
        BasicEvent(__FUNCTION__), devState{devState}
    {
    }

    StateChange devState;
};

using Event = std::variant<RegisterDbusEvent, MountEvent, UnmountEvent,
                           SubprocessStoppedEvent, UdevStateChangeEvent>;
