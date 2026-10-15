#pragma once

#include "ClimateMeasured.h"

class Listener {
public:
    Listener() = default;
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(Listener&&) = delete;
    virtual ~Listener() = default;

    virtual void onClimateMeasured(const ClimateMeasured& event) = 0;
};
