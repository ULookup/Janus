#pragma once

#include <imgui.h>

namespace Janus::Editor
{
// Call immediately after drawing the Game image; other widgets must not replace the last item.
inline bool AcceptGameViewInput(bool inspectorOwnsKeyboard)
{
    // IsItemHovered rejects other active widgets but permits this window's background MoveId.
    // IsAnyItemActive would drop a held Game click even on a fixed, non-movable window.
    return ImGui::IsItemHovered() && !ImGui::GetIO().WantTextInput && !inspectorOwnsKeyboard;
}
} // namespace Janus::Editor
