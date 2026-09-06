#include "Core/Log/LogStore.h"
#include <catch2/catch_test_macros.hpp>
#include <thread>

TEST_CASE("Structured logs report retention gaps and advance filtered cursors", "[log-store][v0.9]")
{
    Janus::LogStore store(3);
    store.Append(Janus::LogLevel::Info, "Editor", "one");
    store.Append(Janus::LogLevel::Error, "Lua", "two");
    store.Append(Janus::LogLevel::Info, "Editor", "three");
    store.Append(Janus::LogLevel::Info, "Editor", "four");
    Janus::LogQuery query;
    query.after = 0;
    query.category = "Lua";
    auto read = store.Read(query);
    REQUIRE(read);
    REQUIRE(read.Value().gap);
    REQUIRE(read.Value().entries.size() == 1);
    REQUIRE(read.Value().entries[0].message == "two");
    REQUIRE(read.Value().nextCursor == 4);
    store.Clear();
    store.Append(Janus::LogLevel::Warning, "Lua", "five");
    query.after = 4;
    read = store.Read(query);
    REQUIRE(read.Value().entries[0].sequence == 5);
    REQUIRE_FALSE(read.Value().gap);
}

TEST_CASE("Structured logs bound messages and pages without skipping matching records",
          "[log-store][v0.9]")
{
    Janus::LogStore store;
    store.Append(Janus::LogLevel::Info, "Core", std::string(20000, 'x'));
    store.Append(Janus::LogLevel::Info, "Core", "tail");
    Janus::LogQuery query;
    query.after = 0;
    query.limit = 1;
    auto page = store.Read(query);
    REQUIRE(page);
    REQUIRE(page.Value().entries[0].truncated);
    REQUIRE(page.Value().entries[0].message.size() <= 8192);
    REQUIRE(page.Value().nextCursor == 1);
    query.after = page.Value().nextCursor;
    page = store.Read(query);
    REQUIRE(page.Value().entries[0].message == "tail");
    query.limit = 201;
    REQUIRE_FALSE(store.Read(query));
}

TEST_CASE("Structured log writers assign distinct ordered sequences", "[log-store][v0.9]")
{
    Janus::LogStore store(1000);
    std::thread a(
        [&]
        {
            for (int i = 0; i < 100; ++i)
                store.Append(Janus::LogLevel::Info, "A", "event");
        });
    std::thread b(
        [&]
        {
            for (int i = 0; i < 100; ++i)
                store.Append(Janus::LogLevel::Info, "B", "event");
        });
    a.join();
    b.join();
    Janus::LogQuery query;
    query.after = 0;
    query.limit = 200;
    auto page = store.Read(query);
    REQUIRE(page);
    REQUIRE(page.Value().entries.size() == 200);
    for (Janus::usize i = 0; i < 200; ++i)
        REQUIRE(page.Value().entries[i].sequence == i + 1);
}

TEST_CASE("Log truncation preserves UTF8 character boundaries", "[log-store][v0.9]")
{
    Janus::LogStore store;
    store.Append(Janus::LogLevel::Info, "Utf8", std::string(8191, 'a') + "中文");
    auto page = store.Read();
    REQUIRE(page);
    REQUIRE(page.Value().entries[0].message == std::string(8191, 'a'));
    REQUIRE(page.Value().entries[0].truncated);
}
