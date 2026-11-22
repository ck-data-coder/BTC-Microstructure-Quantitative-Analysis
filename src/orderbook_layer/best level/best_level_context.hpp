#pragma once


struct BestLevelContext {
    
     double best_level_unresolved_fragility; 

   double best_level_old_unresolved_fragility;

   double best_level_fragility_delta_ema;

 double best_level_behavioral_pressure_rate;

double best_level_behavioral_pressure_intensity;

double best_level_inst_behavioral_pressure_rate;

double best_level_inst_behavioral_pressure_intensity;

double hit_rate_ema;

double hit_intensity_ema;

double cancel_rate_ema; 

double refresh_rate_ema; 
    
double cancel_intensity_ema;

double refresh_intensity_ema;

double willingness_ratio;

double commitment_balance;

double cancel_rate_acceleration;

double refresh_rate_acceleration;

double cancel_intensity_acceleration;

double refresh_intensity_acceleration;


double cancel_rate_stress;

double cancel_intensity_stress;

double refresh_rate_stress; 

double refresh_intensity_stress;

double refill_latency_ema;

    double refill_latency_slope;

    double refill_latency_stress;

    double survival_stress;

      double raw_survival_stress;

    double survival_strain_accumulator;
    
    double survival_activity_accumulator;

    double deterioration_strike;
    
    //  double size_ema;
    //   double size_variance_ema;

     double  old_raw_survival_stress;
      double old_refill_latency_stress;
     double old_cancel_rate_stress;

double old_cancel_intensity_stress;

double old_refresh_rate_stress; 

double old_refresh_intensity_stress;

double old_willingness_ratio;

double old_commitment_balance;
};


