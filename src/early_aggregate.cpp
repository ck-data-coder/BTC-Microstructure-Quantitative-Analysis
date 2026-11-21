#include "early_aggregate.hpp"
#include <cmath>
#include <unordered_map>
#include "ticker.hpp"
#include "trade_processor.hpp"
// ==================== ALL ====================

void aggregate_all(
    CandleManager& mgr,
    std::unordered_map<int64_t, TimeframeBuffer>& buffers,
     TickerPreviousState& previous_state
) {
    aggregate_trades(mgr, buffers);
    aggregate_ticker(mgr, buffers,previous_state);
    aggregate_cancel_refill(mgr, buffers);

    // 🔥 clear after use
    buffers[mgr.tf_ms].trades.clear();
    buffers[mgr.tf_ms].tickers.clear();
    buffers[mgr.tf_ms].cancel_refill.clear();
}

// ==================== TRADES ====================

void aggregate_trades(
    CandleManager& mgr,
    std::unordered_map<int64_t, TimeframeBuffer>& buffers
) {
    auto& buf = buffers[mgr.tf_ms];

    for (auto& t : buf.trades) {
        if (!mgr.in_candle(t.ts)){
            continue;
        }
        if (t.is_buy){
            mgr.current.buy_vol += t.qty;
             mgr.current.ask_hit[t.price] += t.qty;
        }
        else{
            mgr.current.sell_vol += t.qty;
             mgr.current.bid_hit[t.price] += t.qty;
        }
    }
}

// ==================== TICKER ====================

void aggregate_ticker(
    CandleManager& mgr,
    std::unordered_map<int64_t, TimeframeBuffer>& buffers,
     TickerPreviousState& live_current
) {
    auto& buf = buffers[mgr.tf_ms];
    // auto& T = mgr.current.ticker;
    if(buf.tickers.empty()){
        return;
    }
    auto& early=buf.tickers;

    for (size_t i = 1; i < early.size(); ++i) {
        if(!mgr.in_candle(early[i-1].ts) || !mgr.in_candle(early[i].ts))
        continue;
        update_ticker_metrics(
            mgr.current,
            early[i-1].last,early[i-1].mark,early[i-1].index,early[i-1].oi,   // prev
            early[i].last,early[i].mark,early[i].index,early[i].oi      // curr
        );
    }

    // 3️⃣ Compare LAST early ticker → live ticker
    if(mgr.in_candle(early.back().ts) && mgr.in_candle(live_current.ts)){
    
    update_ticker_metrics(
        mgr.current,
        early.back().last, early.back().mark, early.back().index, early.back().oi,     // prev
        live_current.last,live_current.mark,live_current.index,live_current.oi      // curr
    );
}
}

// ================= CANCEL / REFILL =================




void aggregate_cancel_refill(
    CandleManager& mgr,
    std::unordered_map<int64_t, TimeframeBuffer>& buffers
) {
    auto& buf = buffers[mgr.tf_ms];

    for (auto& e : buf.cancel_refill) {
     
        if (!mgr.in_candle(e.ts)){
            continue;
        }
      if(e.side== Side::Bid){
         auto& side = mgr.current.bid;

            if (e.level == 0) {
                side.best.cancel_vol  += e.cancle_quantity;
                side.best.refill_vol  += e.refill_quantity;
                add_to_bucket(side.best.buckets,  e.cancle_quantity, false);
                add_to_bucket(side.best.buckets, e.refill_quantity, true);
            }

            if (e.level <= 4) {
                side.best5.cancel_vol += e.cancle_quantity;
                side.best5.refill_vol +=e.refill_quantity;
                add_to_bucket(side.best5.buckets, e.cancle_quantity, false);
                add_to_bucket(side.best5.buckets, e.refill_quantity, true);
            }

            if (e.level <= 9) {
                side.best10.cancel_vol +=e.cancle_quantity;
                side.best10.refill_vol += e.refill_quantity;
                add_to_bucket(side.best10.buckets, e.cancle_quantity, false);
                add_to_bucket(side.best10.buckets, e.refill_quantity, true);
            }
      }
      else{

         auto& side = mgr.current.ask;

            if (e.level == 0) {
                side.best.cancel_vol  += e.cancle_quantity;
                side.best.refill_vol  += e.refill_quantity;
                add_to_bucket(side.best.buckets, e.cancle_quantity, false);
                add_to_bucket(side.best.buckets, e.refill_quantity, true);
            }

            if (e.level <= 4) {
                side.best5.cancel_vol += e.cancle_quantity;
                side.best5.refill_vol += e.refill_quantity;
                add_to_bucket(side.best5.buckets, e.cancle_quantity, false);
                add_to_bucket(side.best5.buckets, e.refill_quantity, true);
            }

            if (e.level <= 9) {
                side.best10.cancel_vol += e.cancle_quantity;
                side.best10.refill_vol += e.refill_quantity;
                add_to_bucket(side.best10.buckets, e.cancle_quantity, false);
                add_to_bucket(side.best10.buckets, e.refill_quantity, true);
            }
      }
    }
}
