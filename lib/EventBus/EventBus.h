#pragma once

#include <array>
#include <cstddef>

#include "ClimateMeasured.h"
#include "Listener.h"

template <std::size_t N>
class EventBus {
public:
    explicit EventBus(const std::array<Listener*, N>& listeners) : listeners_(listeners) {}

    void publish(const ClimateMeasured& event) const {
        for (Listener* listener : listeners_) {
            listener->onClimateMeasured(event);
        }
    }

private:
    std::array<Listener*, N> listeners_;
};
