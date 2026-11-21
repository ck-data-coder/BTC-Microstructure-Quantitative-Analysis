#pragma once
#include <vector>
#include <utility>
#include <cstdint>

// JUST DATA — no logic, no state
struct BookUpdate {
    bool is_snapshot;     // snapshot or delta
    int64_t seq;          // sequence number
    int64_t book_ts;
    // price , size
    std::vector<std::pair<double,double>> bids;
    std::vector<std::pair<double,double>> asks;
};
