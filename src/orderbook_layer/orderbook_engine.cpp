#include "orderbook_engine.hpp"
#include "best level/best_level_update.hpp"
#include "side depth/side_book_state_update.hpp"
#include <fstream>
#include <iostream>
#include <ostream>
#include <vector>
OrderbookEngine::OrderbookEngine()
{
    LTP = 0.0;

    ALPHA = 0.1;
    BETA = 1;
    TICK_SIZE = 0.1;
    tracked_level_N = 11;
}

double OrderbookEngine::hit_intensity(double qty) const
{
    return 1.0 - std::exp(-qty / SIZE_UNIT);
}

void OrderbookEngine::aggregate_trades(
    const std::vector<TradeEvent>& trade_buffer,
    int64_t last_delta_ts,
    int64_t current_delta_ts,
    BidTradeAggMap& bid_trade_agg,
    AskTradeAggMap& ask_trade_agg
)
{
    for (const auto& t : trade_buffer) {

        if (t.ts < last_delta_ts || t.ts >= current_delta_ts)
            continue;

        if (t.is_buy) {

            auto& agg = ask_trade_agg[t.price];

            agg.qty += t.qty;
            agg.hit_strength += hit_intensity(t.qty);

            if (agg.first_hit_ts == 0)
                agg.first_hit_ts = t.ts;

            agg.last_hit_ts = t.ts;

        } else {

            auto& agg = bid_trade_agg[t.price];

            agg.qty += t.qty;
            agg.hit_strength += hit_intensity(t.qty);

            if (agg.first_hit_ts == 0)
                agg.first_hit_ts = t.ts;

            agg.last_hit_ts = t.ts;
        }
    }
}

inline std::map<double,double> vector_to_map(
    const std::vector<std::pair<double,double>>& v
){
    std::map<double,double> m;

    for(const auto& p : v)
        m[p.first] = p.second;

    return m;
}

void OrderbookEngine::process_orderbook_update(
    const OrderBook& old_book,
    const BookUpdate& u,
    const std::vector<TradeEvent>& trade_buffer,
    int64_t last_delta_ts,
    double pci_fragility,
    RotatingFile& logger
)
{

    // -----------------------------
    // 1. Aggregate trades
    // -----------------------------

    BidTradeAggMap bid_trade_agg;
    AskTradeAggMap ask_trade_agg;

    aggregate_trades(
        trade_buffer,
        last_delta_ts,
        u.book_ts,
        bid_trade_agg,
        ask_trade_agg
    );


    // -----------------------------
    // 2. Build new orderbook
    // -----------------------------

    OrderBook new_book = old_book;
    new_book.apply_delta(u);

    int64_t now = u.book_ts;


    // -----------------------------
    // 3. Update LTP
    // -----------------------------

    if (!trade_buffer.empty())
        LTP = trade_buffer.back().price;

    double last_traded_price =
        (LTP != 0.0) ? LTP : new_book.asks.begin()->first;


    // -----------------------------
    // 4. BID SIDE UPDATE
    // -----------------------------
    auto bid_delta_map = vector_to_map(u.bids);
    auto ask_delta_map = vector_to_map(u.asks);

    update_best_level_state(
        bid_best_context,
        bid_best_level_state,
        old_book.bids,
        new_book.bids,
        bid_delta_map,
        bid_trade_agg,
        pci_fragility,
        now,
        last_delta_ts,
        true
    );


    update_side_book_state(
        bid_side_state,
        new_book.bids,
        tracked_level_N,
        TICK_SIZE,
        bid_best_context.hit_rate_ema,
        last_traded_price,
        true,
        pci_fragility,
        ALPHA,
        BETA
    );


    // -----------------------------
    // 5. ASK SIDE UPDATE
    // -----------------------------

    update_best_level_state(
        ask_best_context,
        ask_best_level_state,
        old_book.asks,
        new_book.asks,
        ask_delta_map,
        ask_trade_agg,
        pci_fragility,
        now,
        last_delta_ts,
        false
    );


    update_side_book_state(
        ask_side_state,
        new_book.asks,
        tracked_level_N,
        TICK_SIZE,
        ask_best_context.hit_rate_ema,
        last_traded_price,
        false,
        pci_fragility,
        ALPHA,
        BETA
    );
   double delta_time= (now-last_delta_ts)/1000.0;

   logger.rotate_if_needed();
print_orderbook_state(
    last_traded_price,
delta_time,
bid_best_context,
ask_best_context,
bid_side_state,
ask_side_state,
logger.out
);
}



void OrderbookEngine::print_orderbook_state(
    double ltp,
    double delta_time,
    const BestLevelContext& bid_ctx,
    const BestLevelContext& ask_ctx,
    const SideBookState& bid_side,
    const SideBookState& ask_side,
    std::ofstream& out
)
{
    auto print_vector = [&](const std::vector<double>& v)
    {
        out << "[";
        for(size_t i = 0; i < v.size(); ++i)
        {
            out << v[i];
            if(i + 1 < v.size()) out << ",";
        }
        out << "]";
    };

    auto print_best_level = [&](const BestLevelContext& ctx)
    {
        out << "{";

        out << "\"best_level_unresolved_fragility\":" << ctx.best_level_unresolved_fragility << ",";
        out << "\"best_level_fragility_delta_ema\":" << ctx.best_level_fragility_delta_ema << ",";
        out << "\"best_level_behavioral_pressure_rate\":" << ctx.best_level_behavioral_pressure_rate << ",";
        out << "\"best_level_behavioral_pressure_intensity\":" << ctx.best_level_behavioral_pressure_intensity << ",";
        out << "\"best_level_inst_behavioral_pressure_rate\":" << ctx.best_level_inst_behavioral_pressure_rate << ",";
        out << "\"best_level_inst_behavioral_pressure_intensity\":" << ctx.best_level_inst_behavioral_pressure_intensity << ",";
        out << "\"hit_rate_ema\":" << ctx.hit_rate_ema << ",";
        out << "\"hit_intensity_ema\":" << ctx.hit_intensity_ema << ",";
        out << "\"cancel_rate_ema\":" << ctx.cancel_rate_ema << ",";
        out << "\"refresh_rate_ema\":" << ctx.refresh_rate_ema << ",";
        out << "\"cancel_intensity_ema\":" << ctx.cancel_intensity_ema << ",";
        out << "\"refresh_intensity_ema\":" << ctx.refresh_intensity_ema << ",";
        out << "\"willingness_ratio\":" << ctx.willingness_ratio << ",";
        out << "\"commitment_balance\":" << ctx.commitment_balance << ",";
        out << "\"cancel_rate_acceleration\":" << ctx.cancel_rate_acceleration << ",";
        out << "\"refresh_rate_acceleration\":" << ctx.refresh_rate_acceleration << ",";
        out << "\"cancel_intensity_acceleration\":" << ctx.cancel_intensity_acceleration << ",";
        out << "\"refresh_intensity_acceleration\":" << ctx.refresh_intensity_acceleration << ",";
        out << "\"cancel_rate_stress\":" << ctx.cancel_rate_stress << ",";
        out << "\"cancel_intensity_stress\":" << ctx.cancel_intensity_stress << ",";
        out << "\"refresh_rate_stress\":" << ctx.refresh_rate_stress << ",";
        out << "\"refresh_intensity_stress\":" << ctx.refresh_intensity_stress << ",";
        out << "\"refill_latency_ema\":" << ctx.refill_latency_ema << ",";
        out << "\"refill_latency_slope\":" << ctx.refill_latency_slope << ",";
        out << "\"refill_latency_stress\":" << ctx.refill_latency_stress << ",";
        out << "\"survival_stress\":" << ctx.survival_stress << ",";
        out << "\"raw_survival_stress\":" << ctx.raw_survival_stress << ",";
        out << "\"deterioration_strike\":" << ctx.deterioration_strike;

        out << "}";
    };

    auto print_sidebook = [&](const SideBookState& side)
    {
        out << "{";

        out << "\"depth_cascade_potential\":";
        print_vector(side.depth_cascade_potential);
        out << ",";

        out << "\"depth_cascade_potential_ema\":";
        print_vector(side.depth_cascade_potential_ema);
        out << ",";

        out << "\"visible_resistance\":";
        print_vector(side.visible_resistance);
        out << ",";

        out << "\"visible_resistance_ema\":";
        print_vector(side.visible_resistance_ema);
        out << ",";

        out << "\"true_resistance\":";
        print_vector(side.true_resistance);
        out << ",";

        out << "\"true_resistance_ema\":";
        print_vector(side.true_resistance_ema);
        out << ",";

        out << "\"depth_structure\":";
        print_vector(side.depth_structure);
        out << ",";

        out << "\"depth_structural_delta\":";
        print_vector(side.depth_structural_delta);
        out << ",";

        out << "\"depth_structure_ema\":";
        print_vector(side.depth_structure_ema);
        out << ",";

        out << "\"depth_structural_delta_ema\":";
        print_vector(side.depth_structural_delta_ema);

        out << "}";
    };

    out << "{";
 
    out << "\"ltp\":" << ltp<<", ";
    out << "\"delta_time\":" << delta_time<<", ";
    out << "\"bid\":{";
    out << "\"best_level_state\":";
    print_best_level(bid_ctx);
    out << ",";
    out << "\"depth_level_state\":";
    print_sidebook(bid_side);
    out << "},";

    out << "\"ask\":{";
    out << "\"best_level_state\":";
    print_best_level(ask_ctx);
    out << ",";
    out << "\"depth_level_state\":";
    print_sidebook(ask_side);
    out << "}";

    out << "}";

    out << "\n";
}