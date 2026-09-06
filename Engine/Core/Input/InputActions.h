#pragma once
#include "Core/Error/Result.h"
#include "Core/Event/Event.h"
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Janus
{
class InputState;
using InputBindings = std::map<std::string, std::vector<KeyCode>, std::less<>>;
struct InputActionState
{
    bool down = false;
    bool pressed = false;
    bool released = false;
};
[[nodiscard]] std::string_view KeyCodeName(KeyCode key) noexcept;
[[nodiscard]] std::optional<KeyCode> ParseKeyCode(std::string_view name) noexcept;
[[nodiscard]] Result<void> ValidateInputBindings(const InputBindings& bindings);
[[nodiscard]] std::optional<InputActionState> QueryInputAction(const InputBindings& bindings,
                                                               const InputState& input,
                                                               std::string_view name) noexcept;
} // namespace Janus
