// Tips: This file contains platform and compiler detection macros, as well as build configuration settings for the Janus Engine.
#pragma once

// =========================================================
// Platform Detection
// =========================================================

#if defined(_WIN32)
#define JANUS_PLATFORM_WINDOWS 1
#elif defined(__linux__)
#define JANUS_PLATFORM_LINUX 1
#else
#error "Janus Engine currently supports only Windows and Linux."
#endif

// =========================================================
// Compiler Detection
// =========================================================

#if defined(__clang__)
#define JANUS_COMPILER_CLANG 1
#elif defined(_MSC_VER)
#define JANUS_COMPILER_MSVC 1
#elif defined(__GNUC__)
#define JANUS_COMPILER_GCC 1
#else
#error "Unsupported compiler."
#endif

// =========================================================
// Build Configuration
// =========================================================

#if defined(JANUS_DEBUG) && defined(JANUS_RELEASE)
#error "Cannot define both JANUS_DEBUG and JANUS_RELEASE."
#endif

#if !defined(JANUS_DEBUG) && !defined(JANUS_RELEASE)
#if defined(NDEBUG)
#define JANUS_RELEASE 1
#else
#define JANUS_DEBUG 1
#endif
#endif

// =========================================================
// Debug Break
// =========================================================

#if defined(JANUS_COMPILER_MSVC)
#include <intrin.h>
#define JANUS_DEBUGBREAK() __debugbreak()
#elif defined(JANUS_COMPILER_CLANG) || defined(JANUS_COMPILER_GCC)
#define JANUS_DEBUGBREAK() __builtin_trap()
#else
#define JANUS_DEBUGBREAK() ((void)0))
#endif