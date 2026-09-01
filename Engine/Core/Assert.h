#pragma once

#include "Core/Base.h"
#include "Core/Log/Log.h"

#if defined(JANUS_DEBUG)

#define JANUS_CORE_ASSERT(condition, message)                 \
        do                                                    \
        {                                                     \
            if (!(condition))                                 \
            {                                                 \
                JANUS_CORE_CRITICAL(                          \
                    "Assertion failed: '{}' at {}:{}",        \
                    #condition,                               \
                    __FILE__,                                 \
                    __LINE__);                                \
                                                              \
                JANUS_CORE_CRITICAL("{}", message);           \
                                                              \
                JANUS_DEBUGBREAK();                           \
            }                                                 \
        } while (false)

#define JANUS_ASSERT(condition, message)                      \
        do                                                    \
        {                                                     \
            if (!(condition))                                 \
            {                                                 \
                JANUS_CRITICAL(                               \
                    "Assertion failed: '{}' at {}:{}",        \
                    #condition,                               \
                    __FILE__,                                 \
                    __LINE__);                                \
                                                              \
                JANUS_CRITICAL("{}", message);                \
                                                              \
                JANUS_DEBUGBREAK();                           \
            }                                                 \
        } while (false)

#else

#define JANUS_CORE_ASSERT(condition, message) ((void)0)
#define JANUS_ASSERT(condition, message)      ((void)0)

#endif