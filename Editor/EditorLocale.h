#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Janus::Editor
{
enum class EditorLanguage
{
    English,
    Chinese
};

// The English key must outlive the returned pointer; use static literals for UI labels.
[[nodiscard]] const char* EditorText(EditorLanguage language, const char* english);
[[nodiscard]] std::string EditorLabel(EditorLanguage language, const char* english);
[[nodiscard]] std::string_view EditorLanguageCode(EditorLanguage language) noexcept;
[[nodiscard]] std::optional<EditorLanguage> ParseEditorLanguage(std::string_view code) noexcept;
} // namespace Janus::Editor
