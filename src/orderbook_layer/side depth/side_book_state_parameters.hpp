#pragma once

#include <cmath>
#include "side_book_state.hpp"
#include <map>


 void update_depth_cascade_potential_ema(SideBookState& side)
{
    const size_t n = side.depth_cascade_potential.size();
    const size_t ema_n = side.depth_cascade_potential_ema.size();

    // ---- grow EMA vector if new depth levels appear ----
    if (n > ema_n) {
        side.depth_cascade_potential_ema.resize(n, 0.0);
    }

    for (size_t i = 0; i < n; ++i) {

        double curr = side.depth_cascade_potential[i];

        double prev =
            (i < side.prev_depth_cascade_potential.size())
                ? side.prev_depth_cascade_potential[i]
                : curr; // new depth → no artificial jump

        double delta = curr - prev;
        double activity = std::abs(delta);

        // adaptive learning rate
        double alpha = activity / (1.0 + activity);

        side.depth_cascade_potential_ema[i] =
            (1.0 - alpha) * side.depth_cascade_potential_ema[i]
          + alpha * curr;
    }
}





template<typename Compare>
 void update_depth_cascade_potential(
    SideBookState& side,
    const std::map<double, double,Compare>& price_size_map,
    double tick_size,
    double hit_rate_ema,
    double last_traded_price
) {
    constexpr double MIN_SIZE = 0.001;
    constexpr int MAX_LEVELS = 10;

   side.prev_depth_cascade_potential=
side.depth_cascade_potential;

 side.depth_cascade_potential.clear();

    if (price_size_map.size() < 2)
        return;

    auto it = price_size_map.begin();

    // ---- best level (level 0) ----
   // double best_price = it->first;
    double prev_price = it->first;
    double prev_size  = std::max(it->second, MIN_SIZE);

    ++it; // move to level 1

    int level_index = 1;

    for (; it != price_size_map.end() && level_index <= MAX_LEVELS; ++it, ++level_index) {

        double price = it->first;
        double size  = it->second;

        // distances in ticks
        double tick_dist_from_ltp =
            std::abs(price - last_traded_price) / tick_size;

        double tick_dist_from_prev =
            std::abs(price - prev_price) / tick_size;

        // core geometric term
        double geometric_core =
            (  tick_dist_from_ltp  / level_index  ) *
            (tick_dist_from_prev / prev_size);

        double cascade_value =
            geometric_core > 0.0
                ? std::log1p(geometric_core) * (1.0 + hit_rate_ema)
                : 0.0;

        side.depth_cascade_potential.push_back(cascade_value);

        // update prev
        prev_price = price;
        prev_size  = std::max(size, MIN_SIZE);
    }


update_depth_cascade_potential_ema(side);
}




 double directional_pci_contribution(
    bool is_bid,
    double pci_fragility
) {
    // Bid side resists downward moves
    // Ask side resists upward moves

    if (is_bid) {
        // downside fragility matters
        return std::max(0.0, -pci_fragility);
    } else {
        // upside fragility matters
        return std::max(0.0, pci_fragility);
    }
}




 void snapshot_previous_resistance_geometry(SideBookState& side)
{
    side.prev_visible_resistance = side.visible_resistance;
    side.prev_true_resistance= side.true_resistance;
}




 void update_visible_resistance_ema(SideBookState& side)
{
    const size_t N = side.visible_resistance.size();
   const size_t ema_N= side.visible_resistance_ema.size();
if(N>ema_N){ 
side.visible_resistance_ema.resize(N);
}
    for (size_t i = 0; i < N; ++i) {

        double curr = side.visible_resistance[i];
        double prev = (i < side.prev_visible_resistance.size())
                        ? side.prev_visible_resistance[i]
                        : curr;

        double delta = std::abs(curr - prev);
        double alpha = delta / (1.0 + delta);

        side.visible_resistance_ema[i] =
            (1.0 - alpha) * side.visible_resistance_ema[i]
            + alpha * curr;
    }
}



 void update_true_resistance_ema(SideBookState& side)
{
    const size_t N = side.true_resistance.size();
   const size_t ema_N= side.true_resistance_ema.size();
if(N>ema_N){
    side.true_resistance_ema.resize(N);
}
    for (size_t i = 0; i < N; ++i) {

        double curr = side.true_resistance[i];
        double prev = (i < side.prev_true_resistance.size())
                        ? side.prev_true_resistance[i]
                        : curr;

        double delta = std::abs(curr - prev);
        double alpha = delta / (1.0 + delta);

        side.true_resistance_ema[i] =
            (1.0 - alpha) * side.true_resistance_ema[i]
            + alpha * curr;
    }
}





 double compute_true_resistance(
    bool is_bid,
    double R_vis,
    double pci_fragility,
    double hit_rate_ema,
    double alpha,
    double beta
) {
    // Directional structural weakness
    double pci_term =
        directional_pci_contribution(is_bid, pci_fragility);

    // Raw aggression (no baseline subtraction)
    double hit_term = hit_rate_ema;

    // Final resistance (no leakage)
    double denom =
        1.0 + alpha * pci_term + beta * hit_term;

    return R_vis / denom;
}



template<typename Compare>
 void update_sidebook_resistance(
    SideBookState& side,
    const std::map<double, double, Compare>& price_size_map, // ordered best → outward
    double tick_size,
    double pci_fragility,
    double hit_rate_ema,
    double alpha,
    double beta,
     bool is_bid,
    double last_traded_price
) {

snapshot_previous_resistance_geometry(side);
    side.visible_resistance.clear();
    side.true_resistance.clear();

    if (price_size_map.size() < 2)
        return;

    auto it_prev = price_size_map.begin(); // best level
    auto it_curr = std::next(it_prev);

    double cumulative_R_vis = 0.0;
      int i=0;
    // Level index starts from 1 (never best = 0)
    for (; it_curr != price_size_map.end(); ++it_curr, ++it_prev,++i) {

        double price_prev = it_prev->first;
      if(i==0){
       price_prev=last_traded_price;
}
        double size_prev  = it_prev->second;

        double price_curr = it_curr->first;

        // Tick distance between adjacent levels
        double tick_dist =
            std::abs(price_curr - price_prev) / tick_size;

        // Safety: real books should never give 0, but protect anyway
        tick_dist = std::max(tick_dist, 1.0);

        // Visible resistance contribution from previous level
        cumulative_R_vis += size_prev / tick_dist;

        // Store visible resistance to reach this level
        side.visible_resistance.push_back(cumulative_R_vis);

        // Compute true resistance
        double R_true =
            compute_true_resistance(
                is_bid,
                cumulative_R_vis,
                pci_fragility,
                hit_rate_ema,
                alpha,
                beta
            );

        side.true_resistance.push_back(R_true);
    }
update_visible_resistance_ema(side);
update_true_resistance_ema(side);
}




 void snapshot_previous_depth_geometry(SideBookState& side)
{
    side.prev_depth_structure = side.depth_structure;
    side.prev_depth_structural_delta = side.depth_structural_delta;
}



 void update_depth_structure_ema(SideBookState& side)
{
    const size_t n = side.depth_structure.size();
const size_t ema_N=side.depth_structure_ema.size
();
if(n>ema_N){
    side.depth_structure_ema.resize(n, 0.0);
}
    for (size_t i = 0; i < n; ++i) {
        double current = side.depth_structure[i];

        double previous =
            (i < side.prev_depth_structure.size())
                ? side.prev_depth_structure[i]
                : current;   // new level → no artificial jump

        double delta = current - previous;

        double activity = std::abs(delta);
        double alpha = activity / (1.0 + activity);

        side.depth_structure_ema[i] =
            (1.0 - alpha) * side.depth_structure_ema[i]
          + alpha * current;
    }
}



 void update_depth_structural_delta_ema(SideBookState& side)
{
    const size_t n = side.depth_structural_delta.size();
   const size_t ema_N= side.depth_structural_delta_ema.size();
if(n>ema_N){ 
side.depth_structural_delta_ema.resize(n, 0.0);
}

    for (size_t i = 0; i < n; ++i) {
        double current = side.depth_structural_delta[i];

        double previous =
            (i < side.prev_depth_structural_delta.size())
                ? side.prev_depth_structural_delta[i]
                : current;

        double delta_change = current - previous;

        // activity = how unstable the shape is
        double activity = std::abs(delta_change);
        double alpha = activity / (1.0 + activity);

        side.depth_structural_delta_ema[i] =
            (1.0 - alpha) * side.depth_structural_delta_ema[i]
          + alpha * current;
    }
}




template<typename Compare>
 void update_depth_structural_delta(
    SideBookState& side,
    const std::map<double, double, Compare>& price_size_map,
    double tick_size
) {

snapshot_previous_depth_geometry(side);
    side.depth_structural_delta.clear();
side.depth_structure.clear();
    if (price_size_map.size() < 2)
        return;

    std::vector<double> prices;
    std::vector<double> sizes;

    for (const auto& [price, size] : price_size_map) {
        prices.push_back(price);
        sizes.push_back(size);
    }

    const double best_price = prices[0];
    const double best_size  = sizes[0];

    std::vector<double> L;
    L.reserve(prices.size());

    // ---- L0 (best level anchor) ----
    double D0 = best_size;          // distance = 1 tick
    L.push_back(std::log1p(D0));
side.depth_structure.push_back(std::log1p(D0));

    // ---- Li for i >= 1 ----
    for (size_t i = 1; i < prices.size(); ++i) {
        double tick_distance =
            std::abs(prices[i] - best_price) / tick_size;

        tick_distance = std::max(tick_distance, 1.0);

        double D_abs =
            sizes[i] / (tick_distance);

        L.push_back(std::log1p(D_abs));
side.depth_structure.push_back(std::log1p(D_abs));
    }

    // ---- Δi = Li − Li−1 (includes best→L1) ----
    for (size_t i = 1; i < L.size(); ++i) {
        side.depth_structural_delta.push_back(
            L[i] - L[i - 1]
        );
    }

update_depth_structure_ema(side);
update_depth_structural_delta_ema(side);
}
