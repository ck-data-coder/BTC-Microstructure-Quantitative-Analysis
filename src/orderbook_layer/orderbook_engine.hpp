#pragma once

#include <vector>
#include <unordered_map>
#include <cmath>
#include <fstream>
#include "../orderbook.h"
#include "best level/best_level_context.hpp"
#include "best level/best_level_state.hpp"
#include "side depth/side_book_state.hpp"
#include "best level/trade_aggregate.h"
#include "../rotating_file.hpp"
// struct TradeEvent;
// struct TradeAgg;

using BidTradeAggMap = std::unordered_map<double, TradeAgg>;
using AskTradeAggMap = std::unordered_map<double, TradeAgg>;

class OrderbookEngine {

public:

    OrderbookEngine();

    void process_orderbook_update(
        const OrderBook& old_book,
        const BookUpdate& u,
        const std::vector<TradeEvent>& trade_buffer,
        int64_t last_delta_ts,
        double pci_fragility,
        RotatingFile& logger
    );

private:

    // ----- STATES -----

    BestLevelContext bid_best_context {};
    BestLevelContext ask_best_context {};

    BestLevelState bid_best_level_state {};
    BestLevelState ask_best_level_state {};

    SideBookState bid_side_state {};
    SideBookState ask_side_state {};

    // ----- ENGINE STATE -----

    double LTP;

    // ----- CONSTANTS -----

    double ALPHA;
    double BETA;
    double TICK_SIZE;
    int tracked_level_N;

    static constexpr double SIZE_UNIT = 1.0;

    // ----- INTERNAL FUNCTIONS -----

    double hit_intensity(double qty) const;

    void aggregate_trades(
        const std::vector<TradeEvent>& trade_buffer,
        int64_t last_delta_ts,
        int64_t current_delta_ts,
        BidTradeAggMap& bid_trade_agg,
        AskTradeAggMap& ask_trade_agg
    );

    void print_orderbook_state(
        double ltp,
        double delta_time,
     const   BestLevelContext& bid_best_context,
   const BestLevelContext& ask_best_context,
   const SideBookState& bid_side_state,
   const  SideBookState& ask_side_state,
     std::ofstream& out
    );
};