#pragma once
#include <cmath>
#include<cstdint>
#include <string>
struct PCIIndicators {
    int64_t ts_ms;
    std::string ist_time;
    double ltp=0;

    // ---- EMA of PCI projections ----
    double pure_up_pci_ema = 0.0;
    double pure_down_pci_ema = 0.0;
    double resistance_down_pci_ema = 0.0;
    double support_up_pci_ema = 0.0;

    // ---- Survival EMAs ----
    double relative_survival_up_ema = 0.0;
    double relative_survival_down_ema = 0.0;

    // ---- Speed EMAs ----
    double buy_volume_speed_ema = 0.0;
    double sell_volume_speed_ema = 0.0;

    double buy_participation_speed_ema = 0.0;
    double sell_participation_speed_ema = 0.0;

    double local_buy_inventory=0;
    double local_sell_inventory=0;  
    // ---- Inventory ----
    double buy_inventory = 0.0;
    double sell_inventory = 0.0;

    double buy_inventory_acceleration_ema;
    double sell_inventory_acceleration_ema;
    double buy_inventory_acceleration_per_unit_time_ema;
    double sell_inventory_acceleration_per_unit_time_ema;

    // ---- Market fragility ----
    double fragility_score = 0.0;

    
};