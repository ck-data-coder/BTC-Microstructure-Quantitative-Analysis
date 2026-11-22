// timeframe_buffer.hpp
#pragma once
#include <vector>
#include "early_arrival_trade_event.hpp"
#include "early_arrival_ticker_event.hpp"
#include "early_arrival_cancel_and_refill_event.hpp"

struct TimeframeBuffer {
    std::vector<EarlyArrivalTradeEvent>        trades;
    std::vector<EarlyArrivalTickerEvent>       tickers;
    std::vector<EarlyArrivalCancelRefillEvent> cancel_refill;
};
