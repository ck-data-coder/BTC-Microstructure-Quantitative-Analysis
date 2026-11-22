// ticker_event.hpp
#pragma once
#include <cstdint>

struct EarlyArrivalTickerEvent {
    int64_t ts;

 

    double last  = 0;
    double mark  = 0;
    double index = 0;
    double oi    = 0;
};
