#pragma once
#include <vector>
#include <cmath>
#include "pci.h"
#include <fstream>
#include "pci_indicators.h"
#include <string>
#include "../rotating_file.hpp"

struct Trade {
    int64_t ts;
    double price;
    double qty;
    bool is_buy;
};

struct TradeMessage {
    int64_t seq;
    std::vector<Trade> trades;
};

class PCIEngine {
public:

    PCIEngine(RotatingFile& logger);

    void on_message(const TradeMessage& msg);
    double publish_fragility();
    void reset_seq();
private:
    RotatingFile& logger;
    // state
    bool has_seen_first_trade = false;
    bool pci_active = false;

    int64_t current_seq = -1;
    int64_t current_pde_id = 0;

    double anchor = 0.0;
    double last_price = 0.0;

    PCI current_pci{};
    PCIIndicators pci_indicators;

    std::vector<PCI> pci_history;

    // lifecycle
    void start_new_pci(int64_t seq, int64_t start_ts, int64_t first_trade_ts, double prev_ltp);
    void update_with_trade(const Trade& t);
    void close_and_finalize_pci();
    void try_anchor_update(const PCI& p);

    // inventory & fragility
    void update_market_inventory(const PCI& pci);
    double net_inventory() const;
    double compute_fragility() const;

    // acceptance
    bool accept_up(const PCI& p) const;
    bool accept_down(const PCI& p) const;
    void update_pci_indicators(const PCI& p);
    inline void update_ema_with_decay(double& ema, double value, double alpha, double reducing_factor);
     void print_pci(std::ofstream& out, const PCI& p);
     size_t print_pci_indicators(std::ofstream& out, const PCIIndicators& ind);
     std::string millis_to_ist(int64_t millis);
};




