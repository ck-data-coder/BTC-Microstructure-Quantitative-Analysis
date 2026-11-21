#pragma once
#include <cstdint>

struct BestLevelState{
    double price;
    double size;
    double survival_time_at_best;       // seconds

    bool is_hit;
    bool is_awaiting_refill;
    double refill_latency_raw;       // Li (ms)
    double expected_refill_size;
    double total_refill_size;
     

   int64_t  last_update_ts;
};