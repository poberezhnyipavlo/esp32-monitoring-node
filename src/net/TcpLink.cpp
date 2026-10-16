#include "TcpLink.h"

#include <WiFi.h>

TcpLink::TcpLink(const char* host, uint16_t port, int32_t connect_timeout_ms, RetryTimer retry)
    : host_(host), port_(port), connect_timeout_ms_(connect_timeout_ms), retry_(retry) {}

void TcpLink::resolve() {
    IPAddress ip;

    if (WiFi.hostByName(host_, ip) != 1) {
        Serial.printf("[net] Cannot resolve %s, will retry\n", host_);
        return;
    }

    ip_ = ip;
}

void TcpLink::close() {
    open_ = false;
    client_.stop();
}

void TcpLink::update(uint32_t now_ms) {
    if (!ip_) {
        resolve();
    }

    if (open_ && !client_.connected()) {
        drop("connection lost");
    }

    if (!open_ && ip_ && retry_.due(now_ms)) {
        tryConnect();
    }
}

void TcpLink::tryConnect() {
    Serial.printf(
        "[net] TCP: connecting to %s:%u (attempt %lu)...\n",
        ip_->toString().c_str(),
        static_cast<unsigned>(port_),
        static_cast<unsigned long>(retry_.failures() + 1)
    );
    const uint32_t started = millis();
    const bool ok = client_.connect(*ip_, port_, connect_timeout_ms_) == 1;
    const uint32_t took = millis() - started;
    if (!ok) {
        retry_.failed(millis());
        Serial.printf(
            "[net] TCP: connect failed after %lu ms, retry in %lu ms\n",
            static_cast<unsigned long>(took),
            static_cast<unsigned long>(retry_.delayMs())
        );
        return;
    }
    client_.setNoDelay(true);
    retry_.succeeded();
    open_ = true;
    Serial.printf("[net] TCP: connected in %lu ms\n", static_cast<unsigned long>(took));
}

void TcpLink::drop(const char* reason) {
    close();
    retry_.failed(millis());
    Serial.printf(
        "[net] TCP: %s, retry in %lu ms\n",
        reason,
        static_cast<unsigned long>(retry_.delayMs())
    );
}

bool TcpLink::send(const char* data, std::size_t len) {
    if (client_.write(data, len) != len) {
        drop("write failed");
        return false;
    }

    retry_.succeeded();

    return true;
}
