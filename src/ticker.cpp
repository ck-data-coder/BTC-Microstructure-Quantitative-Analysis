// src/ticker.cpp
#include "ticker.hpp"
#include <cmath>

 double funding_proxy(double mark, double index) {
    return index == 0 ? 0.0 : (mark - index) / index;
}

 void update_ticker_metrics(
    Candle& c,
    double prev_last, double prev_mark, double prev_index,
    double prev_oi,
    double cur_last, double cur_mark, double cur_index,
    double cur_oi
) {
    double prev_funding = funding_proxy(prev_mark, prev_index);
    double cur_funding  = funding_proxy(cur_mark, cur_index);

    double prev_basis = prev_mark - prev_index;
    double cur_basis  = cur_mark - cur_index;

    if (!c.ticker.initialized) {
        c.ticker.start_last = prev_last;
        c.ticker.start_mark = prev_mark;
        c.ticker.start_index = prev_index;
        c.ticker.start_oi = prev_oi;
        c.ticker.start_funding = prev_funding;
        c.ticker.initialized = true;
    }

    c.ticker.abs_last   += std::abs(cur_last - prev_last);
    c.ticker.abs_mark   += std::abs(cur_mark - prev_mark);
    c.ticker.abs_index  += std::abs(cur_index - prev_index);
    c.ticker.abs_basis  += std::abs(cur_basis - prev_basis);
    c.ticker.abs_oi     += std::abs(cur_oi - prev_oi);
    c.ticker.abs_funding += std::abs(cur_funding - prev_funding);

    c.ticker.end_last = cur_last;
    c.ticker.end_mark = cur_mark;
    c.ticker.end_index = cur_index;
    c.ticker.end_oi = cur_oi;
    c.ticker.end_funding = cur_funding;
}
