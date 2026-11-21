// src/trade_processor.hpp
#pragma once
#include <boost/json.hpp>
#include <string>
#include "candle_manager.hpp"
#include <unordered_map>
#include "early_arrival_timeframe_buffer.hpp"
namespace json = boost::json;

inline double to_double(const json::value& v) {
    return std::stod(std::string(v.as_string().c_str()));
}

 void apply_trade(
    CandleManager& mgr,
    const json::object& trade,
    std::unordered_map<int64_t, TimeframeBuffer>& tf_buffers
);

 void add_to_bucket(Buckets& b, double qty, bool is_refill);