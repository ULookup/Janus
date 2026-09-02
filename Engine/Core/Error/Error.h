#pragma once

#include <source_location>
#include <string>
#include <utility>

namespace Janus
{

enum class ErrorCode
{
	None = 0,

	InvalidArgument,
	InvalidState,

	FileNotFound,
	FileReadFailed,
	FileWriteFailed,

	PlatformInitFailed,
	WindowCreateFailed,
	GraphicsContextCreateFailed,
	GraphicsContextMakeCurrentFailed,
	SwapIntervalFailed,

	Unknown
};

struct Error
{
	ErrorCode code{ ErrorCode::Unknown };
	std::string message;

	std::source_location source = std::source_location::current();

	Error() = default;

	Error(ErrorCode errorCode, std::string errorMessage, std::source_location errorSource = std::source_location::current())
		: code(errorCode),
		message(std::move(errorMessage)),
		source(errorSource)
	{ }
};

} // namespace Janus
