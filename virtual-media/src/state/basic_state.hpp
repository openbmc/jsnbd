#pragma once

#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "utils/log-wrapper.hpp"

#include <memory>
#include <string_view>
#include <utility>

struct BasicState
{
    explicit BasicState(interfaces::MountPointStateMachine& machine) :
        machine{machine}
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
    T* getIf()
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
    explicit BasicStateT(interfaces::MountPointStateMachine& machine) :
        BasicState(machine)
    {}

    ~BasicStateT() override
    {
        LOGGER_DEBUG("cleaning state: {}", T::stateName());
    }

    BasicStateT(const BasicStateT&) = delete;
    BasicStateT(BasicStateT&&) = delete;

    BasicStateT& operator=(const BasicStateT&) = delete;
    BasicStateT& operator=(BasicStateT&&) = delete;

    std::unique_ptr<BasicState> onEnter() override
    {
        return nullptr;
    }

    std::unique_ptr<BasicState> handleEvent(Event event) final
    {
        return std::visit([this](auto e) {
            return static_cast<T*>(this)->handleEvent(std::move(e));
        }, std::move(event));
    }

    std::string_view getStateName() const final
    {
        return T::stateName();
    }
};
