#pragma once

#include "Core/Types.h"

#include <string>

namespace Janus
{

    struct WindowConfig
    {
        std::string title = "Janus";

        u32 width = 1920;
        u32 height = 1080;

        bool resizable = true;
    };

} // namespace Janus