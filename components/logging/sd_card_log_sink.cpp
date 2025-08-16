#include "sd_card_log_sink.h"
#include "log_serializers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include <cJSON.h>
#include <sys/stat.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cerrno>
#include <cstring>

static const char* TAG = "SDCardLogSink";

namespace logging {

SDCardLogSink::SDCardLogSink()
    : state_(SDCardState::UNINITIALIZED)
    , current_file_(nullptr)
    , header_written_(false)
    , last_flush_time_(0)
    , card_(nullptr)
{
    ESP_LOGI(TAG, "SDCardLogSink created");
}

SDCardLogSink::~SDCardLogSink() {
    shutdown();
}

bool SDCardLogSink::init(const std::string& config) {
    ESP_LOGI(TAG, "Initializing SD Card Log Sink");

    // Parse configuration
    if (!parseConfig(config)) {
        setLastError("Failed to parse configuration");
        return false;
    }

    // Initialize SD card
    if (!initSDCard()) {
        setLastError("Failed to initialize SD card");
        return false;
    }

    // Create CSV serializer
    serializer_ = BMSSerializer::createSerializer(SerializationFormat::CSV);
    if (!serializer_) {
        setLastError("Failed to create CSV serializer");
        return false;
    }

    // Initialize buffer and timing
    write_buffer_.reserve(config_.buffer_size);
    last_flush_time_ = esp_timer_get_time();

    state_ = SDCardState::READY;
    ESP_LOGI(TAG, "SD Card Log Sink initialized successfully");
    return true;
}

bool SDCardLogSink::send(const output::BMSSnapshot& data) {
    if (state_ != SDCardState::READY) {
        return false;
    }

    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // Check free space periodically (every 100 writes to avoid overhead)
    if (stats_.current_file_lines % 100 == 0) {
        if (!checkFreeSpace()) {
            return false;
        }
    }

    // Check if we need to rotate the file
    if (!rotateFileIfNeeded()) {
        return false;
    }

    // Serialize data to CSV
    std::string csv_line;
    if (!serializer_->serialize(data, csv_line)) {
        setLastError("Failed to serialize data");
        return false;
    }

    // Add to buffer
    write_buffer_ += csv_line + "\n";
    stats_.current_file_lines++;
    stats_.last_write_time_us = esp_timer_get_time();

    // Check if we need to flush - be more aggressive about flushing
    uint64_t now = esp_timer_get_time();
    if ((now - last_flush_time_) >= (config_.flush_interval_ms * 1000) ||
        write_buffer_.size() >= config_.buffer_size ||
        stats_.current_file_lines % 10 == 0) {  // Flush every 10 lines
        return writeBufferToFile();
    }

    return true;
}

void SDCardLogSink::shutdown() {
    ESP_LOGI(TAG, "Shutting down SD Card Log Sink");

    // Flush any remaining data
    flushBuffer();

    // Close current file
    if (current_file_) {
        fclose(current_file_);
        current_file_ = nullptr;
    }

    // Unmount SD card
    if (card_) {
        esp_vfs_fat_sdcard_unmount(config_.mount_point.c_str(), card_);
        card_ = nullptr;
    }

    state_ = SDCardState::UNINITIALIZED;
    ESP_LOGI(TAG, "SD Card Log Sink shutdown complete");
}

const char* SDCardLogSink::getName() const {
    return "sdcard";
}

bool SDCardLogSink::isReady() const {
    return state_ == SDCardState::READY;
}

bool SDCardLogSink::rotateFile() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // Flush current buffer
    if (!writeBufferToFile()) {
        return false;
    }

    // Close current file
    if (current_file_) {
        fclose(current_file_);
        current_file_ = nullptr;
    }

    // Create new file
    return createNewFile();
}

bool SDCardLogSink::flushBuffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return writeBufferToFile();
}

// Private method implementations
bool SDCardLogSink::parseConfig(const std::string& config_str) {
    if (config_str.empty() || config_str == "{}") {
        ESP_LOGI(TAG, "Using default SD card configuration");
        return true;
    }

    // Parse JSON configuration
    cJSON *json = cJSON_Parse(config_str.c_str());
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse SD card config JSON: %s", config_str.c_str());
        return false;
    }

    // Parse mount point
    cJSON *mount_point = cJSON_GetObjectItemCaseSensitive(json, "mount_point");
    if (cJSON_IsString(mount_point)) {
        config_.mount_point = std::string(mount_point->valuestring);
    }

    // Parse file prefix
    cJSON *file_prefix = cJSON_GetObjectItemCaseSensitive(json, "file_prefix");
    if (cJSON_IsString(file_prefix)) {
        config_.file_prefix = std::string(file_prefix->valuestring);
    }

    // Parse file extension
    cJSON *file_extension = cJSON_GetObjectItemCaseSensitive(json, "file_extension");
    if (cJSON_IsString(file_extension)) {
        config_.file_extension = std::string(file_extension->valuestring);
    }

    // Parse buffer size
    cJSON *buffer_size = cJSON_GetObjectItemCaseSensitive(json, "buffer_size");
    if (cJSON_IsNumber(buffer_size)) {
        config_.buffer_size = static_cast<size_t>(buffer_size->valueint);
    }

    // Parse flush interval
    cJSON *flush_interval = cJSON_GetObjectItemCaseSensitive(json, "flush_interval_ms");
    if (cJSON_IsNumber(flush_interval)) {
        config_.flush_interval_ms = static_cast<uint32_t>(flush_interval->valueint);
    }

    // Parse max lines per file
    cJSON *max_lines = cJSON_GetObjectItemCaseSensitive(json, "max_lines_per_file");
    if (cJSON_IsNumber(max_lines)) {
        config_.max_lines_per_file = static_cast<uint32_t>(max_lines->valueint);
    }

    // Parse free space checking
    cJSON *enable_free_space = cJSON_GetObjectItemCaseSensitive(json, "enable_free_space_check");
    if (cJSON_IsBool(enable_free_space)) {
        config_.enable_free_space_check = cJSON_IsTrue(enable_free_space);
    }

    // Parse minimum free space
    cJSON *min_free_space = cJSON_GetObjectItemCaseSensitive(json, "min_free_space_mb");
    if (cJSON_IsNumber(min_free_space)) {
        config_.min_free_space_mb = static_cast<size_t>(min_free_space->valueint);
    }

    // Parse SPI configuration
    cJSON *spi_config = cJSON_GetObjectItemCaseSensitive(json, "spi");
    if (cJSON_IsObject(spi_config)) {
        cJSON *mosi_pin = cJSON_GetObjectItemCaseSensitive(spi_config, "mosi_pin");
        if (cJSON_IsNumber(mosi_pin)) {
            config_.spi_mosi_pin = mosi_pin->valueint;
        }

        cJSON *miso_pin = cJSON_GetObjectItemCaseSensitive(spi_config, "miso_pin");
        if (cJSON_IsNumber(miso_pin)) {
            config_.spi_miso_pin = miso_pin->valueint;
        }

        cJSON *clk_pin = cJSON_GetObjectItemCaseSensitive(spi_config, "clk_pin");
        if (cJSON_IsNumber(clk_pin)) {
            config_.spi_clk_pin = clk_pin->valueint;
        }

        cJSON *cs_pin = cJSON_GetObjectItemCaseSensitive(spi_config, "cs_pin");
        if (cJSON_IsNumber(cs_pin)) {
            config_.spi_cs_pin = cs_pin->valueint;
        }

        cJSON *freq_khz = cJSON_GetObjectItemCaseSensitive(spi_config, "freq_khz");
        if (cJSON_IsNumber(freq_khz)) {
            config_.spi_freq_khz = freq_khz->valueint;
        }
    }

    cJSON_Delete(json);

    ESP_LOGI(TAG, "SD card configuration parsed successfully");
    ESP_LOGI(TAG, "Mount point: %s", config_.mount_point.c_str());
    ESP_LOGI(TAG, "File prefix: %s", config_.file_prefix.c_str());
    ESP_LOGI(TAG, "Buffer size: %zu bytes", config_.buffer_size);
    ESP_LOGI(TAG, "Flush interval: %u ms", config_.flush_interval_ms);

    return true;
}

bool SDCardLogSink::initSDCard() {
    ESP_LOGI(TAG, "Initializing SD card with SPI interface");

    state_ = SDCardState::INITIALIZING;

    // Configure SPI bus
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = static_cast<gpio_num_t>(config_.spi_mosi_pin);
    bus_cfg.miso_io_num = static_cast<gpio_num_t>(config_.spi_miso_pin);
    bus_cfg.sclk_io_num = static_cast<gpio_num_t>(config_.spi_clk_pin);
    bus_cfg.quadwp_io_num = GPIO_NUM_NC;
    bus_cfg.quadhd_io_num = GPIO_NUM_NC;
    bus_cfg.data4_io_num = GPIO_NUM_NC;
    bus_cfg.data5_io_num = GPIO_NUM_NC;
    bus_cfg.data6_io_num = GPIO_NUM_NC;
    bus_cfg.data7_io_num = GPIO_NUM_NC;
    bus_cfg.max_transfer_sz = 4000;
    bus_cfg.flags = 0;
    bus_cfg.data_io_default_level = 0;
    bus_cfg.isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO;
    bus_cfg.intr_flags = 0;

    esp_err_t ret = spi_bus_initialize((spi_host_device_t)config_.spi_host, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        handleSDCardError("Failed to initialize SPI bus: " + std::string(esp_err_to_name(ret)));
        return false;
    }

    // Configure SD card host
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = config_.spi_host;
    host.max_freq_khz = config_.spi_freq_khz;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = static_cast<gpio_num_t>(config_.spi_cs_pin);
    slot_config.host_id = (spi_host_device_t)config_.spi_host;

    // Configure mount options
    mount_config_ = {};
    mount_config_.format_if_mount_failed = false;
    mount_config_.max_files = 5;
    mount_config_.allocation_unit_size = 16 * 1024;
    mount_config_.disk_status_check_enable = true;
    mount_config_.use_one_fat = false;

    // Mount the SD card
    ret = esp_vfs_fat_sdspi_mount(config_.mount_point.c_str(), &host, &slot_config, &mount_config_, &card_);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            handleSDCardError("Failed to mount filesystem. SD card may not be formatted.");
            state_ = SDCardState::ERROR_MOUNT_FAILED;
        } else {
            handleSDCardError("Failed to initialize SD card: " + std::string(esp_err_to_name(ret)));
            state_ = SDCardState::ERROR_NO_CARD;
        }
        return false;
    }

    // Print card info
    ESP_LOGI(TAG, "SD card mounted successfully");
    ESP_LOGI(TAG, "Card name: %s", card_->cid.name);
    ESP_LOGI(TAG, "Card type: %s", (card_->ocr & (1UL << 30)) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI(TAG, "Card speed: %s", (card_->csd.tr_speed > 25000000) ? "high speed" : "default speed");
    ESP_LOGI(TAG, "Card size: %lluMB", ((uint64_t) card_->csd.capacity) * card_->csd.sector_size / (1024 * 1024));

    return true;
}

bool SDCardLogSink::rotateFileIfNeeded() {
    // If no file is open, create one
    if (!current_file_) {
        return createNewFile();
    }

    bool needs_rotation = false;
    FileRotationReason reason = FileRotationReason::DAILY_ROTATION;

    // Check for daily rotation
    time_t now;
    time(&now);
    std::string current_date = formatTimestamp(now);

    if (current_date != current_date_string_) {
        needs_rotation = true;
        reason = FileRotationReason::DAILY_ROTATION;
        ESP_LOGI(TAG, "Daily rotation needed: %s -> %s", current_date_string_.c_str(), current_date.c_str());
    }

    // Check for line count limit
    if (!needs_rotation && stats_.current_file_lines >= config_.max_lines_per_file) {
        needs_rotation = true;
        reason = FileRotationReason::LINE_COUNT_LIMIT;
        ESP_LOGI(TAG, "Line count rotation needed: %zu lines", stats_.current_file_lines);
    }

    // Perform rotation if needed
    if (needs_rotation) {
        ESP_LOGI(TAG, "Rotating file due to %s",
                 (reason == FileRotationReason::DAILY_ROTATION) ? "daily rotation" : "line count limit");

        // Flush current buffer first
        if (!writeBufferToFile()) {
            return false;
        }

        // Close current file
        if (current_file_) {
            fclose(current_file_);
            current_file_ = nullptr;
        }

        // Create new file
        return createNewFile();
    }

    return true;
}

std::string SDCardLogSink::generateFilename() {
    time_t now;
    time(&now);

    std::string date_str = formatTimestamp(now);

    // Use FAT-compatible filename format (numbers-only timestamp works best)
    std::string filename = date_str + config_.file_extension;

    // If file already exists, add a sequence number
    // std::string full_path = config_.mount_point + "/" + filename;
    std::string full_path = filename;
    int sequence = 1;

    while (access(full_path.c_str(), F_OK) == 0) {
        std::ostringstream oss;
        // Use numbers-only format for sequence (avoid underscore)
        oss << date_str
            << std::setfill('0') << std::setw(3) << sequence
            << config_.file_extension;
        filename = oss.str();
        full_path = config_.mount_point + "/" + filename;
        sequence++;

        // Prevent infinite loop
        if (sequence > 999) {
            ESP_LOGW(TAG, "Too many files for date %s, using sequence 999", date_str.c_str());
            break;
        }
    }

    if (!validateFilename(filename)) {
        ESP_LOGW(TAG, "Generated filename is invalid: %s", filename.c_str());
        // Fallback to simple naming
        filename = "data" + config_.file_extension;
    }

    ESP_LOGI(TAG, "Generated filename: %s", filename.c_str());
    return filename;
}

bool SDCardLogSink::writeBufferToFile() {
    if (write_buffer_.empty()) {
        return true;
    }

    if (!current_file_) {
        setLastError("No file open for writing");
        return false;
    }

    // Check if SD card is still present before writing
    if (!isSDCardPresent()) {
        ESP_LOGW(TAG, "SD card no longer present, cannot write buffer");
        state_ = SDCardState::ERROR_NO_CARD;
        return false;
    }

    // ESP_LOGW(TAG, "FLUSHING: Writing %zu bytes to SD card", write_buffer_.size());

    // Write buffer contents to file
    size_t buffer_size = write_buffer_.size();
    size_t written = fwrite(write_buffer_.c_str(), 1, buffer_size, current_file_);

    if (written != buffer_size) {
        int error_code = errno;
        ESP_LOGE(TAG, "Write failed: wrote %zu of %zu bytes (errno: %d - %s)",
                 written, buffer_size, error_code, strerror(error_code));
        handleSDCardError("Failed to write buffer to file. Wrote " +
                         std::to_string(written) + " of " + std::to_string(buffer_size) + " bytes");
        return false;
    }

    // Flush to ensure data is written to SD card
    if (fflush(current_file_) != 0) {
        int error_code = errno;
        ESP_LOGE(TAG, "Flush failed (errno: %d - %s)", error_code, strerror(error_code));
        handleSDCardError("Failed to flush file buffer");
        return false;
    }

    // Force filesystem sync to ensure data reaches SD card
    if (fsync(fileno(current_file_)) != 0) {
        int error_code = errno;
        ESP_LOGW(TAG, "fsync failed (errno: %d - %s)", error_code, strerror(error_code));
        // Don't fail on fsync error, just warn
    }

    // Update statistics
    stats_.total_bytes_written += written;
    stats_.last_flush_time_us = esp_timer_get_time();
    
    // Update file stats using ftell() for accurate byte count
    updateFileStats();

    // Clear the buffer
    write_buffer_.clear();
    last_flush_time_ = esp_timer_get_time();

    ESP_LOGI(TAG, "Successfully wrote %zu bytes to file, total file size: %zu bytes",
             written, stats_.current_file_bytes);

    return true;
}

bool SDCardLogSink::checkFreeSpace() {
    if (!config_.enable_free_space_check) {
        return true;
    }

    size_t available_bytes = getAvailableSpace();
    size_t available_mb = available_bytes / (1024 * 1024);

    if (available_mb < config_.min_free_space_mb) {
        std::string error_msg = "Insufficient free space: " + std::to_string(available_mb) +
                               "MB available, " + std::to_string(config_.min_free_space_mb) + "MB required";
        handleSDCardError(error_msg);
        state_ = SDCardState::ERROR_DISK_FULL;
        return false;
    }

    ESP_LOGD(TAG, "Free space check passed: %zu MB available", available_mb);
    return true;
}

void SDCardLogSink::updateFileStats() {
    if (!current_file_) {
        return;
    }

    // Get current file position to determine file size
    long current_pos = ftell(current_file_);
    if (current_pos >= 0) {
        stats_.current_file_bytes = static_cast<size_t>(current_pos);
    }

    ESP_LOGD(TAG, "File stats updated - Lines: %zu, Bytes: %zu, Total files: %zu",
             stats_.current_file_lines, stats_.current_file_bytes, stats_.total_files_created);
}

bool SDCardLogSink::createNewFile() {
    // Generate new filename
    std::string filename = generateFilename();
    std::string full_path = config_.mount_point + "/" + filename;

    ESP_LOGI(TAG, "Creating new file: %s", full_path.c_str());
    ESP_LOGI(TAG, "Filename: '%s', Full path: '%s'", filename.c_str(), full_path.c_str());

    // Debug: List the contents of the mount point to see what's there
    ESP_LOGI(TAG, "Checking mount point contents...");
    struct stat st;
    if (stat(config_.mount_point.c_str(), &st) == 0) {
        ESP_LOGI(TAG, "Mount point exists and is accessible");
        if (S_ISDIR(st.st_mode)) {
            ESP_LOGI(TAG, "Mount point is a directory");
        } else {
            ESP_LOGI(TAG, "Mount point is not a directory");
        }
    } else {
        ESP_LOGW(TAG, "Mount point not accessible: %s", config_.mount_point.c_str());
    }

    // Validate the filename before attempting to create the file
    if (!validateFilename(filename)) {
        ESP_LOGE(TAG, "Invalid filename generated: %s", filename.c_str());
        // Use a simple fallback filename
        filename = "bms_data.csv";
        full_path = config_.mount_point + "/" + filename;
        ESP_LOGI(TAG, "Using fallback filename: %s", filename.c_str());
    }

    // Ensure the path doesn't have double slashes
    size_t double_slash_pos = full_path.find("//");
    while (double_slash_pos != std::string::npos) {
        full_path.replace(double_slash_pos, 2, "/");
        double_slash_pos = full_path.find("//");
    }

    ESP_LOGI(TAG, "Final path after cleanup: %s", full_path.c_str());

    // Open file for writing
    current_file_ = fopen(full_path.c_str(), "w");
    if (!current_file_) {
        // Try to get more detailed error information
        int error_code = errno;
        ESP_LOGW(TAG, "Failed to create file: %s (errno: %d - %s)", full_path.c_str(), error_code, strerror(error_code));

        // Try creating a simple test file to see if it's a path issue
        ESP_LOGW(TAG, "Attempting to create test file in root of mount point");
        std::string test_path = config_.mount_point + "/test.txt";
        FILE* test_file = fopen(test_path.c_str(), "w");
        if (test_file) {
            ESP_LOGI(TAG, "Test file creation successful");
            fclose(test_file);
            // Try to remove the test file
            remove(test_path.c_str());

            // Try different filename variations to find what works
            // Based on testing, FAT filesystem has issues with mixed letters/numbers
            time_t current_time = time(nullptr);
            std::string timestamp = formatTimestamp(current_time);

            std::vector<std::string> filename_attempts = {
                // Try numbers-only timestamp (this works)
                timestamp + ".csv",
                // Try with simple prefix
                "data" + timestamp + ".csv",
                // Try all letters prefix
                "bmsdata.csv",
                // Last resort
                "data.csv"
            };

            for (const auto& attempt_filename : filename_attempts) {
                std::string attempt_path = config_.mount_point + "/" + attempt_filename;
                ESP_LOGI(TAG, "Trying filename: %s", attempt_filename.c_str());

                current_file_ = fopen(attempt_path.c_str(), "w");
                if (current_file_) {
                    ESP_LOGI(TAG, "Filename worked: %s", attempt_filename.c_str());
                    filename = attempt_filename;
                    full_path = attempt_path;
                    break;
                } else {
                    ESP_LOGW(TAG, "Filename failed: %s (errno: %d - %s)", attempt_filename.c_str(), errno, strerror(errno));
                }
            }
        } else {
            ESP_LOGE(TAG, "Test file creation also failed (errno: %d - %s)", errno, strerror(errno));
        }

        if (!current_file_) {
            std::string error_msg = "Failed to create any file variation in: " + config_.mount_point;
            handleSDCardError(error_msg);
            return false;
        }
    }

    // Update current date string for rotation tracking
    time_t now;
    time(&now);
    current_date_string_ = formatTimestamp(now);

    // Reset file statistics
    stats_.current_filename = filename;
    stats_.current_file_lines = 0;
    stats_.current_file_bytes = 0;
    stats_.total_files_created++;

    // Write header if the serializer supports it
    if (serializer_->hasHeader()) {
        std::string header = serializer_->getHeader();
        if (!header.empty()) {
            size_t written = fwrite(header.c_str(), 1, header.size(), current_file_);
            if (written != header.size()) {
                int error_code = errno;
                ESP_LOGE(TAG, "Failed to write header: wrote %zu of %zu bytes (errno: %d - %s)",
                         written, header.size(), error_code, strerror(error_code));
                fclose(current_file_);
                current_file_ = nullptr;
                handleSDCardError("Failed to write CSV header to file");
                return false;
            }
            ESP_LOGI(TAG, "CSV header written successfully (%zu bytes)", header.size());
        }
    }
    header_written_ = true;

    // Flush to ensure header is written
    fflush(current_file_);

    ESP_LOGI(TAG, "New file created successfully: %s", filename.c_str());
    return true;
}

void SDCardLogSink::handleSDCardError(const std::string& error) {
    ESP_LOGE(TAG, "SD Card Error: %s", error.c_str());
    setLastError(error);
    state_ = SDCardState::ERROR_IO_FAILURE;
}

// Helper method implementations
bool SDCardLogSink::isSDCardPresent() {
    // Check if card is mounted and accessible
    if (!card_) {
        return false;
    }

    // Try to access the mount point
    struct stat st;
    return (stat(config_.mount_point.c_str(), &st) == 0);
}

std::string SDCardLogSink::formatTimestamp(time_t timestamp) {
    // Get current time if timestamp is 0
    if (timestamp == 0) {
        time(&timestamp);
    }

    // If still 0 or invalid, use system uptime as fallback
    if (timestamp <= 0) {
        uint64_t uptime_us = esp_timer_get_time();
        uint32_t uptime_sec = uptime_us / 1000000;
        return "uptime_" + std::to_string(uptime_sec);
    }

    struct tm timeinfo;
    if (localtime_r(&timestamp, &timeinfo) == nullptr) {
        // If localtime_r fails, use uptime fallback
        uint64_t uptime_us = esp_timer_get_time();
        uint32_t uptime_sec = uptime_us / 1000000;
        return "uptime_" + std::to_string(uptime_sec);
    }

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (timeinfo.tm_year + 1900)
        << std::setw(2) << (timeinfo.tm_mon + 1)
        << std::setw(2) << timeinfo.tm_mday;

    return oss.str();
}

size_t SDCardLogSink::getAvailableSpace() {
    if (!card_) {
        return 0;
    }

    // Use ESP-IDF's high-level function to get actual filesystem information
    uint64_t total_bytes, free_bytes;
    esp_err_t result = esp_vfs_fat_info(config_.mount_point.c_str(), &total_bytes, &free_bytes);
    
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get filesystem info: %s", esp_err_to_name(result));
        
        // Fallback to card capacity estimation if VFS info fails
        uint64_t total_capacity = ((uint64_t) card_->csd.capacity) * card_->csd.sector_size;
        ESP_LOGD(TAG, "Using fallback capacity estimation: %llu bytes", total_capacity);
        return static_cast<size_t>(total_capacity * 0.9); // Assume 90% available as fallback
    }
    
    ESP_LOGD(TAG, "Filesystem info - Total: %llu bytes, Free: %llu bytes", total_bytes, free_bytes);
    return static_cast<size_t>(free_bytes);
}

bool SDCardLogSink::validateFilename(const std::string& filename) {
    if (filename.empty()) {
        return false;
    }

    // Check for invalid characters
    const std::string invalid_chars = "<>:\"/\\|?*";
    for (char c : filename) {
        if (invalid_chars.find(c) != std::string::npos) {
            return false;
        }
    }

    // Check length (FAT32 limit is 255 characters)
    if (filename.length() > 255) {
        return false;
    }

    return true;
}

} // namespace logging
