#pragma once

#include <map>
#include "side_book_state_parameters.hpp"
#include "side_book_state.hpp"

template<typename Compare>
void update_side_book_state(
    SideBookState& side,
    const std::map<double,double,Compare>& new_side,
    int tracked_level_N,
    double tick_size,
    double hit_rate_ema,
    double last_traded_price,
    bool is_bid,
    double pci_fragility,
    double alpha,
    double beta
)
{
    auto it = new_side.begin();
    int level_index = 0;

    // use template comparator
    std::map<double,double,Compare> side_map;

    for(; it != new_side.end() && level_index < tracked_level_N; ++it, ++level_index)
    {
        side_map[it->first] = it->second;
    }

    update_depth_cascade_potential(
        side,
        side_map,
        tick_size,
        hit_rate_ema,
        last_traded_price
    );

    update_sidebook_resistance(
        side,
        side_map,
        tick_size,
        pci_fragility,
        hit_rate_ema,
        alpha,
        beta,
        is_bid,
        last_traded_price
    );

    update_depth_structural_delta(
        side,
        side_map,
        tick_size
    );
}