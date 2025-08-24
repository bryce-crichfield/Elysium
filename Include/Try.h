#pragma once

#include <string>
#include <memory>

namespace Elysium {
    template <typename T>
    class Try {
        bool hasError_;
        std::unique_ptr<T> value_;
        std::string errorMessage_;
    public:
        Try() : hasError_(false), value_(nullptr), errorMessage_("") { }

        Try(T&& value) : hasError_(false), value_(std::make_unique<T>(std::move(value))), errorMessage_("") { }

        Try(const std::string& error) : hasError_(true), value_(nullptr), errorMessage_(error) { }

        Try(const Try& other) = delete;
        Try& operator=(const Try& other) = delete;

        Try(Try&& other) noexcept = default;
        Try& operator=(Try&& other) noexcept = default;

        ~Try() = default;


    };


}
