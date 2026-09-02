#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Types.h"

#include <string>
#include <vector>

namespace Janus
{

struct Color
{
    f32 r = 1.0f;
    f32 g = 1.0f;
    f32 b = 1.0f;
    f32 a = 1.0f;

    static constexpr Color White()
    {
        return Color{1.0f, 1.0f, 1.0f, 1.0f};
    }
};

struct Viewport
{
    u32 width = 0;
    u32 height = 0;
};

struct TextureRegion
{
    Vector2 min{0.0f, 0.0f};
    Vector2 max{1.0f, 1.0f};

    static constexpr TextureRegion Full()
    {
        return TextureRegion{};
    }
};

enum class BlendMode
{
    Alpha
};

// ----------------------------------------------------------
// Opaque GPU object handles
// ----------------------------------------------------------

struct TextureHandle
{
    u32 value = 0;
};

struct VertexBufferHandle
{
    u32 value = 0;
};

struct IndexBufferHandle
{
    u32 value = 0;
};

struct VertexArrayHandle
{
    u32 value = 0;
};

struct ShaderHandle
{
    u32 value = 0;
};

struct FramebufferHandle
{
    u32 value = 0;
};

// ----------------------------------------------------------
// Internal GPU resource descriptions
// ----------------------------------------------------------

struct BufferDesc
{
    const void* data = nullptr;
    usize size = 0;
};

enum class VertexAttributeType
{
    Float2,
    Float4
};

struct VertexAttribute
{
    u32 location = 0;
    VertexAttributeType type = VertexAttributeType::Float2;
    u32 offset = 0;
};

struct VertexLayout
{
    std::vector<VertexAttribute> attributes;
    u32 stride = 0;
};

struct ShaderDesc
{
    std::string vertexSource;
    std::string fragmentSource;
};

struct TextureDesc
{
    u32 width = 0;
    u32 height = 0;
    const void* data = nullptr;
    usize dataSize = 0;
};

struct FramebufferDesc
{
    TextureHandle colorTexture;
    u32 width = 0;
    u32 height = 0;
};

} // namespace Janus
