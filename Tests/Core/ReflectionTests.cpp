#include "Core/Reflection/ReflectionRegistry.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace
{

struct TestComponent
{
    Janus::f32 speed = 1.0f;
    bool enabled = true;
};

Janus::PropertyDescriptor MakeSpeedProperty()
{
    return Janus::PropertyDescriptor{
        Janus::MakePropertyId("Test.speed"),
        "speed",
        "speed",
        Janus::PropertyType::Float32,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* test =
                static_cast<const TestComponent*>(component);
            return Janus::Result<Janus::PropertyValue>::Success(
                Janus::PropertyValue{test->speed});
        },
        [](void* component, const Janus::PropertyValue& value)
        {
            auto* test = static_cast<TestComponent*>(component);
            test->speed = std::get<Janus::f32>(value);
            return Janus::Result<void>::Success();
        }};
}

Janus::PropertyDescriptor MakeEnabledProperty()
{
    return Janus::PropertyDescriptor{
        Janus::MakePropertyId("Test.enabled"),
        "enabled",
        "enabled",
        Janus::PropertyType::Bool,
        true,
        true,
        true,
        {},
        [](const void* component)
        {
            const auto* test =
                static_cast<const TestComponent*>(component);
            return Janus::Result<Janus::PropertyValue>::Success(
                Janus::PropertyValue{test->enabled});
        },
        [](void* component, const Janus::PropertyValue& value)
        {
            auto* test = static_cast<TestComponent*>(component);
            test->enabled = std::get<bool>(value);
            return Janus::Result<void>::Success();
        }};
}

Janus::ComponentDescriptor MakeTestComponent(
    std::string name = "Test")
{
    return Janus::ComponentDescriptor{
        Janus::MakeComponentTypeId(name),
        name,
        name,
        true,
        true,
        {MakeSpeedProperty(), MakeEnabledProperty()}};
}

} // namespace

TEST_CASE(
    "Reflection PropertyValue reports neutral authoring types",
    "[core][reflection][v0.7]")
{
    REQUIRE(
        Janus::GetPropertyType(Janus::PropertyValue{true})
        == Janus::PropertyType::Bool);
    REQUIRE(
        Janus::GetPropertyType(Janus::PropertyValue{Janus::i32{7}})
        == Janus::PropertyType::Int32);
    REQUIRE(
        Janus::GetPropertyType(Janus::PropertyValue{Janus::f32{2.0f}})
        == Janus::PropertyType::Float32);
    REQUIRE(
        Janus::GetPropertyType(Janus::PropertyValue{std::string{"value"}})
        == Janus::PropertyType::String);
    REQUIRE(
        Janus::GetPropertyType(
            Janus::PropertyValue{Janus::Vector2{1.0f, 2.0f}})
        == Janus::PropertyType::Vector2);
    REQUIRE(
        Janus::GetPropertyType(
            Janus::PropertyValue{Janus::ColorValue{}})
        == Janus::PropertyType::Color);
    REQUIRE(
        Janus::GetPropertyType(
            Janus::PropertyValue{Janus::AssetReferenceValue{}})
        == Janus::PropertyType::AssetReference);
}

TEST_CASE(
    "Reflection registry registers and finds component metadata",
    "[core][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    auto registered = registry.RegisterComponent(MakeTestComponent());

    REQUIRE(registered);
    REQUIRE(registry.GetComponentCount() == 1);

    const auto* byName = registry.FindComponent("Test");
    REQUIRE(byName != nullptr);
    REQUIRE(byName->id == Janus::MakeComponentTypeId("Test"));

    const auto* byId =
        registry.FindComponent(Janus::MakeComponentTypeId("Test"));
    REQUIRE(byId == byName);

    const auto* bySerialized =
        registry.FindComponentBySerializedName("Test");
    REQUIRE(bySerialized == byName);
}

TEST_CASE(
    "Reflection registry rejects duplicate component identity",
    "[core][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(registry.RegisterComponent(MakeTestComponent("First")));

    auto duplicateId = MakeTestComponent("Second");
    duplicateId.id = Janus::MakeComponentTypeId("First");

    const auto duplicate = registry.RegisterComponent(
        std::move(duplicateId));

    REQUIRE_FALSE(duplicate);
    REQUIRE(
        duplicate.GetError().code
        == Janus::ErrorCode::InvalidArgument);
}


TEST_CASE(
    "Reflection registry rejects duplicate component names and serialized names",
    "[core][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(registry.RegisterComponent(MakeTestComponent("First")));

    auto duplicateName = MakeTestComponent("First");
    duplicateName.id = Janus::MakeComponentTypeId("DifferentId");

    const auto nameResult =
        registry.RegisterComponent(std::move(duplicateName));
    REQUIRE_FALSE(nameResult);
    REQUIRE(
        nameResult.GetError().code
        == Janus::ErrorCode::InvalidArgument);

    auto duplicateSerialized = MakeTestComponent("Second");
    duplicateSerialized.serializedName = "First";

    const auto serializedResult =
        registry.RegisterComponent(std::move(duplicateSerialized));
    REQUIRE_FALSE(serializedResult);
    REQUIRE(
        serializedResult.GetError().code
        == Janus::ErrorCode::InvalidArgument);
}

TEST_CASE(
    "Reflection registry rejects duplicate property names",
    "[core][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    auto descriptor = MakeTestComponent();

    auto duplicate = MakeEnabledProperty();
    duplicate.id = Janus::MakePropertyId("Test.otherEnabled");
    duplicate.name = "speed";
    duplicate.serializedName = "otherEnabled";
    descriptor.properties.push_back(std::move(duplicate));

    const auto registered =
        registry.RegisterComponent(std::move(descriptor));

    REQUIRE_FALSE(registered);
    REQUIRE(
        registered.GetError().code
        == Janus::ErrorCode::InvalidArgument);
}

TEST_CASE(
    "Reflection registry rejects duplicate property metadata",
    "[core][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    auto descriptor = MakeTestComponent();

    auto duplicate = MakeEnabledProperty();
    duplicate.id = descriptor.properties.front().id;
    descriptor.properties.push_back(std::move(duplicate));

    const auto registered =
        registry.RegisterComponent(std::move(descriptor));

    REQUIRE_FALSE(registered);
    REQUIRE(
        registered.GetError().code
        == Janus::ErrorCode::InvalidArgument);
}

TEST_CASE(
    "Reflection component property enumeration is deterministic",
    "[core][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;

    REQUIRE(registry.RegisterComponent(MakeTestComponent("Zulu")));
    REQUIRE(registry.RegisterComponent(MakeTestComponent("Alpha")));

    const auto components = registry.GetComponents();
    REQUIRE(components.size() == 2);
    REQUIRE(components[0]->name == "Alpha");
    REQUIRE(components[1]->name == "Zulu");

    const auto* test = registry.FindComponent("Alpha");
    REQUIRE(test != nullptr);
    REQUIRE(test->properties.size() == 2);
    REQUIRE(test->properties[0].name == "enabled");
    REQUIRE(test->properties[1].name == "speed");
}

TEST_CASE(
    "Reflection property accessors get and set typed values",
    "[core][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(registry.RegisterComponent(MakeTestComponent()));

    const auto* descriptor = registry.FindComponent("Test");
    REQUIRE(descriptor != nullptr);

    const auto* speed = descriptor->FindProperty("speed");
    REQUIRE(speed != nullptr);
    REQUIRE(
        descriptor->FindProperty(
            Janus::MakePropertyId("Test.speed"))
        == speed);
    REQUIRE(
        descriptor->FindPropertyBySerializedName("speed")
        == speed);

    TestComponent component;

    auto original = speed->Get(&component);
    REQUIRE(original);
    REQUIRE(std::get<Janus::f32>(original.Value()) == 1.0f);

    auto updated = speed->Set(
        &component,
        Janus::PropertyValue{Janus::f32{3.5f}});
    REQUIRE(updated);
    REQUIRE(component.speed == 3.5f);
}

TEST_CASE(
    "Reflection property setter rejects mismatched PropertyValue types",
    "[core][reflection][v0.7]")
{
    const auto speed = MakeSpeedProperty();
    TestComponent component;

    const auto result = speed.Set(
        &component,
        Janus::PropertyValue{true});

    REQUIRE_FALSE(result);
    REQUIRE(
        result.GetError().code
        == Janus::ErrorCode::InvalidArgument);
    REQUIRE(component.speed == 1.0f);
}


TEST_CASE(
    "Reflection property getter rejects mismatched declared type",
    "[core][reflection][v0.7]")
{
    auto speed = MakeSpeedProperty();
    speed.type = Janus::PropertyType::Bool;

    const TestComponent component;
    const auto result = speed.Get(&component);

    REQUIRE_FALSE(result);
    REQUIRE(
        result.GetError().code
        == Janus::ErrorCode::InvalidState);
}

TEST_CASE(
    "Reflection read-only properties reject mutation",
    "[core][reflection][v0.7]")
{
    auto speed = MakeSpeedProperty();
    speed.editable = false;
    speed.setter = {};

    Janus::ReflectionRegistry registry;
    auto descriptor = MakeTestComponent();
    descriptor.properties = {speed};
    REQUIRE(registry.RegisterComponent(std::move(descriptor)));

    const auto* componentDescriptor =
        registry.FindComponent("Test");
    REQUIRE(componentDescriptor != nullptr);

    const auto* property =
        componentDescriptor->FindProperty("speed");
    REQUIRE(property != nullptr);

    TestComponent component;
    const auto result = property->Set(
        &component,
        Janus::PropertyValue{Janus::f32{2.0f}});

    REQUIRE_FALSE(result);
    REQUIRE(
        result.GetError().code
        == Janus::ErrorCode::InvalidState);
}
