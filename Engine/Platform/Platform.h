#pragma once

#include "Core/Error/Result.h"

namespace Janus::Platform
{

	[[nodiscard]] Result<void> Initialize();

	void Shutdown() noexcept;

} // namespace Janus::Platf