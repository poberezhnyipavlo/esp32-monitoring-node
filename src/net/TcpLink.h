#pragma once

#include <IPAddress.h>
#include <WiFiClient.h>

#include <cstddef>
#include <cstdint>
#include <optional>

#include "ByteSink.h"
#include "RetryTimer.h"

class TcpLink : public ByteSink {
public:
    TcpLink(const char* host, uint16_t port, int32_t connect_timeout_ms, RetryTimer retry);

    void update(uint32_t now_ms);

    void close();

    [[nodiscard]] bool isOpen() const { return open_; }

    [[nodiscard]] bool send(const char* data, std::size_t len) override;

private:
    void resolve();
    void tryConnect();
    void drop(const char* reason);
    void disconnect();

    const char* host_;
    uint16_t port_;
    int32_t connect_timeout_ms_;
    RetryTimer retry_;
    WiFiClient client_;
    std::optional<IPAddress> ip_;
    bool open_ = false;
};
