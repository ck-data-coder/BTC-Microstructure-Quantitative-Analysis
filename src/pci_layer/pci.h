#pragma once
#include<cstdint>

// PCI-- Pressure Compression Index
struct PCI {
    // identity
    int64_t pde_id;
    int64_t seq;

    // timestamps
    int64_t start_ts;
    int64_t last_trade_ts;
    int64_t end_ts;

    // price
    double final_ltp;
      double previous_pci_last_price;
    double effective_delta;   // |final_ltp - previous_pci_last_price|

    // aggression volume (whole seq)
    double buy_vol;
    double sell_vol;

    // size-normalized participation (0–1 weighted)
    double buy_participation;
    double sell_participation;

    // aggression active time (side-attributed)
    double buy_active_time;
    double sell_active_time;

    // survival (path-aware, anchor-based)
    double up_survival;
    double down_survival;
    double anchor_survival;

    // absolute path deltas
    double abs_up_delta;
    double abs_down_delta;

    double abs_up_effort_from_anchor;
double abs_down_effort_from_anchor;

    bool 
is_trade_start_from_anchor;

    bool 
is_trade_start_above_anchor;

   bool
is_trade_start_below_anchor;

    // directional PCI projections
    double pure_up_pci;
    double resistance_down_pci;
    double pure_down_pci;
    double support_up_pci;

    // relative survival vs absolute distance
    double relative_survival_up;
    double relative_survival_down;

    // aggression speeds
    double buy_volume_speed;
    double sell_volume_speed;
    double buy_participation_speed;
    double sell_participation_speed;

double buy_resistance;     // ∈ [0,1]
double sell_resistance;    // ∈ [0,1]

double buy_compression;    // effort × resistance
double sell_compression;

    
    // validity
    bool isvalid;
};



