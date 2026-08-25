#ifndef MLB_LOGGING_H
#define MLB_LOGGING_H

#ifdef ARDUINO
#include <Arduino.h>
#endif

// Debug/info/error logging macros -- output goes to Serial (Arduino IDE
// Serial Monitor). Both MLBDataSource (network layer) and MLBScoreboard
// (orchestration layer) use these, so they live in their own header
// rather than in either class's header -- MLBDataSource previously
// #included "MLBScoreboard.h" just to reach these macros, an upside-down
// dependency (the lower-level network class depending on the top-level
// orchestrator) that this file removes.
//
// Info and error messages are always emitted; debug messages only when
// gMLBDebugEnabled is true (toggle via MLBScoreboard::setDebugLogging()).
extern bool gMLBDebugEnabled;

#define MLB_DEBUG(fmt, ...)                                                                                          \
    do                                                                                                               \
    {                                                                                                                \
        if (gMLBDebugEnabled)                                                                                        \
        {                                                                                                            \
            Serial.printf("[MLB DEBUG] " fmt "\n", ##__VA_ARGS__);                                                   \
        }                                                                                                            \
    } while (0)

#define MLB_INFO(fmt, ...)                                                                                           \
    do                                                                                                               \
    {                                                                                                                \
        Serial.printf("[MLB INFO] " fmt "\n", ##__VA_ARGS__);                                                       \
    } while (0)

#define MLB_ERROR(fmt, ...)                                                                                          \
    do                                                                                                               \
    {                                                                                                                \
        Serial.printf("[MLB ERROR] " fmt "\n", ##__VA_ARGS__);                                                      \
    } while (0)

#endif // MLB_LOGGING_H
