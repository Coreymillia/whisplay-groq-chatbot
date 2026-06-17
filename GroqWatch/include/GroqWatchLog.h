#pragma once

#include <Arduino.h>

namespace GroqWatch {

#if defined(GROQWATCH_LOW_LOG_BUILD) && GROQWATCH_LOW_LOG_BUILD

inline void gwLogln() {}
template <typename T>
inline void gwLogln(const T &) {}

template <typename T>
inline void gwLog(const T &) {}

template <typename... Args>
inline void gwLogf(const char *, Args...) {}

#else

inline void gwLogln() { Serial.println(); }

template <typename T>
inline void gwLogln(const T &value) {
    Serial.println(value);
}

template <typename T>
inline void gwLog(const T &value) {
    Serial.print(value);
}

template <typename... Args>
inline void gwLogf(const char *fmt, Args... args) {
    Serial.printf(fmt, args...);
}

#endif

}  // namespace GroqWatch

#define GW_LOGLN(...) do { GroqWatch::gwLogln(__VA_ARGS__); } while (0)
#define GW_LOG(...)   do { GroqWatch::gwLog(__VA_ARGS__); } while (0)
#define GW_LOGF(...)  do { GroqWatch::gwLogf(__VA_ARGS__); } while (0)
