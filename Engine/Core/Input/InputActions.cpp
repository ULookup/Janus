#include "Core/Input/InputActions.h"
#include "Core/Input/InputState.h"
#include <array>
#include <set>

namespace Janus
{
namespace
{
constexpr std::array<std::string_view, static_cast<usize>(KeyCode::Count)> Names{
    "Escape", "Space",  "Enter",  "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight",
    "W",      "A",      "S",      "D",       "Digit0",    "Digit1",    "Digit2",
    "Digit3", "Digit4", "Digit5", "Digit6",  "Digit7",    "Digit8",    "Digit9"};
}
std::string_view KeyCodeName(KeyCode key) noexcept
{
    const auto index = static_cast<usize>(key);
    return index < Names.size() ? Names[index] : std::string_view{};
}
std::optional<KeyCode> ParseKeyCode(std::string_view name) noexcept
{
    for (usize i = 0; i < Names.size(); ++i)
        if (name == Names[i])
            return static_cast<KeyCode>(i);
    return std::nullopt;
}
Result<void> ValidateInputBindings(const InputBindings& bindings)
{
    if (bindings.size() > 64)
        return Result<void>::Failure(ErrorCode::InvalidArgument,
                                     "At most 64 input actions are allowed.");
    for (const auto& [name, keys] : bindings)
    {
        if (name.empty() || name.size() > 64 ||
            name.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-") !=
                std::string::npos ||
            keys.empty() || keys.size() > 8)
            return Result<void>::Failure(ErrorCode::InvalidArgument,
                                         "Invalid action name or binding count.");
        std::set<KeyCode> seen;
        for (auto key : keys)
            if (KeyCodeName(key).empty() || !seen.insert(key).second)
                return Result<void>::Failure(ErrorCode::InvalidArgument,
                                             "Invalid or duplicate action key.");
    }
    return Result<void>::Success();
}
std::optional<InputActionState> QueryInputAction(const InputBindings& bindings,
                                                 const InputState& input,
                                                 std::string_view name) noexcept
{
    const auto found = bindings.find(name);
    if (found == bindings.end())
        return std::nullopt;
    bool before = false, pressed = false, released = false;
    InputActionState result;
    for (auto key : found->second)
    {
        before |= input.WasKeyDownAtFrameStart(key);
        result.down |= input.IsKeyDown(key);
        pressed |= input.WasKeyPressed(key);
        released |= input.WasKeyReleased(key);
    }
    // Aggregate at frame boundaries: releasing one of two held keys is not an action release.
    result.pressed = !before && pressed;
    result.released = !result.down && released && (before || pressed);
    return result;
}
} // namespace Janus
