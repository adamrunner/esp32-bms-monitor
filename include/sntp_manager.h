#ifndef SNTP_MANAGER_H
#define SNTP_MANAGER_H

#include <ctime>
#include <string>

namespace sntp {

/**
 * SNTP manager for time synchronization
 * Handles connecting to NTP servers and maintaining system time
 */
class SNTPManager {
public:
    SNTPManager() = default;
    ~SNTPManager() = default;

    /**
     * Initialize SNTP client
     * @param server NTP server to use (default: "pool.ntp.org")
     * @param timezone timezone string (default: "UTC")
     * @return true if initialization succeeded
     */
    bool init(const std::string& server = "pool.ntp.org", 
             const std::string& timezone = "UTC");

    /**
     * Check if time has been synchronized
     * @return true if time is valid and synchronized
     */
    bool isTimeSynced() const;

    /**
     * Get current time as Unix timestamp
     * @return current time in seconds since epoch
     */
    time_t getCurrentTime() const;

    /**
     * Get formatted time string
     * @param format strftime format string
     * @return formatted time string
     */
    std::string getFormattedTime(const char* format = "%Y-%m-%d %H:%M:%S") const;

    /**
     * Wait for time synchronization (blocking)
     * @param timeout_ms maximum time to wait in milliseconds
     * @return true if time was synchronized within timeout
     */
    bool waitForSync(int timeout_ms = 10000);

    /**
     * Shutdown SNTP client
     */
    void shutdown();

private:
    bool initialized_ = false;
    bool time_synced_ = false;
    std::string server_;
    std::string timezone_;
};

} // namespace sntp

#endif // SNTP_MANAGER_H