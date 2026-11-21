#pragma once
#include <map>
#include <cstdint>
#include "orderbook_types.h"

class OrderBook {
public:
double best_bid_size() const;

double best_ask_size() const;


size_t bid_size() const;
    size_t ask_size() const;
    void apply_snapshot(const BookUpdate& u);
    bool apply_delta(const BookUpdate& u);
 void print_book(std::ofstream& out) const;
// private:
    void trim();

    static constexpr size_t MAX_LEVELS = 50;
    int64_t last_seq = -1;

    // price → size
    std::map<double,double,std::greater<double>> bids; // high → low
    std::map<double,double,std::less<double>> asks;    // low → high
};
