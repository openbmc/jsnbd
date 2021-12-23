#pragma once

#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "utils/log-wrapper.hpp"

#include <memory>
#include <string>
#include <utility>

struct BasicState
{
    BasicState(interfaces::MountPointStateMachine& machine) : machine{machine}
    {}
    virtual ~BasicState() = default;

    BasicState(const BasicState& state) = delete;
    BasicState(BasicState&&) = delete;

    BasicState& operator=(const BasicState&) = delete;
    BasicState& operator=(BasicState&& state) = delete;

    virtual std::unique_ptr<BasicState> handleEvent(Event event) = 0;
    virtual std::unique_ptr<BasicState> onEnter() = 0;
    virtual std::string_view getStateName() const = 0;

    template <class T>
    T* get_if()
    {
        if (getStateName() == T::stateName())
        {
            return static_cast<T*>(this);
        }
        return nullptr;
    }

    interfaces::MountPointStateMachine& machine;
};

template <class T>
struct BasicStateT : public BasicState
{
    BasicStateT(interfaces::MountPointStateMachine& machine) :
        BasicState(machine)
    {}

    ~BasicStateT() override
    {
        LOGGER_DEBUG << "cleaning state: " << T::stateName();
    }

    std::unique_ptr<BasicState> onEnter() override
    {
        return nullptr;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
    std::unique_ptr<BasicState> handleEvent(Event event) final
    {
        return std::visit(
            [this](auto e) {
            return static_cast<T*>(this)->handleEvent(std::move(e));
        },
            std::move(event));
    }
#pragma GCC diagnostic pop

    std::string_view getStateName() const final
    {
        return T::stateName();
    }
};
