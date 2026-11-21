#pragma once
#include <cstdint>
struct TickerPreviousState
{
    double last=0.0;
    double mark=0.0;
    double index=0.0;
    double oi=0.0;
    int64_t ts;
};
