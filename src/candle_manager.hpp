// src/candle_manager.hpp
#pragma once
#include "candle.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
struct CandleManager {
    int64_t tf_ms;
    Candle current;

    CandleManager(int64_t tf)
        : tf_ms(tf) {}

    bool in_candle(int64_t ts) const {
        return ts >= current.start && ts <= current.end;
    }

    void start_new(int64_t start_ts, int64_t end_ts) {
        current = Candle{};
        current.start = start_ts;
        current.end   = end_ts;
    }

inline std::string ms_to_local(int64_t ms) {
    using namespace std::chrono;

    std::time_t t = ms / 1000;
    std::tm tm = *std::localtime(&t);   // 👈 LOCAL TIME

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
inline double safe_ratio(double num, double den) {
    return den > 0 ? num / den : 0.0;
}



inline void print_buckets(
    std::ostream& out,
    const Buckets& b,
    const std::string& label
) {
    out << label << "\n";

    out << "  CANCEL:\n";
    out << "    0.001–0.01 : " << b.cancel_001_01 << "\n";
    out << "    0.011–0.1  : " << b.cancel_01_1   << "\n";
    out << "    0.11–0.5   : " << b.cancel_1_5    << "\n";
    out << "    0.51–1     : " << b.cancel_5_10   << "\n";
    out << "    1.01–5     : " << b.cancel_10_50  << "\n";
    out << "    >5         : " << b.cancel_50p    << "\n";

    out << "  REFILL:\n";
    out << "    0.001–0.01 : " << b.refill_001_01 << "\n";
    out << "    0.011–0.1  : " << b.refill_01_1   << "\n";
    out << "    0.11–0.5   : " << b.refill_1_5    << "\n";
    out << "    0.51–1     : " << b.refill_5_10   << "\n";
    out << "    1.01–5     : " << b.refill_10_50  << "\n";
    out << "    >5         : " << b.refill_50p    << "\n";
}


   void finalize(std::ostream& out) {
    auto& T = current.ticker;
    out << "\n=================================================\n";
    out << "CANDLE\n";
    out << "Start : " << ms_to_local(current.start)
        << " (" << current.start << ")\n";
    out << "End   : " << ms_to_local(current.end)
        << " (" << current.end << ")\n";

    // =====================
    // Trade flow
    // =====================
    out << "\n--- TRADE FLOW ---\n";
    out << "BuyVol   = " << current.buy_vol << "\n";
    out << "SellVol  = " << current.sell_vol << "\n";
    out << "KlineVol = " << current.volume << "\n";
    out << "CalcVol  = " << (current.buy_vol + current.sell_vol) << "\n";
    out << "open     = " << current.open<<"\n";
    out << "high     = " << current.high<<"\n";
    out << "low      = " << current.low<<"\n";
    out << "close    = " << current.close<<"\n";


    out << "ticker open     = " << T.start_last<<"\n";
    out << "ticker close    = " << T.end_last<<"\n";
    out << "ticker mark open     = " << T.start_mark<<"\n";
    out << "ticker mark close    = " << T.end_mark<<"\n";
    out << "ticker starting oi   = " << T.start_oi<<"\n";
    out << "ticker ending oi   = " << T.end_oi<<"\n";
    out << "ticker starting funding   = " << T.start_funding<<"\n";
    out << "ticker ending funding   = " << T.end_funding<<"\n";
    

    // =====================
    // PRICE LEVEL HITS
    // =====================

    out << "\n--- BID HIT (Sell trades) ---\n";
    for (auto& [p, v] : current.bid_hit)
        out << p << " : " << v << "\n";

    out << "\n--- ASK HIT (Buy trades) ---\n";
    for (auto& [p, v] : current.ask_hit)
        out << p << " : " << v << "\n";

    // =====================
    // ORDERBOOK BEHAVIOR
    // =====================
    auto& B = current.bid;
    auto& A = current.ask;

     out << "\n--- ORDERBOOK METRICS ---\n";

    out << "\n[BID SIDE]\n";
    out << "Best  : cancel=" << B.best.cancel_vol
        << " refill=" << B.best.refill_vol << "\n";

    out << "Best5 : cancel=" << B.best5.cancel_vol
        << " refill=" << B.best5.refill_vol << "\n";

    out << "Best10: cancel=" << B.best10.cancel_vol
        << " refill=" << B.best10.refill_vol << "\n";

    out << "\n[ASK SIDE]\n";
    out << "Best  : cancel=" << A.best.cancel_vol
        << " refill=" << A.best.refill_vol << "\n";

    out << "Best5 : cancel=" << A.best5.cancel_vol
        << " refill=" << A.best5.refill_vol << "\n";

    out << "Best10: cancel=" << A.best10.cancel_vol
        << " refill=" << A.best10.refill_vol << "\n";




        out << "\n--- BID BUCKETS ---\n";
print_buckets(out, current.bid.best.buckets,   "[BEST]");
print_buckets(out, current.bid.best5.buckets,  "[BEST 5]");
print_buckets(out, current.bid.best10.buckets, "[BEST 10]");



out << "\n--- ASK BUCKETS ---\n";
print_buckets(out, current.ask.best.buckets,   "[BEST]");
print_buckets(out, current.ask.best5.buckets,  "[BEST 5]");
print_buckets(out, current.ask.best10.buckets, "[BEST 10]");

//     // =====================
//     // RATIOS (IMPORTANT)
//     // =====================
//     out << "\n--- RATIOS ---\n";

//     // Refill dominance
    out << "Bid Best refill / Best5 cancel = "
        << safe_ratio(B.best.refill_vol, B.best5.cancel_vol) << "\n";

    out << "Bid Best refill / Best10 cancel = "
        << safe_ratio(B.best.refill_vol, B.best10.cancel_vol) << "\n";

    out << "Ask Best refill / Best5 cancel = "
        << safe_ratio(A.best.refill_vol, A.best5.cancel_vol) << "\n";

    out << "Ask Best refill / Best10 cancel = "
        << safe_ratio(A.best.refill_vol, A.best10.cancel_vol) << "\n";

    // Cancel pressure
    out << "Bid Best cancel / Best5 refill = "
        << safe_ratio(B.best.cancel_vol, B.best5.refill_vol) << "\n";

    out << "Bid Best cancel / Best10 refill = "
        << safe_ratio(B.best.cancel_vol, B.best10.refill_vol) << "\n";

    out << "Ask Best cancel / Best5 refill = "
        << safe_ratio(A.best.cancel_vol, A.best5.refill_vol) << "\n";

    out << "Ask Best cancel / Best10 refill = "
        << safe_ratio(A.best.cancel_vol, A.best10.refill_vol) << "\n";

        
     
        out << "\n--- TICKER METRICS ---\n";
        out << "NetPrice change   = " << (T.end_last - T.start_last) << "\n";
        out << "AbsPrice          = " << T.abs_last << "\n";
        out << "NetOI change      = " << (T.end_oi - T.start_oi) << "\n";
        out << "AbsOI             = " << T.abs_oi << "\n";
        out << "NetFunding change = " << (T.end_funding - T.start_funding) << "\n";
        out << "AbsFunding        = " << T.abs_funding << "\n";
        out << "BasisAbs          = " << T.abs_basis << "\n";
        out << "=================================================\n";

        out << std::flush;
   }

};
