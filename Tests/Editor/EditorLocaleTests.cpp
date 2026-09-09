#include "EditorLocale.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace Janus::Editor;

TEST_CASE("Editor locale translates presentation without changing stable widget identity",
          "[editor][locale]")
{
    CHECK(std::string(EditorText(EditorLanguage::English, "Cancel")) == "Cancel");
    CHECK(std::string(EditorText(EditorLanguage::Chinese, "Cancel")) == "取消");
    CHECK(EditorLabel(EditorLanguage::English, "Cancel") == "Cancel###Cancel");
    CHECK(EditorLabel(EditorLanguage::Chinese, "Cancel") == "取消###Cancel");
    CHECK(std::string(EditorText(EditorLanguage::Chinese, "Custom asset name")) ==
          "Custom asset name");
}

TEST_CASE("Editor locale serialization uses canonical language tags", "[editor][locale]")
{
    CHECK(EditorLanguageCode(EditorLanguage::English) == "en-US");
    CHECK(EditorLanguageCode(EditorLanguage::Chinese) == "zh-CN");
    CHECK(ParseEditorLanguage("en-US") == EditorLanguage::English);
    CHECK(ParseEditorLanguage("zh-CN") == EditorLanguage::Chinese);
    CHECK_FALSE(ParseEditorLanguage("zh"));
    CHECK_FALSE(ParseEditorLanguage("Chinese"));
}

TEST_CASE("Editor Chinese catalog covers core workflows and keeps diagnostic formats",
          "[editor][locale]")
{
    for (const auto* key : {"File",
                            "Hierarchy",
                            "Inspector",
                            "Console",
                            "Project Settings",
                            "Save Scene",
                            "Play",
                            "Undo",
                            "Redo",
                            "Duplicate (Ctrl+D)",
                            "Export Prefab",
                            "Instantiate Prefab",
                            "Search assets...",
                            "All types",
                            "Assign to SpriteRenderer",
                            "Assign to AudioSource",
                            "No entity selected.",
                            "Locate asset",
                            "Search matching assets...",
                            "Project name",
                            "Asset root",
                            "Game resolution",
                            "Input actions",
                            "Save project settings",
                            "Auto-scroll",
                            "All levels",
                            "Rotation (rad)"})
    {
        INFO(key);
        CHECK(std::string(EditorText(EditorLanguage::Chinese, key)) != key);
        CHECK(std::string(EditorText(EditorLanguage::English, key)) == key);
    }
    CHECK(std::string(EditorText(EditorLanguage::Chinese, "Assets / %s")).find("%s") !=
          std::string::npos);
    const std::string custom = "My authored entity";
    CHECK(EditorText(EditorLanguage::Chinese, custom.c_str()) == custom.c_str());
}
