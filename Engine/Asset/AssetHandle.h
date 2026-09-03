#pragma once

#include "Core/Error/Result.h"
#include "Core/UUID/UUID.h"

#include <compare>
#include <string>
#include <string_view>
#include <utility>

namespace Janus
{

struct AssetHandle
{
    UUID id;

    [[nodiscard]] static AssetHandle Random()
    {
        return AssetHandle{UUID::Random()};
    }

    [[nodiscard]] static Result<AssetHandle> Parse(std::string_view text)
    {
        auto uuid = UUID::Parse(text);
        if (!uuid)
        {
            return Result<AssetHandle>::Failure(uuid.GetError());
        }

        if (!uuid.Value().IsValid())
        {
            return Result<AssetHandle>::Failure(
                ErrorCode::InvalidArgument,
                "AssetHandle cannot use the nil UUID.");
        }

        return Result<AssetHandle>::Success(
            AssetHandle{std::move(uuid).Value()});
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return id.IsValid();
    }

    [[nodiscard]] std::string ToString() const
    {
        return id.ToString();
    }

    auto operator<=>(const AssetHandle&) const = default;
};

struct AssetHandleHash
{
    [[nodiscard]] usize operator()(const AssetHandle& handle) const noexcept
    {
        return UUIDHash{}(handle.id);
    }
};

} // namespace Janus
