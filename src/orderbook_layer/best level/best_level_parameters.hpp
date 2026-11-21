#pragma once
#include "best_level_context.hpp"

void update_best_level_unresolved_fragility(
    BestLevelContext& ctx,
     double old_size,
    double traded_qty_at_best,
    double size_post_hit,
     double new_size,

    double cancelled_size_at_best,

    double refreshed_size_at_best,
    double size_pre_refresh

    
);


void update_best_level_behavioral_pressure_rate_intensity(
    BestLevelContext& ctx
);

 void update_hit_rate_ema(
    BestLevelContext& ctx,
    double hit_qty,        // total hit quantity in this delta (BTC)
    double delta_time      // Δt in seconds
);

 void update_hit_intensity_ema(
    BestLevelContext& ctx,
    double hit_qty        // total hit quantity in this delta (BTC)
);



 double compute_intensity(double qty, double size_unit);

 double compute_magnitude(double qty, double size_unit) ;


 void update_cancel_rate_ema(
    BestLevelContext& ctx,
    double cancel_qty,
    double delta_time
) ;

 void update_refresh_rate_ema(
    BestLevelContext& ctx,
    double refresh_qty,
    double delta_time
);


 void update_cancel_intensity_ema(
    BestLevelContext& ctx,
    double cancel_qty
);


 void update_refresh_intensity_ema(
    BestLevelContext& ctx,
    double refresh_qty
);


 void update_willingness_ratio(
    BestLevelContext& ctx
);


 void update_commitment_balance(
    BestLevelContext& ctx
);


 void update_cancel_intensity_acceleration(
    BestLevelContext& ctx,
    double prev_cancel_intensity_ema
);

 void update_refresh_intensity_acceleration(
    BestLevelContext& ctx,
    double prev_refresh_intensity_ema
);

 void update_cancel_rate_acceleration(
    BestLevelContext& ctx,
    double prev_cancel_rate_ema
);

 void update_refresh_rate_acceleration(
    BestLevelContext& ctx,
    double prev_refresh_rate_ema
);

void update_refresh_stress_at_best(
BestLevelContext& ctx,
double refresh_qty
);

void update_refill_metrics(
    BestLevelContext& best,

    // ----- inputs -----
    double refill_latency_raw,        // Li (ms)
    double expected_refill_size,
    double total_refill_size,
    double hit_intensity_ema,
    double median_refill_latency,
    double median_hit_intensity
);

void update_survival_strain_accumulator(
    BestLevelContext& ctx,
    double dt ,  // seconds since last update
    int swept_multiplier
);

void finalize_survival_stress(
    BestLevelContext& ctx,
     double survival_time_at_best,
    bool is_front_run
);

void update_cancel_stress_at_best(
BestLevelContext& ctx,
double cancel_qty
);

double calculate_deterioration_value(double delta_difference, double old_value);

void update_deterioration_strike(
BestLevelContext& ctx
);
