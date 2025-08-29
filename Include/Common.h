#pragma once

#ifdef TRACY_ENABLE
    #include <tracy/Tracy.hpp>
    
    // Convenient Tracy profiling macros
    #define Profile         ZoneScopedN(__PRETTY_FUNCTION__)
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