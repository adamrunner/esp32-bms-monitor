#ifndef LOG_SERIALIZERS_H
#define LOG_SERIALIZERS_H

#include <string>
#include "output.h"

namespace logging {

// Forward declaration
class CSVSerializer;

enum class SerializationFormat {
    JSON,
    CSV,
    XML,
    BINARY,
    HUMAN,
    KEY_VALUE
};

const char* formatToString(SerializationFormat format);
SerializationFormat stringToFormat(const std::string& format_str);

/**
 * Interface for serializing BMS data into different formats
 */
class BMSSerializer {
public:
    virtual ~BMSSerializer() = default;

    /**
     * Serialize the BMS snapshot data
     * @param data BMS data to serialize
     * @param result output string buffer
     * @return true if serialization succeeded
     */
    virtual bool serialize(const output::BMSSnapshot& data, std::string& result) = 0;

    /**
     * Get the serialization format type
     * @return format type
     */
    virtual SerializationFormat getFormat() const = 0;

    /**
     * Set serialization options
     * @param options JSON-style options string
     * @return true if options parsed successfully
     */
    virtual bool setOptions(const std::string& options) { return true; }

    /**
     * Get supported content type (for HTTP sinks, etc.)
     * @return MIME content type
     */
    virtual std::string getContentType() const = 0;

    /**
     * Check if batching/multiple records are supported
     * @return true if batching supported
     */
    virtual bool supportsBatching() const { return false; }

    /**
     * Start a batch of records
     * @return true if started successfully
     */
    virtual bool beginBatch() { return supportsBatching(); }

    /**
     * End a batch of records
     * @param result output string buffer with batch data
     * @return true if completed successfully
     */
    virtual bool endBatch(std::string& result) { return supportsBatching(); }

    /**
     * Create serializer instance for specified format
     * @param format serialization format
     * @return serializer instance (nullptr if unsupported)
     */
    static BMSSerializer* createSerializer(SerializationFormat format);
    static BMSSerializer* createSerializer(const std::string& format_str);
};

} // namespace logging

#endif // LOG_SERIALIZERS_H