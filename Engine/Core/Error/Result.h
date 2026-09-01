// Anthour : ULookup
// Time    : 2026/9/1
// File    : Result.h
// Function: Result<T> is a template class that represents the result of an operation that can either succeed with a value of type T or fail with an error.
//           It provides a way to handle success and failure cases in a type-safe manner, allowing for better error handling and code clarity.
#pragma once

#include "Core/Error/Error.h"
#include "Core/Assert.h"

#include <optional>
#include <type_traits>
#include <utility>

namespace Janus
{

template <typename T>
class [[nodiscard]] Result
{
	static_assert(!std::is_void_v<T>, "Using Result<void> specailization for void results.");

public:
	// ------------------------------------------
	// Factory
	// ------------------------------------------

	static Result Success(T value)
	{
		return Result(std::move(value));
	}

	static Result Failure(Error error)
	{
		return Result(std::move(error));
	}

	static Result Failure(ErrorCode code, std::string messgae, std::source_location source = std::source_location::current())
	{
		return Result(Error(code, std::move(messgae), source));
	}

	// ------------------------------------------
	// State
	// ------------------------------------------

	[[nodiscard]]
	bool HasValue() const noexcept
	{
		return m_Value.has_value();
	}

	[[nodiscard]]
	bool HasError() const noexcept
	{
		return !m_Value.has_value();
	}

	[[nodiscard]]
	explicit operator bool() const noexcept
	{
		return HasValue();
	}

	// ------------------------------------------
	// Value Access
	// ------------------------------------------

	/* Tips: T& Value() '&' means the member function can only be called on lvalues */

	T& Value()&
	{
		JANUS_CORE_ASSERT(HasValue(), "Attempted to access the value of a failed Result.");
		return *m_Value;
	}

	const T& Value() const &
	{
		JANUS_CORE_ASSERT(HasValue(), "Attempted to access the value of a failed Result.");
		return *m_Value;
	}

	/* Tips: T&& Value() '&&' means the member function can only be called on rvalues */

	T&& Value()&&
	{
		JANUS_CORE_ASSERT(HasValue(), "Attempted to access the value of a failed Result.");
		return std::move(*m_Value);
	}

	// ------------------------------------------
	// Error Access
	// ------------------------------------------

	Error& GetError() &
	{
		JANUS_CORE_ASSERT(HasError(), "Attempted to access the error of a successful Result.");
		return m_Error;
	}

	const Error& GetError() const &
	{
		JANUS_CORE_ASSERT(HasError(), "Attempted to access the error of a successful Result.");
		return m_Error;
	}

private:
	explicit Result(T value) : m_Value(std::move(value))
	{}

	explicit Result(Error error) : m_Value(std::nullopt), m_Error(std::move(error))
	{}

private:
	std::optional<T> m_Value;
	Error m_Error;
};

// ============================================================
// Result<void>
// ============================================================

template<>
class [[nodiscard]] Result<void>
{
public:
	// --------------------------------------------------------
	// Factory
	// --------------------------------------------------------

	static Result Success()
	{
		return Result();
	}

	static Result Failure(Error error)
	{
		return Result(std::move(error));
	}

	static Result Failure(
		ErrorCode code,
		std::string message,
		std::source_location source =
		std::source_location::current())
	{
		return Result(
			Error{
				code,
				std::move(message),
				source
			});
	}

	// --------------------------------------------------------
	// State
	// --------------------------------------------------------

	[[nodiscard]]
	bool HasValue() const noexcept
	{
		return !m_Error.has_value();
	}

	[[nodiscard]]
	bool HasError() const noexcept
	{
		return m_Error.has_value();
	}

	[[nodiscard]]
	explicit operator bool() const noexcept
	{
		return HasValue();
	}

	// --------------------------------------------------------
	// Error Access
	// --------------------------------------------------------

	Error& GetError()&
	{
		JANUS_CORE_ASSERT(HasError(), "Attempted to access the error of a successful Result.");
		return *m_Error;
	}

	const Error& GetError() const&
	{
		JANUS_CORE_ASSERT(HasError(), "Attempted to access the error of a successful Result.");
		return *m_Error;
	}

private:
	Result() = default;

	explicit Result(Error error)
		: m_Error(std::move(error))
	{
	}

private:
	std::optional<Error> m_Error;
};

} // namespace Janus
