#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Types.h"

#include <array>

namespace Janus
{

class Mat4
{
public:
    static Mat4 Identity();
    static Mat4 Orthographic(
        f32 left,
        f32 right,
        f32 bottom,
        f32 top,
        f32 nearPlane,
        f32 farPlane);
    static Mat4 Translate(Vector2 translation);
    static Mat4 Scale(Vector2 scale);
    static Mat4 Rotate(f32 radians);
    static Mat4 Multiply(const Mat4& left, const Mat4& right);
    static Vector2 TransformPoint(const Mat4& matrix, Vector2 point);

    [[nodiscard]] const f32* Data() const noexcept;

private:
    std::array<f32, 16> m_Value{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace Janus
