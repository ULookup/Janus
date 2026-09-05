#include "Core/Command/CommandBus.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string_view>

namespace
{

class CounterCommand final : public Janus::ICommand
{
public:
    CounterCommand(
        int& value,
        int delta,
        bool failExecute = false,
        bool failUndo = false,
        bool failRedo = false)
        : m_Value(value),
          m_Delta(delta),
          m_FailExecute(failExecute),
          m_FailUndo(failUndo),
          m_FailRedo(failRedo)
    {
    }

    Janus::Result<void> Execute() override
    {
        if (m_FailExecute)
        {
            return Janus::Result<void>::Failure(
                Janus::ErrorCode::InvalidState,
                "execute failed");
        }

        m_Value += m_Delta;
        return Janus::Result<void>::Success();
    }

    Janus::Result<void> Undo() override
    {
        if (m_FailUndo)
        {
            return Janus::Result<void>::Failure(
                Janus::ErrorCode::InvalidState,
                "undo failed");
        }

        m_Value -= m_Delta;
        return Janus::Result<void>::Success();
    }

    Janus::Result<void> Redo() override
    {
        if (m_FailRedo)
        {
            return Janus::Result<void>::Failure(
                Janus::ErrorCode::InvalidState,
                "redo failed");
        }

        m_Value += m_Delta;
        return Janus::Result<void>::Success();
    }

    std::string_view Describe() const noexcept override
    {
        return "Counter";
    }

private:
    int& m_Value;
    int m_Delta = 0;
    bool m_FailExecute = false;
    bool m_FailUndo = false;
    bool m_FailRedo = false;
};

} // namespace

TEST_CASE(
    "CommandBus executes undoes and redoes commands",
    "[core][command][v0.7]")
{
    Janus::CommandBus bus;
    int value = 0;

    REQUIRE(bus.Execute(
        std::make_unique<CounterCommand>(
            value,
            3)));
    REQUIRE(value == 3);
    REQUIRE(bus.CanUndo());
    REQUIRE_FALSE(bus.CanRedo());
    REQUIRE(bus.GetUndoDescription() == "Counter");

    REQUIRE(bus.Undo());
    REQUIRE(value == 0);
    REQUIRE_FALSE(bus.CanUndo());
    REQUIRE(bus.CanRedo());

    REQUIRE(bus.Redo());
    REQUIRE(value == 3);
    REQUIRE(bus.CanUndo());
    REQUIRE_FALSE(bus.CanRedo());
}

TEST_CASE(
    "CommandBus discards redo tail only after successful new execution",
    "[core][command][v0.7]")
{
    Janus::CommandBus bus;
    int value = 0;

    REQUIRE(bus.Execute(
        std::make_unique<CounterCommand>(
            value,
            1)));
    REQUIRE(bus.Execute(
        std::make_unique<CounterCommand>(
            value,
            2)));
    REQUIRE(value == 3);

    REQUIRE(bus.Undo());
    REQUIRE(value == 1);
    REQUIRE(bus.CanRedo());

    const auto failed = bus.Execute(
        std::make_unique<CounterCommand>(
            value,
            5,
            true));
    REQUIRE_FALSE(failed);
    REQUIRE(value == 1);
    REQUIRE(bus.CanRedo());
    REQUIRE(bus.GetHistorySize() == 2);
    REQUIRE(bus.GetCursor() == 1);

    REQUIRE(bus.Execute(
        std::make_unique<CounterCommand>(
            value,
            4)));
    REQUIRE(value == 5);
    REQUIRE_FALSE(bus.CanRedo());
    REQUIRE(bus.GetHistorySize() == 2);
    REQUIRE(bus.GetCursor() == 2);
}

TEST_CASE(
    "CommandBus keeps cursor stable when undo or redo fails",
    "[core][command][v0.7]")
{
    SECTION("undo failure")
    {
        Janus::CommandBus bus;
        int value = 0;

        REQUIRE(bus.Execute(
            std::make_unique<CounterCommand>(
                value,
                2,
                false,
                true)));

        const auto cursor = bus.GetCursor();
        REQUIRE_FALSE(bus.Undo());
        REQUIRE(bus.GetCursor() == cursor);
        REQUIRE(value == 2);
    }

    SECTION("redo failure")
    {
        Janus::CommandBus bus;
        int value = 0;

        REQUIRE(bus.Execute(
            std::make_unique<CounterCommand>(
                value,
                2,
                false,
                false,
                true)));
        REQUIRE(bus.Undo());
        REQUIRE(value == 0);

        const auto cursor = bus.GetCursor();
        REQUIRE_FALSE(bus.Redo());
        REQUIRE(bus.GetCursor() == cursor);
        REQUIRE(value == 0);
    }
}

TEST_CASE(
    "CommandBus clear and bounded history are deterministic",
    "[core][command][v0.7]")
{
    Janus::CommandBus bus(2);
    int value = 0;

    REQUIRE(bus.Execute(
        std::make_unique<CounterCommand>(
            value,
            1)));
    REQUIRE(bus.Execute(
        std::make_unique<CounterCommand>(
            value,
            2)));
    REQUIRE(bus.Execute(
        std::make_unique<CounterCommand>(
            value,
            3)));

    REQUIRE(bus.GetHistorySize() == 2);
    REQUIRE(bus.GetCursor() == 2);
    REQUIRE(value == 6);

    REQUIRE(bus.Undo());
    REQUIRE(value == 3);
    REQUIRE(bus.Undo());
    REQUIRE(value == 1);
    REQUIRE_FALSE(bus.CanUndo());

    bus.Clear();
    REQUIRE(bus.GetHistorySize() == 0);
    REQUIRE(bus.GetCursor() == 0);
    REQUIRE_FALSE(bus.CanUndo());
    REQUIRE_FALSE(bus.CanRedo());
}

TEST_CASE(
    "CommandBus rejects null commands",
    "[core][command][v0.7]")
{
    Janus::CommandBus bus;
    const auto result =
        bus.Execute(nullptr);

    REQUIRE_FALSE(result);
    REQUIRE(
        result.GetError().code
        == Janus::ErrorCode::InvalidArgument);
    REQUIRE(bus.GetHistorySize() == 0);
}
