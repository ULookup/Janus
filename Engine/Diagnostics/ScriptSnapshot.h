#pragma once

#include "Core/Types.h"
#include "Core/UUID/UUID.h"

#include <map>
#include <string>
#include <variant>

namespace Janus
{
using SnapshotScalar = std::variant<bool, f64, std::string>;

// An explicitly published diagnostic value, independent of Scene authoring and Lua tables.
struct ScriptSnapshot
{
    static constexpr usize MaxFields = 32;
    static constexpr usize MaxKeyBytes = 64;
    static constexpr usize MaxStringBytes = 256;
    static constexpr usize MaxBytes = 8192;

    UUID runtimeId;
    u64 frameIndex = 0;
    std::map<std::string, SnapshotScalar> fields;
};
} // namespace Janus
