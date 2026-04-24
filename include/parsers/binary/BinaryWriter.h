#ifndef BINARY_PARSER_H
#define BINARY_PARSER_H

#include <Arduino.h>
#include <string>
#include <cstring>
#include <limits>

/// @brief This class will write the binary representation of each type into a buffer of choice. It will not allocate any memory nor own any. 
/// The user is responsible for providing a buffer with enough space to write the data, and for managing the memory of the buffer. The class will keep track of the current offset in the buffer, and will write the data sequentially. The class will also provide a method to get the current offset in the buffer,
/// which can be used to know how much data has been written to the buffer. The class will also provide a method to get a pointer to the buffer, which can be used to read the data from the buffer after writing.
class BinaryWriter
{
private:
    uint8_t* buffer;
    size_t buffer_size;
    size_t offset;

    bool write_bytes(const uint8_t* data, size_t length);
public:
    BinaryWriter(uint8_t* buffer, size_t buffer_size);
    ~BinaryWriter();

    inline bool write_u8(uint8_t value);
    bool write_u16(uint16_t value);
    bool write_u32(uint32_t value);
    bool write_u64(uint64_t v);

    bool write_i16(int16_t value);
    bool write_i32(int32_t value);

    bool write_float(float value);
    bool write_double(double value);


    /// @brief Only strings smaller than 65536 bytes can be written with this method, as the length of the string is represented by a 16-bit unsigned integer.
    /// If the string is too long, the method will return false and not write anything to the buffer.
    /// The method will also return false if the buffer does not have enough space to write the string and its length prefix. The method will return true if the string was successfully written to the buffer.
    /// @param value 
    /// @return 
    bool write_string(const std::string& value);
    bool write_string(const char* value, uint16_t length);
    /// @brief Writes a string into a binary buffer with the first two bytes representing the length -> only strings of length up to 65535 can be written with this method. 
    /// If the length is greater than 65535, the method will return false and not write anything to the buffer. The method will also return false if the buffer does not have enough space to write the string and its length prefix. 
    /// The method will return true if the string was successfully written to the buffer.
    /// @param value 
    /// @param length 
    /// @return 
    bool write_string(const char* value, size_t length);

    bool finish(size_t& written_length); // this will pad the buffer with zeros until it reaches the next 16-bit boundary.

    uint16_t* get_words() { return reinterpret_cast<uint16_t*>(buffer); }
    size_t get_word_count() { return offset / 2 + 1; }

    uint8_t* get_buffer() { return buffer; }
    size_t get_buffer_size() { return buffer_size; }
    size_t get_offset() { return offset; }
};

inline bool BinaryWriter::write_bytes(const uint8_t * data, size_t length)
{
    if(offset + length > buffer_size)
    {
        // not enough space in the buffer to write the data
        return false;
    }
    std::memcpy(buffer + offset, data, length);
    offset += length;
    return true;
}

BinaryWriter::BinaryWriter(uint8_t* buffer, size_t buffer_size) : buffer(buffer), buffer_size(buffer_size), offset(0) 
{
}

BinaryWriter::~BinaryWriter()
{
}

inline bool BinaryWriter::write_u8(uint8_t value)
{
    return write_bytes(&value, sizeof(value));
}

inline bool BinaryWriter::write_u16(uint16_t value)
{
    uint8_t b[2] = {
        static_cast<uint8_t>(value & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF)
    };
    return write_bytes(b, 2);
}

inline bool BinaryWriter::write_u32(uint32_t value)
{
    uint8_t b[4] = {
        static_cast<uint8_t>(value & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >> 24) & 0xFF)
    };
    return write_bytes(b, 4);
}

inline bool BinaryWriter::write_u64(uint64_t v)
{
    uint8_t b[8] = {
        static_cast<uint8_t>(v),
        static_cast<uint8_t>(v >> 8),
        static_cast<uint8_t>(v >> 16),
        static_cast<uint8_t>(v >> 24),
        static_cast<uint8_t>(v >> 32),
        static_cast<uint8_t>(v >> 40),
        static_cast<uint8_t>(v >> 48),
        static_cast<uint8_t>(v >> 56)
    };

    return write_bytes(b, 8);
}

// Teensy 4.1/GCC uses two's-complement representation.
// Values above INT16_MAX decode as negative int32_t values.
inline bool BinaryWriter::write_i16(int16_t value)
{
    return write_u16(static_cast<uint16_t>(value));
}

// Teensy 4.1/GCC uses two's-complement representation.
// Values above INT32_MAX decode as negative int32_t values.
inline bool BinaryWriter::write_i32(int32_t value)
{
    return write_u32(static_cast<uint32_t>(value));
}

inline bool BinaryWriter::write_float(float value)
{
    // write float into four bytes:
    static_assert(sizeof(float) == 4, "float must be 32-bit");
    uint32_t raw;
    std::memcpy(&raw, &value, sizeof(value));
    return write_u32(raw);
}

inline bool BinaryWriter::write_double(double value)
{
    // write double into eight bytes:
    static_assert(sizeof(double) == 8, "double must be 64-bit");
    uint64_t raw;
    std::memcpy(&raw, &value, sizeof(value));
    return write_u64(raw);
}

inline bool BinaryWriter::write_string(const std::string& value)
{
    if(value.size() > std::numeric_limits<uint16_t>::max())
    {
        // string is too long to be represented by a 16-bit length prefix
        return false;
    }

    return write_string(value.c_str(), static_cast<uint16_t>(value.size()));
}

inline bool BinaryWriter::write_string(const char *value, uint16_t length)
{
    if(value == nullptr && length != 0)
    {
        // invalid input, null pointer with non-zero length
        return false;
    }

    // check that there's enough space for both the length prefix and the string data
    if(offset + sizeof(uint16_t) + length > buffer_size)
    {
        // not enough space in the buffer to write the string and its length prefix
        return false;
    }

    // write the length of the string as a 32-bit unsigned integer, followed by the bytes of the string:
    if(!write_u16(static_cast<uint16_t>(length)))
    {
        return false;
    }
    if(!write_bytes(reinterpret_cast<const uint8_t*>(value), length))
    {
        return false;
    }
    return true;
}

inline bool BinaryWriter::write_string(const char *value, size_t length)
{
    if(length > std::numeric_limits<uint16_t>::max())
    {
        // string is too long to be represented by a 16-bit length prefix
        return false;
    }
    return write_string(value, static_cast<uint16_t>(length));
}

bool BinaryWriter::finish(size_t &written_length)
{
    // pad the buffer with zeros until it reaches the next 16-bit boundary
    if(offset % 2 != 0)
    {
        write_u8(0x00);
    }
    
    written_length = offset;
    return true;
}

#endif // BINARY_PARSER_H