// src/ticker.hpp
#pragma once

#include "candle.hpp"
#include <iostream>
#include <string>

double funding_proxy(double mark, double index);

 void update_ticker_metrics(
    Candle& c,
    double prev_last, double prev_mark, double prev_index,
    double prev_oi,
    double cur_last, double cur_mark, double cur_index,
    double cur_oi
) ;