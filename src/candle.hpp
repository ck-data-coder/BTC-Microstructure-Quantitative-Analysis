// src/candle.hpp
#pragma once
#include <cstdint>
#include <unordered_map>


struct Buckets {
    // cancellation counts
    uint32_t cancel_001_01 = 0;
    uint32_t cancel_01_1   = 0;
    uint32_t cancel_1_5    = 0;
    uint32_t cancel_5_10   = 0;
    uint32_t cancel_10_50  = 0;
    uint32_t cancel_50p    = 0;

    // refill counts
    uint32_t refill_001_01 = 0;
    uint32_t refill_01_1   = 0;
    uint32_t refill_1_5    = 0;
    uint32_t refill_5_10   = 0;
    uint32_t refill_10_50  = 0;
    uint32_t refill_50p    = 0;
};



struct LevelMetrics {
    double cancel_vol = 0.0;
    double refill_vol = 0.0;

    Buckets buckets;
};

struct SideMetrics {
    // Explicit level groups
    LevelMetrics best;
    LevelMetrics best5;
    LevelMetrics best10;
};


struct TickerMetrics {
    double start_last = 0, end_last = 0;
    double start_mark = 0, end_mark = 0;
    double start_index = 0, end_index = 0;
    double start_oi = 0, end_oi = 0;
    double start_funding = 0, end_funding = 0;

    double abs_last = 0;
    double abs_mark = 0;
    double abs_index = 0;
    double abs_basis = 0;
    double abs_oi = 0;
    double abs_funding = 0;

    bool initialized = false;
};

struct Candle {
    int64_t start;
    int64_t end;

    // OHLCV from kline
    double open = 0;
    double high = 0;
    double low  = 0;
    double close = 0;
    double volume = 0;
    bool confirmed = false;

    // Flow metrics (from trades)
    double buy_vol = 0;
    double sell_vol = 0;

    // price -> volume
    std::unordered_map<double, double> bid_hit; // Sell
    std::unordered_map<double, double> ask_hit; // Buy

    
    // ===== Orderbook behavior =====
    SideMetrics bid;
    SideMetrics ask;

    TickerMetrics ticker;

};
