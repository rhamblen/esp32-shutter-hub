#include "Diagnostics.h"
#include "AppConfig.h"
#include <WiFi.h>
#include <esp_system.h>
#include <stdarg.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

namespace {

const char *resetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int-wdt";
    case ESP_RST_TASK_WDT:  return "task-wdt";
    case ESP_RST_WDT:       return "other-wdt";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_EXT:       return "external";
    default:                return "unknown";
  }
}

// minimal JSON string encoder (quote + escape " and \)
String jstr(const String &s) {
  String o = "\"";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  o += "\"";
  return o;
}

}  // namespace

namespace Diagnostics {

void begin() {
  LOGI("diag", "boot #%u, reset reason: %s", (unsigned)AppConfig::bootCount(), resetReasonStr());
}

void logf(char level, const char *tag, const char *fmt, ...) {
  char msg[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  unsigned long ms = millis();
  Serial.printf("[%7lu.%03lu] %c/%s: %s\n", ms / 1000UL, ms % 1000UL, level, tag, msg);
}

String resetReason() { return resetReasonStr(); }

String humanUptime() {
  unsigned long s = millis() / 1000UL;
  unsigned long d = s / 86400UL; s %= 86400UL;
  unsigned long h = s / 3600UL;  s %= 3600UL;
  unsigned long m = s / 60UL;    s %= 60UL;
  char buf[48];
  snprintf(buf, sizeof(buf), "%lud %02luh %02lum %02lus", d, h, m, s);
  return String(buf);
}

String infoJson() {
  String j = "{";
  j += "\"firmware\":"     + jstr(FW_VERSION) + ",";
  j += "\"device\":"       + jstr(AppConfig::deviceName()) + ",";
  j += "\"uptime\":"       + jstr(humanUptime()) + ",";
  j += "\"uptime_s\":"     + String(millis() / 1000UL) + ",";
  j += "\"boot_count\":"   + String(AppConfig::bootCount()) + ",";
  j += "\"ap_enabled\":"   + String(AppConfig::apEnabled() ? "true" : "false") + ",";
  j += "\"last_flash\":{\"type\":" + jstr(AppConfig::lastFlashType()) +
       ",\"ok\":" + String(AppConfig::lastFlashOk() ? "true" : "false") +
       ",\"epoch\":" + String(AppConfig::lastFlashEpoch()) + "},";
  j += "\"reset_reason\":" + jstr(resetReasonStr()) + ",";
  j += "\"free_heap\":"    + String(ESP.getFreeHeap()) + ",";
  j += "\"min_free_heap\":"+ String(ESP.getMinFreeHeap()) + ",";
  j += "\"wifi\":{";
  j += "\"ssid\":" + jstr(WiFi.SSID()) + ",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  j += "\"ip\":"   + jstr(WiFi.localIP().toString()) + ",";
  j += "\"mac\":"  + jstr(WiFi.macAddress()) + ",";
  j += "\"host\":" + jstr(AppConfig::deviceName() + ".local");
  j += "}}";
  return j;
}

void reboot() {
  LOGW("diag", "reboot requested");
  delay(300);
  ESP.restart();
}

}  // namespace Diagnostics
