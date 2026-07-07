// Diagnostics — device health, structured serial logging, and info/reboot helpers.
#pragma once
#include <Arduino.h>

namespace Diagnostics {
void   begin();                                          // log boot reason + boot count
void   logf(char level, const char *tag, const char *fmt, ...);  // structured serial log line
String humanUptime();                                    // "0d 01h 02m 03s"
String resetReason();                                    // why the chip last restarted
String infoJson();                                       // device health as JSON (served at /info)
void   reboot();                                         // graceful restart
}

// Structured log macros ->  [   12.345] I/tag: message
#define LOGI(tag, ...) Diagnostics::logf('I', tag, __VA_ARGS__)
#define LOGW(tag, ...) Diagnostics::logf('W', tag, __VA_ARGS__)
#define LOGE(tag, ...) Diagnostics::logf('E', tag, __VA_ARGS__)
