#pragma once

#include <cstdint>

class ByteSink {
public:
    ByteSink() = default;
    ByteSink(const ByteSink&) = delete;
    ByteSink& operator=(const ByteSink&) = delete;
    ByteSink(ByteSink&&) = delete;
    ByteSink& operator=(ByteSink&&) = delete;
    virtual ~ByteSink() = default;

    [[nodiscard]] virtual bool send(const char* data, std::size_t len) = 0;
};
