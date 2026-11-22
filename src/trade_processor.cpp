#include "trade_processor.hpp"

void apply_trade(
    CandleManager& mgr,
    const json::object& trade,
    std::unordered_map<int64_t, TimeframeBuffer>& tf_buffers
) {
    int64_t ts = trade.at("T").as_int64();
    if (!mgr.in_candle(ts)){
        double price = std::stod(trade.at("p").as_string().c_str());
        double qty  = std::stod(trade.at("v").as_string().c_str());
        bool is_buy= trade.at("S").as_string()=="Buy";
   
        tf_buffers[mgr.tf_ms].trades.push_back({
        price,
        qty,
        is_buy,
        ts
     });
         return;
    }
    double price = to_double(trade.at("p"));
    double qty   = to_double(trade.at("v"));
    std::string side = trade.at("S").as_string().c_str();

    if (side == "Buy") {
        mgr.current.buy_vol += qty;
        mgr.current.ask_hit[price] += qty;
    } else {
        mgr.current.sell_vol += qty;
        mgr.current.bid_hit[price] += qty;
    }
}

 void add_to_bucket(Buckets& b, double qty, bool is_refill) {
    if (qty <= 0) return;

    if (qty >= 0.001 && qty <= 0.01) {
        is_refill ? b.refill_001_01++ : b.cancel_001_01++;
    }
    else if (qty <= 0.1) {
        is_refill ? b.refill_01_1++ : b.cancel_01_1++;
    }
    else if (qty <= 0.5) {
        is_refill ? b.refill_1_5++ : b.cancel_1_5++;
    }
    else if (qty <= 1.0) {
        is_refill ? b.refill_5_10++ : b.cancel_5_10++;
    }
    else if (qty <= 5.0) {
        is_refill ? b.refill_10_50++ : b.cancel_10_50++;
    }
    else {
        is_refill ? b.refill_50p++ : b.cancel_50p++;
    }
}