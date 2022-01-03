#pragma once

#include "state/basic_state.hpp"

struct TestStateA : public BasicStateT<TestStateA>
{
    static std::string_view stateName()
    {
        return "TestStateA";
    }

    TestStateA(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}

    template <class AnyEvent>
    std::unique_ptr<BasicState> handleEvent([[maybe_unused]] AnyEvent event)
    {
        return nullptr;
    }
};

struct TestStateB : public BasicStateT<TestStateB>
{
    static std::string_view stateName()
    {
        return "TestStateB";
    }

    TestStateB(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}
};

struct TestStateC : public BasicStateT<TestStateC>
{
    static std::string_view stateName()
    {
        return "TestStateC";
    }

    TestStateC(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}

    std::unique_ptr<BasicState> onEnter() override
    {
        return std::make_unique<TestStateB>(machine);
    }
};

struct TestStateD : public BasicStateT<TestStateD>
{
    static std::string_view stateName()
    {
        return "TestStateD";
    }

    TestStateD(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}
};

struct TestStateE : public BasicStateT<TestStateE>
{
    static std::string_view stateName()
    {
        return "TestStateE";
    }

    TestStateE(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}
};

struct TestStateF : public BasicStateT<TestStateF>
{
    static std::string_view stateName()
    {
        return "TestStateF";
    }

    TestStateF(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}
};

struct TestStateG : public BasicStateT<TestStateG>
{
    static std::string_view stateName()
    {
        return "TestStateG";
    }

    TestStateG(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}
};

struct TestStateH : public BasicStateT<TestStateH>
{
    static std::string_view stateName()
    {
        return "TestStateH";
    }

    TestStateH(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}
};

struct TestStateI : public BasicStateT<TestStateI>
{
    static std::string_view stateName()
    {
        return "TestStateI";
    }

    TestStateI(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}

    std::unique_ptr<BasicState>
        handleEvent([[maybe_unused]] const RegisterDbusEvent& event)
    {
        return std::make_unique<TestStateD>(machine);
    }

    std::unique_ptr<BasicState>
        handleEvent([[maybe_unused]] const MountEvent& event)
    {
        return std::make_unique<TestStateE>(machine);
    }

    std::unique_ptr<BasicState>
        handleEvent([[maybe_unused]] const UnmountEvent& event)
    {
        return std::make_unique<TestStateF>(machine);
    }

    std::unique_ptr<BasicState>
        handleEvent([[maybe_unused]] const SubprocessStoppedEvent& event)
    {
        return std::make_unique<TestStateG>(machine);
    }

    std::unique_ptr<BasicState>
        handleEvent([[maybe_unused]] const UdevStateChangeEvent& event)
    {
        return std::make_unique<TestStateH>(machine);
    }
};
