#ifndef SD_CARD_LOG_SINK_H
#define SD_CARD_LOG_SINK_H

#include "log_sink.h"
#include "log_serializers.h"
#include <memory>
#include <string>
#include <mutex>
#include <cstdio>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

namespace logging {

/**
 * Configuration structure for SD Card logging
 */
struct SDCardConfig {
    std::string mount_point = "/sdcard";
    std::string file_prefix = "bms";
    std::string file_extension = ".csv";
    size_t buffer_size = 10240;  // 10KB default
    uint32_t flush_interval_ms = 30000;  // 30 seconds
    uint32_t max_lines_per_file = 10000;  // Fallback rotation
    bool enable_free_space_check = true;
    size_t min_free_space_mb = 10;  // Minimum free space before stopping

    // SPI Configuration (ESP32-C6 defaults)
    int spi_mosi_pin = 23;
    int spi_miso_pin = 19;
    int spi_clk_pin = 18;
    int spi_cs_pin = 22;
    int spi_host = SPI2_HOST;
    int spi_freq_khz = 20000;  // 20MHz
};

/**
 * SD Card state enumeration
 */
enum class SDCardState {
    UNINITIALIZED,
    INITIALIZING,
    READY,
    ERROR_NO_CARD,
    ERROR_MOUNT_FAILED,
    ERROR_DISK_FULL,
    ERROR_IO_FAILURE
};

/**
 * File rotation reason enumeration
 */
enum class FileRotationReason {
    DAILY_ROTATION,
    LINE_COUNT_LIMIT,
    FILE_SIZE_LIMIT,
    MANUAL_ROTATION
};

/**
 * File statistics structure
 */
struct FileStats {
    std::string current_filename;
    size_t current_file_lines = 0;
    size_t current_file_bytes = 0;
    size_t total_files_created = 0;
    size_t total_bytes_written = 0;
    uint64_t last_write_time_us = 0;
    uint64_t last_flush_time_us = 0;
};

/**
 * SD Card log sink that outputs to SD card in CSV format
 * Provides persistent local storage with file rotation and buffering
 */
class SDCardLogSink : public LogSink {
public:
    SDCardLogSink();
    ~SDCardLogSink() override;

    // LogSink interface implementation
    bool init(const std::string& config) override;
    bool send(const output::BMSSnapshot& data) override;
    void shutdown() override;
    const char* getName() const override;
    bool isReady() const override;

    // SD card specific methods
    SDCardState getState() const { return state_; }
    const FileStats& getFileStats() const { return stats_; }
    bool rotateFile();
    bool flushBuffer();

private:
    // Configuration and state
    SDCardConfig config_;
    SDCardState state_;
    FileStats stats_;

    // Serialization and buffering
    std::unique_ptr<BMSSerializer> serializer_;
    std::string write_buffer_;
    mutable std::mutex buffer_mutex_;

    // File management
    FILE* current_file_;
    std::string current_date_string_;
    bool header_written_;

    // Timing
    uint64_t last_flush_time_;

    // SD card hardware
    sdmmc_card_t* card_;
    esp_vfs_fat_sdmmc_mount_config_t mount_config_;

    // Private methods
    bool parseConfig(const std::string& config_str);
    bool initSDCard();
    bool rotateFileIfNeeded();
    std::string generateFilename();
    bool writeBufferToFile();
    bool checkFreeSpace();
    void updateFileStats();
    bool createNewFile();
    void handleSDCardError(const std::string& error);

    // Helper methods
    bool isSDCardPresent();
    std::string formatTimestamp(time_t timestamp);
    size_t getAvailableSpace();
    bool validateFilename(const std::string& filename);
};

} // namespace logging

#endif // SD_CARD_LOG_SINK_H
