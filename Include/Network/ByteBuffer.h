#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Elysium::Network {

/**
 * ByteBuffer - Simple binary serialization for network packets.
 * Uses little-endian encoding for cross-platform compatibility.
 * Header-only implementation.
 */
class ByteBuffer {
   public:
    ByteBuffer() = default;

    explicit ByteBuffer(size_t initialCapacity) {
        buffer_.reserve(initialCapacity);
    }

    explicit ByteBuffer(const std::vector<uint8_t>& data) : buffer_(data), readPos_(0) {}

    ByteBuffer(const uint8_t* data, size_t length) : buffer_(data, data + length), readPos_(0) {}

    // === Write Operations ===

    void WriteU8(uint8_t value) {
        buffer_.push_back(value);
    }

    void WriteU16(uint16_t value) {
        buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    void WriteU32(uint32_t value) {
        buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    void WriteI32(int32_t value) {
        WriteU32(static_cast<uint32_t>(value));
    }

    void WriteFloat(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        WriteU32(bits);
    }

    void WriteBytes(const void* data, size_t length) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + length);
    }

    void WriteString(const std::string& str) {
        WriteU16(static_cast<uint16_t>(str.length()));
        WriteBytes(str.data(), str.length());
    }

    // === Read Operations ===

    uint8_t ReadU8() {
        EnsureReadable(1);
        return buffer_[readPos_++];
    }

    uint16_t ReadU16() {
        EnsureReadable(2);
        uint16_t value = static_cast<uint16_t>(buffer_[readPos_]) |
                         (static_cast<uint16_t>(buffer_[readPos_ + 1]) << 8);
        readPos_ += 2;
        return value;
    }

    uint32_t ReadU32() {
        EnsureReadable(4);
        uint32_t value = static_cast<uint32_t>(buffer_[readPos_]) |
                         (static_cast<uint32_t>(buffer_[readPos_ + 1]) << 8) |
                         (static_cast<uint32_t>(buffer_[readPos_ + 2]) << 16) |
                         (static_cast<uint32_t>(buffer_[readPos_ + 3]) << 24);
        readPos_ += 4;
        return value;
    }

    int32_t ReadI32() {
        return static_cast<int32_t>(ReadU32());
    }

    float ReadFloat() {
        uint32_t bits = ReadU32();
        float value;
        std::memcpy(&value, &bits, sizeof(float));
        return value;
    }

    void ReadBytes(void* dest, size_t length) {
        EnsureReadable(length);
        std::memcpy(dest, buffer_.data() + readPos_, length);
        readPos_ += length;
    }

    std::string ReadString() {
        uint16_t length = ReadU16();
        EnsureReadable(length);
        std::string str(reinterpret_cast<const char*>(buffer_.data() + readPos_), length);
        readPos_ += length;
        return str;
    }

    // === Buffer Access ===

    const uint8_t* Data() const { return buffer_.data(); }
    uint8_t* Data() { return buffer_.data(); }
    size_t Size() const { return buffer_.size(); }
    bool Empty() const { return buffer_.empty(); }

    void Clear() {
        buffer_.clear();
        readPos_ = 0;
    }

    void Reset() {
        readPos_ = 0;
    }

    size_t ReadPosition() const { return readPos_; }
    size_t Remaining() const { return buffer_.size() - readPos_; }
    bool HasRemaining() const { return readPos_ < buffer_.size(); }

    std::vector<uint8_t>& GetBuffer() { return buffer_; }
    const std::vector<uint8_t>& GetBuffer() const { return buffer_; }

    // Skip bytes without reading
    void Skip(size_t count) {
        EnsureReadable(count);
        readPos_ += count;
    }

    // Peek at data without advancing read position
    uint8_t PeekU8() const {
        if (readPos_ >= buffer_.size()) {
            throw std::out_of_range("ByteBuffer underflow on peek");
        }
        return buffer_[readPos_];
    }

   private:
    void EnsureReadable(size_t bytes) const {
        if (readPos_ + bytes > buffer_.size()) {
            throw std::out_of_range("ByteBuffer underflow: need " +
                                    std::to_string(bytes) + " bytes, have " +
                                    std::to_string(buffer_.size() - readPos_));
        }
    }

    std::vector<uint8_t> buffer_;
    size_t readPos_ = 0;
};

}  // namespace Elysium::Network
