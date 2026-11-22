#include "orderbook.h"
#include<iostream>
#include <fstream>

size_t OrderBook::bid_size() const {
    return bids.size();
}

size_t OrderBook::ask_size() const {
    return asks.size();
}


double OrderBook::best_bid_size() const {
    if (bids.empty())
        return 0.0;
    return bids.begin()->second;
}


double OrderBook::best_ask_size() const {
    if (asks.empty())
        return 0.0;
    return asks.begin()->second;
}



void OrderBook::print_book(std::ofstream& out) const {
    out << "\n========== ORDERBOOK ==========\n";
    // out << "Bid levels: " << bids.size() << "\n";
    // out << "Ask levels: " << asks.size() << "\n";

    out << "\nBIDS (price : size)\n";
    for (auto& [price, size] : bids) {
        out << price << " : " << size << "\n";
    }

    out << "\nASKS (price : size)\n";
    for (auto& [price, size] : asks) {
        out << price << " : " << size << "\n";
    }

    out << "===============================\n";
}


void OrderBook::apply_snapshot(const BookUpdate& u) {
    bids.clear();
    asks.clear();

    // u.bids = vector<pair<price,size>>
    for (auto& p : u.bids) {
        if (p.second > 0)           // size
            bids[p.first] = p.second; // price → size
    }

    for (auto& p : u.asks) {
        if (p.second > 0)
            asks[p.first] = p.second;
    }

    trim();
    last_seq = u.seq;
}

bool OrderBook::apply_delta(const BookUpdate& u) {
    if (u.seq <= last_seq)
        return false;

    for (auto& p : u.bids) {
        if (p.second == 0)
            bids.erase(p.first);
        else
            bids[p.first] = p.second;
    }

    for (auto& p : u.asks) {
        if (p.second == 0)
            asks.erase(p.first);
        else
            asks[p.first] = p.second;
    }

    trim();
    last_seq = u.seq;
    return true;
}

void OrderBook::trim() {
    while (bids.size() > MAX_LEVELS)
        bids.erase(std::prev(bids.end())); // worst bid

    while (asks.size() > MAX_LEVELS)
        asks.erase(std::prev(asks.end())); // worst ask
}
