#include "Core/UUID/UUID.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_set>

TEST_CASE("UUID random values round trip through canonical text", "[core][uuid]")
{
    const Janus::UUID uuid = Janus::UUID::Random();

    REQUIRE(uuid.IsValid());

    const std::string text = uuid.ToString();
    REQUIRE(text.size() == 36);
    REQUIRE(text[8] == '-');
    REQUIRE(text[13] == '-');
    REQUIRE(text[18] == '-');
    REQUIRE(text[23] == '-');
    REQUIRE(text[14] == '4');
    REQUIRE(
        text[19] == '8'
        || text[19] == '9'
        || text[19] == 'a'
        || text[19] == 'b');

    auto parsed = Janus::UUID::Parse(text);
    REQUIRE(parsed);
    REQUIRE(parsed.Value() == uuid);
}

TEST_CASE("UUID parser accepts uppercase hexadecimal", "[core][uuid]")
{
    const auto lower = Janus::UUID::Parse("550e8400-e29b-41d4-a716-446655440000");
    const auto upper = Janus::UUID::Parse("550E8400-E29B-41D4-A716-446655440000");

    REQUIRE(lower);
    REQUIRE(upper);
    REQUIRE(lower.Value() == upper.Value());
    REQUIRE(upper.Value().ToString() == "550e8400-e29b-41d4-a716-446655440000");
}

TEST_CASE("UUID parser rejects malformed canonical values", "[core][uuid]")
{
    const std::string invalidValues[] = {
        "",
        "550e8400e29b41d4a716446655440000",
        "550e8400-e29b-41d4-a716-44665544000",
        "550e8400-e29b-41d4-a716-44665544000z"};

    for (const auto& value : invalidValues)
    {
        const auto parsed = Janus::UUID::Parse(value);
        REQUIRE_FALSE(parsed);
        REQUIRE(parsed.GetError().code == Janus::ErrorCode::InvalidArgument);
    }
}

TEST_CASE("UUID hash is stable for equal UUID values", "[core][uuid]")
{
    const auto first =
        Janus::UUID::Parse("550e8400-e29b-41d4-a716-446655440000").Value();
    const auto second =
        Janus::UUID::Parse("550E8400-E29B-41D4-A716-446655440000").Value();

    REQUIRE(Janus::UUIDHash{}(first) == Janus::UUIDHash{}(second));

    std::unordered_set<Janus::UUID, Janus::UUIDHash> values;
    values.insert(first);
    values.insert(second);
    REQUIRE(values.size() == 1);
}
