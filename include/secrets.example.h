#pragma once

constexpr const char* kWifiSsid = "your-ssid";
constexpr const char* kWifiPassword = "your-password";

constexpr const char* kMonitorHost = "192.168.0.100";

// Shared with the monitor (MONITOR_HMAC_KEY), 64 lowercase hex digits: openssl rand -hex 32
constexpr const char* kHmacKeyHex =
    "0000000000000000000000000000000000000000000000000000000000000000";
