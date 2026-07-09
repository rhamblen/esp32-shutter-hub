#include "HomeKit.h"
#include "Diagnostics.h"

namespace HomeKit {
void begin() { LOGI("homekit", "stub — Phase 5 (HomeSpan bridge) not built yet"); }
bool running()       { return false; }
bool paired()        { return false; }
int  controllers()   { return 0; }
bool resetPairings() { return false; }
}
