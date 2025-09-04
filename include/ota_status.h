#ifndef OTA_STATUS_H
#define OTA_STATUS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA status snapshot structure for logging
 */
typedef struct {
    uint64_t timestamp_us;        // Real timestamp from SNTP
    uint32_t uptime_sec;          // System uptime in seconds
    int status;                   // OTA status (from ota_status_t enum)
    int progress_pct;             // Progress percentage (0-100)
    char message[128];            // Status message
    char current_version[32];     // Current firmware version
    char available_version[32];   // Available firmware version (if known)
    bool rollback_pending;        // True if rollback is pending
    uint32_t free_heap;           // Free heap memory during OTA
} ota_status_snapshot_t;

#ifdef __cplusplus
}
#endif

#endif // OTA_STATUS_H