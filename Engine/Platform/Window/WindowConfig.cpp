#include "Platform/Window/WindowConfig.h"

#include <limits>

namespace Janus
{

    Result<void> ValidateWindowConfig(const WindowConfig& config)
    {
        if (config.width == 0 || config.height == 0)
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "Window width and height must be greater than zero.");
        }

        constexpr auto maxWindowDimension =
            static_cast<u32>(std::numeric_limits<i32>::max());

        if (config.width > maxWindowDimension ||
            config.height > maxWindowDimension)
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "Window dimensions exceed SDL supported integer range.");
        }

        return Result<void>::Success();
    }

} // namespace Janus
