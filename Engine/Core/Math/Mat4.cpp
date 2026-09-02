#include "Core/Math/Mat4.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>

#include <glm/mat4x4.hpp>

namespace Janus
{

namespace
{

[[nodiscard]] glm::mat4 ToGlm(const Mat4& matrix)
{
    return glm::make_mat4(matrix.Data());
}

Mat4 FromGlm(const glm::mat4& matrix)
{
    Mat4 result;
    const f32* data = glm::value_ptr(matrix);

    for (usize index = 0; index < 16; ++index)
    {
        const_cast<f32*>(result.Data())[index] = data[index];
    }

    return result;
}

} // namespace

Mat4 Mat4::Identity()
{
    return Mat4();
}

Mat4 Mat4::Orthographic(
    f32 left,
    f32 right,
    f32 bottom,
    f32 top,
    f32 nearPlane,
    f32 farPlane)
{
    return FromGlm(
        glm::ortho(left, right, bottom, top, nearPlane, farPlane));
}

Mat4 Mat4::Translate(Vector2 translation)
{
    return FromGlm(glm::translate(
        glm::mat4{1.0f},
        glm::vec3{translation.x, translation.y, 0.0f}));
}

Mat4 Mat4::Scale(Vector2 scale)
{
    return FromGlm(glm::scale(
        glm::mat4{1.0f},
        glm::vec3{scale.x, scale.y, 1.0f}));
}

Mat4 Mat4::Rotate(f32 radians)
{
    return FromGlm(glm::rotate(
        glm::mat4{1.0f},
        radians,
        glm::vec3{0.0f, 0.0f, 1.0f}));
}

Mat4 Mat4::Multiply(const Mat4& left, const Mat4& right)
{
    return FromGlm(ToGlm(left) * ToGlm(right));
}

Vector2 Mat4::TransformPoint(const Mat4& matrix, Vector2 point)
{
    const auto transformed =
        ToGlm(matrix) * glm::vec4{point.x, point.y, 0.0f, 1.0f};
    return Vector2{transformed.x, transformed.y};
}

const f32* Mat4::Data() const noexcept
{
    return m_Value.data();
}

} // namespace Janus
