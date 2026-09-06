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

    Janus::Result<Janus::usize> EstimateUndoBytes() const override
    {
        return Janus::Result<Janus::usize>::Success(64);
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

TEST_CASE("Transactions preserve redo until commit and publish one reversible group",
          "[transaction][v0.9]")
{
    using namespace Janus;
    CommandBus bus;
    int value = 0;
    REQUIRE(bus.Execute(std::make_unique<CounterCommand>(value, 10)));
    REQUIRE(bus.Undo());
    auto tx = bus.BeginTransaction(CommandActor::Agent);
    REQUIRE(tx);
    REQUIRE(
        bus.Execute(std::make_unique<CounterCommand>(value, 2), CommandActor::Agent, tx.Value()));
    REQUIRE(bus.GetHistorySize() == 1);
    REQUIRE_FALSE(bus.Execute(std::make_unique<CounterCommand>(value, 100)));
    REQUIRE_FALSE(bus.Undo());
    REQUIRE(bus.RollbackTransaction(tx.Value()));
    REQUIRE(value == 0);
    REQUIRE(bus.CanRedo());
    REQUIRE(bus.RollbackTransaction(tx.Value()));
    tx = bus.BeginTransaction(CommandActor::Agent);
    REQUIRE(tx);
    REQUIRE(
        bus.Execute(std::make_unique<CounterCommand>(value, 2), CommandActor::Agent, tx.Value()));
    REQUIRE(
        bus.Execute(std::make_unique<CounterCommand>(value, 3), CommandActor::Agent, tx.Value()));
    REQUIRE(bus.CommitTransaction(tx.Value()));
    REQUIRE(bus.CommitTransaction(tx.Value()));
    REQUIRE(value == 5);
    REQUIRE(bus.GetHistorySize() == 1);
    REQUIRE(bus.Undo());
    REQUIRE(value == 0);
    REQUIRE(bus.Redo());
    REQUIRE(value == 5);
    REQUIRE(bus.GetActivity().size() >= 8);
}
TEST_CASE("Transaction failures reverse prior commands and failed compensation freezes writes",
          "[transaction][v0.9]")
{
    using namespace Janus;
    CommandBus bus;
    int value = 0;
    auto tx = bus.BeginTransaction(CommandActor::Agent);
    REQUIRE(tx);
    REQUIRE(
        bus.Execute(std::make_unique<CounterCommand>(value, 2), CommandActor::Agent, tx.Value()));
    REQUIRE_FALSE(bus.Execute(std::make_unique<CounterCommand>(value, 1, true), CommandActor::Agent,
                              tx.Value()));
    REQUIRE(value == 0);
    REQUIRE_FALSE(bus.HasTransaction());
    REQUIRE(bus.GetHistorySize() == 0);
    tx = bus.BeginTransaction(CommandActor::Agent);
    REQUIRE(tx);
    REQUIRE(bus.Execute(std::make_unique<CounterCommand>(value, 2, false, true),
                        CommandActor::Agent, tx.Value()));
    REQUIRE(bus.Execute(std::make_unique<CounterCommand>(value, 3, false, false, true),
                        CommandActor::Agent, tx.Value()));
    REQUIRE(bus.CommitTransaction(tx.Value()));
    REQUIRE_FALSE(bus.Undo());
    REQUIRE(bus.RecoveryRequired());
    REQUIRE(bus.GetCursor() == 1);
    REQUIRE_FALSE(bus.Execute(std::make_unique<CounterCommand>(value, 99)));
    REQUIRE_FALSE(bus.Redo());
}
TEST_CASE("Grouped undo failure compensates without moving history cursor", "[transaction][v0.9]")
{
    using namespace Janus;
    CommandBus bus;
    int value = 0;
    auto tx = bus.BeginTransaction(CommandActor::Agent);
    REQUIRE(tx);
    REQUIRE(bus.Execute(std::make_unique<CounterCommand>(value, 2, false, true),
                        CommandActor::Agent, tx.Value()));
    REQUIRE(
        bus.Execute(std::make_unique<CounterCommand>(value, 3), CommandActor::Agent, tx.Value()));
    REQUIRE(bus.CommitTransaction(tx.Value()));
    REQUIRE_FALSE(bus.Undo());
    REQUIRE(value == 5);
    REQUIRE_FALSE(bus.RecoveryRequired());
    REQUIRE(bus.GetCursor() == 1);
}

TEST_CASE("Transaction quotas abort without losing redo and observers cannot reenter",
          "[transaction][activity][v0.9]")
{
    using namespace Janus;
    CommandBus bus;
    int value = 0;
    REQUIRE(bus.Execute(std::make_unique<CounterCommand>(value, 5)));
    REQUIRE(bus.Undo());
    auto tx = bus.BeginTransaction(CommandActor::Agent);
    REQUIRE(tx);
    for (int i = 0; i < 64; ++i)
        REQUIRE(bus.Execute(std::make_unique<CounterCommand>(value, 1), CommandActor::Agent,
                            tx.Value()));
    REQUIRE_FALSE(
        bus.Execute(std::make_unique<CounterCommand>(value, 1), CommandActor::Agent, tx.Value()));
    REQUIRE(value == 0);
    REQUIRE(bus.CanRedo());
    bool rejected = false;
    bus.SetObserver([&](const CommandReceipt&)
                    { rejected = !bus.Execute(std::make_unique<CounterCommand>(value, 100)); });
    REQUIRE(bus.Redo());
    REQUIRE(rejected);
    REQUIRE(value == 5);
    bus.SetObserver({});
    for (int i = 0; i < 2005; ++i)
        bus.RecordOperation(CommandActor::System, "bounded", Result<void>::Success());
    REQUIRE(bus.GetActivity().size() == 2000);
    REQUIRE(bus.GetActivity().front().sequence > 1);
}
TEST_CASE("Grouped redo compensates applied prefix on a later failure", "[transaction][v0.9]")
{
    using namespace Janus;
    CommandBus bus;
    int value = 0;
    auto tx = bus.BeginTransaction(CommandActor::Agent);
    REQUIRE(tx);
    REQUIRE(
        bus.Execute(std::make_unique<CounterCommand>(value, 2), CommandActor::Agent, tx.Value()));
    REQUIRE(bus.Execute(std::make_unique<CounterCommand>(value, 3, false, false, true),
                        CommandActor::Agent, tx.Value()));
    REQUIRE(bus.CommitTransaction(tx.Value()));
    REQUIRE(bus.Undo());
    REQUIRE_FALSE(bus.Redo());
    REQUIRE(value == 0);
    REQUIRE(bus.GetCursor() == 0);
    REQUIRE_FALSE(bus.RecoveryRequired());
}

TEST_CASE("Unbounded commands and oversized undo reservations are rejected before mutation",
          "[transaction][v0.9]")
{
    using namespace Janus;
    class BudgetCommand final : public ICommand
    {
      public:
        bool& touched;
        bool supported;
        BudgetCommand(bool& flag, bool support) : touched(flag), supported(support) {}
        Result<void> Execute() override
        {
            touched = true;
            return Result<void>::Success();
        }
        Result<void> Undo() override
        {
            touched = false;
            return Result<void>::Success();
        }
        Result<void> Redo() override
        {
            return Execute();
        }
        std::string_view Describe() const noexcept override
        {
            return "Budget";
        }
        Result<usize> EstimateUndoBytes() const override
        {
            return supported ? Result<usize>::Success(8 * 1024 * 1024 + 1)
                             : ICommand::EstimateUndoBytes();
        }
    };
    for (bool supported : {false, true})
    {
        CommandBus bus;
        bool touched = false;
        auto tx = bus.BeginTransaction(CommandActor::Agent);
        REQUIRE(tx);
        REQUIRE_FALSE(bus.Execute(std::make_unique<BudgetCommand>(touched, supported),
                                  CommandActor::Agent, tx.Value()));
        REQUIRE_FALSE(touched);
        REQUIRE_FALSE(bus.HasTransaction());
        REQUIRE(bus.GetHistorySize() == 0);
    }
}
