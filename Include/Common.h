#pragma once

#ifdef TRACY_ENABLE
    #include <tracy/Tracy.hpp>
    
    #if defined(_MSC_VER)
        #define FUNCTION_SIGNATURE __FUNCSIG__
    #elif defined(__GNUC__) || defined(__clang__)
        #define FUNCTION_SIGNATURE __PRETTY_FUNCTION__
    #else
        #define FUNCTION_SIGNATURE __func__  // C++11 standard fallback
    #endif

    // Convenient Tracy profiling macros
    #define Profile         ZoneScopedN(FUNCTION_SIGNATURE)
    #define ProfileN(name)  ZoneScopedN(name)
    #define ProfileC(color) ZoneScopedC(color)
    #define ProfileNC(name, color) ZoneScopedNC(name, color)
    
    // Additional useful Tracy macros
    #define ProfileText(text) ZoneText(text, strlen(text))
    #define ProfileValue(name, value) ZoneValue(name, value)
    #define ProfileName(name) ZoneName(name, strlen(name))
#else
    // No-op macros when Tracy is disabled
    #define Profile         
    #define ProfileN(name)  
    #define ProfileC(color) 
    #define ProfileNC(name, color) 
    #define ProfileText(text)
    #define ProfileValue(name, value)
    #define ProfileName(name)
#endif