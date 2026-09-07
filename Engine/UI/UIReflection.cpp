#include "UI/UIReflection.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "UI/TextLayout.h"
#include "UI/UIComponents.h"
#include <cmath>
#include <type_traits>

namespace Janus
{
namespace
{
template <typename C, typename T>
PropertyDescriptor Field(const char* component, const char* name, T C::* member)
{
    PropertyDescriptor property;
    property.id = MakePropertyId(std::string(component) + "." + name);
    property.name = property.serializedName = name;
    property.type = GetPropertyType(PropertyValue{T{}});
    if constexpr (std::is_same_v<T, AssetReferenceValue>)
        property.referenceConstraint = std::is_same_v<C, TextComponent> ? "font" : "texture";
    property.getter = [member](const void* value)
    { return Result<PropertyValue>::Success(static_cast<const C*>(value)->*member); };
    property.setter = [member](void* object, const PropertyValue& value)
    {
        const auto& field = std::get<T>(value);
        if constexpr (std::is_same_v<T, Vector2>)
        {
            if (!std::isfinite(field.x) || !std::isfinite(field.y))
                return Result<void>::Failure(ErrorCode::InvalidArgument,
                                             "UI vector must be finite.");
        }
        if constexpr (std::is_same_v<T, ColorValue>)
        {
            for (f32 channel : {field.r, field.g, field.b, field.a})
                if (!std::isfinite(channel) || channel < 0 || channel > 1)
                    return Result<void>::Failure(ErrorCode::InvalidArgument,
                                                 "UI color channels must be in [0, 1].");
        }
        static_cast<C*>(object)->*member = field;
        return Result<void>::Success();
    };
    return property;
}
} // namespace

Result<void> RegisterUIReflection(ReflectionRegistry& registry)
{
    auto canvas = registry.RegisterComponent(
        ComponentDescriptor{MakeComponentTypeId("Canvas"),
                            "Canvas",
                            "Canvas",
                            true,
                            true,
                            {Field("Canvas", "enabled", &CanvasComponent::enabled)}});
    if (!canvas)
        return canvas;
    auto uirect = registry.RegisterComponent(ComponentDescriptor{
        MakeComponentTypeId("UIRect"),
        "UIRect",
        "UIRect",
        true,
        true,
        {Field("UIRect", "anchorMin", &UIRectComponent::anchorMin),
         Field("UIRect", "anchorMax", &UIRectComponent::anchorMax),
         Field("UIRect", "pivot", &UIRectComponent::pivot),
         Field("UIRect", "offset", &UIRectComponent::offset),
         Field("UIRect", "size", &UIRectComponent::size),
         Field("UIRect", "enabled", &UIRectComponent::enabled),
         Field("UIRect", "clipChildren", &UIRectComponent::clipChildren)},
        [](const void* value)
        { return ValidateUIRect(*static_cast<const UIRectComponent*>(value)); }});
    if (!uirect)
        return uirect;
    auto panel = registry.RegisterComponent(
        ComponentDescriptor{MakeComponentTypeId("Panel"),
                            "Panel",
                            "Panel",
                            true,
                            true,
                            {Field("Panel", "color", &PanelComponent::color),
                             Field("Panel", "enabled", &PanelComponent::enabled)}});
    if (!panel)
        return panel;
    auto image = registry.RegisterComponent(
        ComponentDescriptor{MakeComponentTypeId("Image"),
                            "Image",
                            "Image",
                            true,
                            true,
                            {Field("Image", "texture", &ImageComponent::texture),
                             Field("Image", "color", &ImageComponent::color),
                             Field("Image", "uvMin", &ImageComponent::uvMin),
                             Field("Image", "uvMax", &ImageComponent::uvMax),
                             Field("Image", "enabled", &ImageComponent::enabled)}});
    if (!image)
        return image;
    return registry.RegisterComponent(ComponentDescriptor{
        MakeComponentTypeId("Text"),
        "Text",
        "Text",
        true,
        true,
        {Field("Text", "font", &TextComponent::font),
         Field("Text", "content", &TextComponent::content),
         Field("Text", "fontSize", &TextComponent::fontSize),
         Field("Text", "color", &TextComponent::color),
         Field("Text", "alignment", &TextComponent::alignment),
         Field("Text", "enabled", &TextComponent::enabled)},
        [](const void* value) { return ValidateText(*static_cast<const TextComponent*>(value)); }});
}
} // namespace Janus
