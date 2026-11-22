// trade_event.hpp
#pragma once
#include <cstdint>

struct EarlyArrivalTradeEvent {
    double price;
    double qty;
    bool   is_buy;
    int64_t ts;
};
