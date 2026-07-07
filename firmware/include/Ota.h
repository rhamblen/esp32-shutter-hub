// Ota — browser firmware update (ElegantOTA at /update), wired to an AsyncWebServer.
#pragma once
#include <ESPAsyncWebServer.h>

namespace Ota {
void begin(AsyncWebServer &server);  // mount /update + progress logging
void loop();                         // must be pumped from the main loop
}
