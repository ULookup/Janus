#pragma once

#include "Core/Error/Result.h"
#include "Core/Types.h"

#include <array>
#include <compare>
#include <string>
#include <string_view>

namespace Janus
{

class UUID
{
public:
    using Storage = std::array<u8, 16>;

    UUID() = default;
    explicit UUID(Storage bytes) noexcept;

    [[nodiscard]] static UUID Random();
    [[nodiscard]] static Result<UUID> Parse(std::string_view text);

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] std::string ToString() const;
    [[nodiscard]] const Storage& GetBytes() const noexcept;

    auto operator<=>(const UUID&) const = default;

private:
    Storage m_Bytes{};
};

struct UUIDHash
{
    [[nodiscard]] usize operator()(const UUID& uuid) const noexcept;
};

} // namespace Janus
