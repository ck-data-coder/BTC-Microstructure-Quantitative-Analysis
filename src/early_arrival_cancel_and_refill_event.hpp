// cancel_refill_event.hpp
#pragma once
#include <cstdint>

// enum class CRType { Cancel, Refill };
enum class Side   { Bid, Ask };

struct EarlyArrivalCancelRefillEvent {
    int64_t ts;
    Side    side;
    double cancle_quantity;
    double refill_quantity;
    int     level;
    
};
