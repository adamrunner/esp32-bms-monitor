#include "time_manager.h"
#include <ezTime.h>

namespace time_manager {

static bool time_initialized = false;
static bool time_synchronized = false;
static Timezone myTZ;

bool initialize() {
    // Set up time synchronization
    setDebug(INFO); // Set debug level for ezTime
    time_initialized = true;
    return true;
}

bool syncTime() {
    if (!time_initialized) {
        return false;
    }
    
    // Try to sync time with NTP server
    // updateNTP() returns void, so we need to check if time is set differently
    updateNTP();
    
    // Check if time is now synchronized
    if (myTZ.now() > 1000000000UL) { // Check if we have a reasonable timestamp
        time_synchronized = true;
        Serial.println("[TIME] Time synchronized successfully");
        Serial.printf("[TIME] Current time: %s\n", myTZ.dateTime().c_str());
        return true;
    } else {
        Serial.println("[TIME] Failed to synchronize time");
        time_synchronized = false;
        return false;
    }
}

bool isTimeSynchronized() {
    return time_synchronized;
}

unsigned long getUnixTimestamp() {
    if (!time_synchronized) {
        return 0;
    }
    return myTZ.now();
}

String getFormattedTime() {
    if (!time_synchronized) {
        return "TIME_NOT_SYNCED";
    }
    return myTZ.dateTime();
}

String getISOTimeString() {
    if (!time_synchronized) {
        return "1970-01-01T00:00:00Z";
    }
    return myTZ.dateTime("Y-m-d_H:i:s");
}

} // namespace time_manager