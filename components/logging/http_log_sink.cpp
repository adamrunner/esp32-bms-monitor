#include "http_log_sink.h"
#include <esp_http_client.h>
#include <map>

using namespace logging;

HTTPLogSink::HTTPLogSink() : 
    serializer_(nullptr),
    timeout_ms_(5000),
    initialized_(false),
    requests_sent_(0),
    bytes_sent_(0),
    errors_(0),
    last_success_ms_(0)
{
    setLastError("");
}

HTTPLogSink::~HTTPLogSink() {
    shutdown();
}

bool HTTPLogSink::init(const std::string& config) {
    if (!parseConfig(config)) {
        setLastError("Failed to parse configuration");
        return false;
    }

    // Create serializer
    delete serializer_;
    serializer_ = logging::BMSSerializer::createSerializer(config_.format);
    if (!serializer_) {
        setLastError("Failed to create serializer");
        return false;
    }

    initialized_ = true;
    return true;
}

bool HTTPLogSink::send(const output::BMSSnapshot& data) {
    if (!initialized_ || !isReady()) {
        return false;
    }

    std::string serialized;
    if (!serializer_->serialize(data, serialized)) {
        setLastError("Failed to serialize data");  
        errors_++;
        return false;
    }

    std::string content_type = serializer_->getContentType();
    return sendRequest(serialized, content_type);
}

void HTTPLogSink::shutdown() {
    delete serializer_;
    serializer_ = nullptr;
    initialized_ = false;
}

const char* HTTPLogSink::getName() const {
    return "http";
}

bool HTTPLogSink::isReady() const {
    return initialized_ && !url_.empty();
}

bool HTTPLogSink::sendRequest(const std::string& data, const std::string& content_type) {
    // For ESP-IDF, we would use esp_http_client or similar
    // This is a basic implementation outline
    
    #ifdef ESP_PLATFORM
        esp_http_client_config_t http_config = {};
        http_config.url = url_.c_str();
        http_config.timeout_ms = timeout_ms_;
        if (!method_.empty()) {
            http_config.method = (method_ == "POST") ? HTTP_METHOD_POST : HTTP_METHOD_PUT;
        }
        
        esp_http_client_handle_t client = esp_http_client_init(&http_config);
        if (!client) {
            setLastError("Failed to initialize HTTP client");
            errors_++;
            return false;
        }
        
        // Set headers
        for (const auto& header : headers_) {
            esp_http_client_set_header(client, header.first.c_str(), header.second.c_str());
        }
        
        esp_http_client_set_header(client, "Content-Type", content_type.c_str());
        if (!auth_token_.empty()) {
            esp_http_client_set_header(client, "Authorization", auth_token_.c_str());
        }
        
        esp_http_client_set_post_field(client, data.c_str(), data.length());
        
        esp_err_t err = esp_http_client_perform(client);
        esp_http_client_cleanup(client);
        
        if (err == ESP_OK) {
            requests_sent_++;
            bytes_sent_ += data.length();
            last_success_ms_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
            return true;
        } else {
            setLastError("HTTP request failed: " + std::string(esp_err_to_name(err)));
            errors_++;
            return false;
        }
    #else
        // Standalone/Linux implementation
        #error "HTTP client implementation needed for non-ESP platforms"
    #endif
}

bool HTTPLogSink::parseConfig(const std::string& config_str) {
    // Simple key=value parser
    // Format: "url=http://example.com,method=POST,format=json,timeout_ms=5000"
    std::string config = config_str;
    config += ","; // Add sentinel
    
    size_t start = 0;
    size_t pos = config.find('=');
    
    while (pos != std::string::npos) {
        size_t next_comma = config.find(',', pos);
        size_t prev_comma = config.rfind(',', pos-1);
        
        std::string key = config.substr(prev_comma+1, pos-prev_comma-1);
        std::string value = config.substr(pos+1, next_comma-pos-1);
        
        // Trim whitespace
        auto first_non_space = key.find_first_not_of(" \t\r\n");
        auto last_non_space = key.find_last_not_of(" \t\r\n");
        if (first_non_space != std::string::npos) {
            key = key.substr(first_non_space, last_non_space - first_non_space + 1);
        }
        
        first_non_space = value.find_first_not_of(" \t\r\n");
        last_non_space = value.find_last_not_of(" \t\r\n");
        if (first_non_space != std::string::npos) {
            value = value.substr(first_non_space, last_non_space - first_non_space + 1);
        }
        
        // Strip quotes if present
        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.length()-2);
        }
        
        if (key == "url") url_ = value;
        else if (key == "method") method_ = value;
        else if (key == "format") config_.format = value;
        else if (key == "timeout_ms") timeout_ms_ = atoi(value.c_str());
        else if (key == "auth_token") auth_token_ = value;
        
        start = next_comma + 1;
        pos = config.find('=', start);
        if (next_comma+1 >= config.length()) break;
    }
    
    return true;
}