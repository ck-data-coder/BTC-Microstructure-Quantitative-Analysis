#pragma once
#include <vector>

struct SideBookState {

    bool is_bid;
    int tracked_depth_levels;

    // ---------- Geometric (snapshot) ----------
std::vector<double> depth_cascade_potential;

std::vector<double> depth_cascade_potential_ema;

std::vector<double> prev_depth_cascade_potential;

std::vector<double> visible_resistance;

std::vector<double> visible_resistance_ema;

std::vector<double> prev_visible_resistance;

    // True resistance per level
    std::vector<double> true_resistance;

std::vector<double> true_resistance_ema;

std::vector<double> prev_true_resistance;

std::vector<double> depth_structure;

std::vector<double> depth_structural_delta;


std::vector<double> depth_structure_ema;            // slow sustain of L_i
std::vector<double> depth_structural_delta_ema;     // slow sustain of Δ_i

std::vector<double> prev_depth_structure;            // previous L_i
std::vector<double> prev_depth_structural_delta;     // previous Δ_i
    
};
