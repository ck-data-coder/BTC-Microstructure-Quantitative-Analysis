#pragma once
#include <unordered_map>
#include <cstdint>
#include "candle_manager.hpp"
#include "early_arrival_timeframe_buffer.hpp"
#include "ticker_previous_state.hpp"


void aggregate_all(
    CandleManager& mgr,
    std::unordered_map<int64_t, TimeframeBuffer>& buffers,
    TickerPreviousState& previous_state
);

void aggregate_trades(
    CandleManager& mgr,
    std::unordered_map<int64_t, TimeframeBuffer>& buffers
);

void aggregate_ticker(
    CandleManager& mgr,
    std::unordered_map<int64_t, TimeframeBuffer>& buffers,
    TickerPreviousState& previous_state
);

void aggregate_cancel_refill(
    CandleManager& mgr,
    std::unordered_map<int64_t, TimeframeBuffer>& buffers
);