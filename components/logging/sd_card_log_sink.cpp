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
    last_fsync_time_us_ = last_flush_time_;

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
        write_buffer_.size() >= config_.buffer_size) {
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
        // Ensure data is persisted before closing
        fsync(fileno(current_file_));
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

    // Create new file (manual rotation -> always start a new unique file)
    return createNewFile(OpenMode::AlwaysNewUnique);
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
    // Parse fsync interval
    cJSON *fsync_interval = cJSON_GetObjectItemCaseSensitive(json, "fsync_interval_ms");
    if (cJSON_IsNumber(fsync_interval)) {
        config_.fsync_interval_ms = static_cast<uint32_t>(fsync_interval->valueint);
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

    // Parse optional line counting on open
    cJSON *count_lines_flag = cJSON_GetObjectItemCaseSensitive(json, "count_lines_on_open");
    if (cJSON_IsBool(count_lines_flag)) {
        config_.count_lines_on_open = cJSON_IsTrue(count_lines_flag);
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
    bus_cfg.max_transfer_sz = 8192;
    bus_cfg.flags = 0;
    bus_cfg.data_io_default_level = 0;
    bus_cfg.isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO;
    bus_cfg.intr_flags = 0;

    // Configure GPIO drive capabilities for signal integrity
    gpio_set_drive_capability(static_cast<gpio_num_t>(config_.spi_mosi_pin), GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(static_cast<gpio_num_t>(config_.spi_clk_pin), GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(static_cast<gpio_num_t>(config_.spi_cs_pin), GPIO_DRIVE_CAP_3);

    // Configure MISO pin pull-up for signal integrity
    gpio_set_pull_mode(static_cast<gpio_num_t>(config_.spi_miso_pin), GPIO_PULLUP_ONLY);

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
        return createNewFile(OpenMode::AppendIfExists);
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
            // Ensure data is persisted before closing
            fsync(fileno(current_file_));
            fclose(current_file_);
            current_file_ = nullptr;
        }

        // Create new file based on rotation reason
        return createNewFile(reason == FileRotationReason::LINE_COUNT_LIMIT
                                 ? OpenMode::AlwaysNewUnique
                                 : OpenMode::AppendIfExists);
    }

    return true;
}

std::string SDCardLogSink::generateFilename() {
    // Legacy wrapper to maintain compatibility
    return generateUniqueFilenameForToday();
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

    // Throttled fsync to reduce blocking on SD cards
    {
        const uint64_t now_us = esp_timer_get_time();
        if (config_.fsync_interval_ms > 0 &&
            (now_us - last_fsync_time_us_) >= (uint64_t)config_.fsync_interval_ms * 1000ULL) {
            if (fsync(fileno(current_file_)) != 0) {
                int error_code = errno;
                ESP_LOGW(TAG, "fsync failed (errno: %d - %s)", error_code, strerror(error_code));
                // Don't fail on fsync error, just warn
            } else {
                last_fsync_time_us_ = now_us;
            }
        }
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

bool SDCardLogSink::createNewFile(OpenMode mode) {
    // Determine today's base filename and full path
    time_t now;
    time(&now);
    current_date_string_ = formatTimestamp(now);

    std::string filename;
    std::string full_path;

    bool append = false;
    bool is_new_file = true;

    if (mode == OpenMode::AppendIfExists) {
        // Try to append to the daily base file if it exists
        filename = buildDailyBaseFilename();
        full_path = config_.mount_point + "/" + filename;

        if (fileExists(full_path)) {
            append = true;
            if (!openFileForAppendOrWrite(full_path, /*append=*/true, is_new_file)) {
                return false;
            }
            ESP_LOGI(TAG, "Opened existing daily file for append: %s", full_path.c_str());
        } else {
            // Create a new base file for today
            append = false;
            if (!openFileForAppendOrWrite(full_path, /*append=*/false, is_new_file)) {
                return false;
            }
            ESP_LOGI(TAG, "Created new daily base file: %s", full_path.c_str());
        }
    } else { // AlwaysNewUnique
        filename = generateUniqueFilenameForToday();
        full_path = config_.mount_point + "/" + filename;
        append = false;
        if (!openFileForAppendOrWrite(full_path, /*append=*/false, is_new_file)) {
            return false;
        }
        ESP_LOGI(TAG, "Created new unique file: %s", full_path.c_str());
    }

    // Validate the filename
    if (!validateFilename(filename)) {
        ESP_LOGE(TAG, "Invalid filename generated: %s", filename.c_str());
        fclose(current_file_);
        current_file_ = nullptr;
        handleSDCardError("Invalid filename");
        return false;
    }

    // Write header only for new files or when appending to a zero-sized file
    if (serializer_->hasHeader() && is_new_file) {
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
            // Ensure header is persisted
            fflush(current_file_);
            header_written_ = true;
            ESP_LOGI(TAG, "CSV header written (%zu bytes)", header.size());
        }
    } else {
        // Header already present in existing file or serializer has no header
        header_written_ = true;
    }

    // Initialize stats
    stats_.current_filename = filename;

    // Determine initial bytes and optionally line count if appending to existing file
    long end_pos = ftell(current_file_);
    size_t initial_bytes = (end_pos > 0) ? static_cast<size_t>(end_pos) : 0;
    size_t initial_lines = 0;

    if (append && !is_new_file) {
        if (config_.count_lines_on_open) {
            size_t line_count = 0;
            size_t byte_count = 0;
            if (scanExistingFileStats(full_path, line_count, byte_count)) {
                initial_lines = line_count;
                initial_bytes = byte_count;
            }
        } else {
            // Only set bytes based on current file position; start counting new lines from 0
            initial_lines = 0;
        }
    } else {
        // New file path; start from zero
        initial_lines = 0;
    }

    stats_.current_file_lines = initial_lines;
    stats_.current_file_bytes = initial_bytes;

    // Increment "files created" only when opening a brand new file for writing
    if (!append) {
        stats_.total_files_created++;
    }

    ESP_LOGI(TAG, "File open complete: %s (lines=%zu, bytes=%zu, created=%s)",
             full_path.c_str(), stats_.current_file_lines, stats_.current_file_bytes, (!append ? "yes" : "no"));

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

bool SDCardLogSink::fileExists(const std::string& full_path) {
    struct stat st;
    return (stat(full_path.c_str(), &st) == 0) && S_ISREG(st.st_mode);
}

std::string SDCardLogSink::buildDailyBaseFilename() {
    time_t now;
    time(&now);
    return formatTimestamp(now) + config_.file_extension;
}

std::string SDCardLogSink::generateUniqueFilenameForToday() {
    std::string date_str;
    {
        time_t now;
        time(&now);
        date_str = formatTimestamp(now);
    }

    // Try base first
    std::string base = date_str + config_.file_extension;
    std::string full_path = config_.mount_point + "/" + base;
    if (!fileExists(full_path)) {
        return base;
    }

    // Then try numbered suffixes
    for (int sequence = 1; sequence <= 999; ++sequence) {
        std::ostringstream oss;
        oss << date_str << std::setfill('0') << std::setw(3) << sequence << config_.file_extension;
        std::string candidate = oss.str();
        std::string candidate_path = config_.mount_point + "/" + candidate;
        if (!fileExists(candidate_path)) {
            return candidate;
        }
    }

    ESP_LOGW(TAG, "Too many files for date %s, using last fallback name", date_str.c_str());
    return date_str + "999" + config_.file_extension;
}

bool SDCardLogSink::openFileForAppendOrWrite(const std::string& full_path, bool append, bool& is_new_file) {
    const char* mode = append ? "a" : "w";
    current_file_ = fopen(full_path.c_str(), mode);
    if (!current_file_) {
        int error_code = errno;
        ESP_LOGE(TAG, "Failed to open file '%s' with mode '%s' (errno: %d - %s)",
                 full_path.c_str(), mode, error_code, strerror(error_code));
        handleSDCardError("Failed to open file: " + full_path);
        return false;
    }

    // Position at end and determine if file is empty
    if (fseek(current_file_, 0, SEEK_END) != 0) {
        // If fseek fails, treat as new file
        is_new_file = true;
        return true;
    }
    long pos = ftell(current_file_);
    if (pos < 0) pos = 0;
    is_new_file = (pos == 0);
    return true;
}

bool SDCardLogSink::scanExistingFileStats(const std::string& full_path, size_t& line_count, size_t& byte_count) {
    FILE* f = fopen(full_path.c_str(), "r");
    if (!f) {
        int error_code = errno;
        ESP_LOGW(TAG, "Failed to open file for scanning '%s' (errno: %d - %s)", full_path.c_str(), error_code, strerror(error_code));
        return false;
    }

    static constexpr size_t BUF_SZ = 4096;
    char buf[BUF_SZ];
    size_t lines = 0;
    size_t bytes_total = 0;

    while (true) {
        size_t n = fread(buf, 1, BUF_SZ, f);
        if (n == 0) {
            if (ferror(f)) {
                ESP_LOGW(TAG, "Error while reading file during scan: %s", full_path.c_str());
                fclose(f);
                return false;
            }
            break; // EOF
        }
        bytes_total += n;
        for (size_t i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                lines++;
            }
        }
    }

    // Use ftell to confirm byte count if possible
    if (fseek(f, 0, SEEK_END) == 0) {
        long end_pos = ftell(f);
        if (end_pos >= 0) {
            byte_count = static_cast<size_t>(end_pos);
        } else {
            byte_count = bytes_total;
        }
    } else {
        byte_count = bytes_total;
    }

    line_count = lines;
    fclose(f);
    return true;
}

} // namespace logging
