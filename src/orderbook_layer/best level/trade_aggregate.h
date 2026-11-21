#pragma once 
#include <cstdint>

struct TradeEvent {
    double price;
    double qty;
    bool is_buy;   // true = buy hits ask, false = sell hits bid
    int64_t ts;
};

struct TradeAgg {
    double qty = 0.0;
    double hit_strength = 0.0;
    int64_t first_hit_ts = 0;
    int64_t last_hit_ts  = 0;
};