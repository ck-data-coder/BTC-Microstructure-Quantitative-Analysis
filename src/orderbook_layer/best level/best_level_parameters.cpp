#include "best_level_parameters.hpp"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cmath>


double SIZE_UNIT=1;
   constexpr double EMA_ALPHA=0.6;
   constexpr double DECAY=0.85;
constexpr double TIME_CONST=0.02;
constexpr double SIZE_INTENSITY_UNIT=0.5;

double TAU_PRESSURE= 0.1;
double TAU_FATIGUE=1;
double GAMMA_FAIL=1;

void update_best_level_unresolved_fragility(
    BestLevelContext& ctx,
     double old_size,
    double traded_qty_at_best,
    double size_post_hit,
     double new_size,

    double cancelled_size_at_best,

    double refreshed_size_at_best,
    double size_pre_refresh

    
) {
    
double cancel_intensity=0;
double refresh_intensity=0;
double traded_intensity_at_best=0;

    double& F = ctx.best_level_unresolved_fragility;

double& fragility_ema=ctx.best_level_fragility_delta_ema;

double previous_fragility= F;

ctx.best_level_old_unresolved_fragility=F;

    // --------------------------------------------------
    // 1. HIT DAMAGE (forced depletion)
    // --------------------------------------------------
    if (traded_qty_at_best > 0.0) {
if(size_post_hit<0){
if(old_size>0)
F+= std::log1p(old_size / SIZE_UNIT) ;

F+=std::log1p((std::abs(size_post_hit)/ SIZE_UNIT) *  (1.0 / (1+new_size)));
}
else{
        double hit_damage =
 std::log1p((traded_qty_at_best / SIZE_UNIT) *  (1.0 / (1+size_post_hit)));

        F += hit_damage;
}
traded_intensity_at_best=compute_intensity(traded_qty_at_best, SIZE_UNIT);
    }

    // --------------------------------------------------
    // 2. CANCELLATION DAMAGE (voluntary withdrawal)
    // --------------------------------------------------
    if (cancelled_size_at_best > 0.0) {
        double cancel_damage = std::log1p((cancelled_size_at_best / SIZE_UNIT) *  (1.0 / (1+new_size)));

        F += cancel_damage;
      cancel_intensity=compute_intensity(cancelled_size_at_best, SIZE_UNIT);
    }

    // --------------------------------------------------
    // 3. REFRESH / REFILL RELIEF (same price defense)
    // --------------------------------------------------
    if (refreshed_size_at_best > 0.0 ) {
  if (size_pre_refresh<0.0) {
     
        F-=std::log1p((std::abs(size_pre_refresh)+new_size) / SIZE_UNIT);

}
else{
        double refresh_relief =
     std::log1p((refreshed_size_at_best / SIZE_UNIT) * (1.0 / (1+size_pre_refresh)));

        F -= refresh_relief;
}
      refresh_intensity=compute_intensity(refreshed_size_at_best,SIZE_UNIT);
    }

    // --------------------------------------------------
 
double fragility_intensity=traded_intensity_at_best+ cancel_intensity
-refresh_intensity;

double fragility_intensity_magnitude=std::fabs(fragility_intensity);
double alpha=(fragility_intensity_magnitude)/(1+fragility_intensity_magnitude);
  
alpha = std::clamp(alpha, 0.0, 1.0);

double delta_fragility= F- previous_fragility;
fragility_ema=(1-alpha)*fragility_ema+alpha*delta_fragility;
}


void update_best_level_behavioral_pressure_rate_intensity(BestLevelContext& ctx)
{
    double hit_rate     = ctx.hit_rate_ema;
    double cancel_rate  = ctx.cancel_rate_ema;
    double refresh_rate = ctx.refresh_rate_ema;

   double hit_intensity=ctx.hit_intensity_ema;

double cancel_intensity=ctx.cancel_intensity_ema;

  double refresh_intensity=ctx.refresh_intensity_ema;

    double F = ctx.best_level_unresolved_fragility;

 

 double net_rate_pressure=((1+hit_rate)*(1+cancel_rate))/(1+refresh_rate);

   double net_intensity_pressure=((1+hit_intensity)*(1+cancel_intensity))/(1+refresh_intensity);

double rate_deviation = std::abs(std::log(net_rate_pressure));

double intensity_deviation = std::abs(std::log(net_intensity_pressure));

    double rate_activity_factor =
        1.0 - std::exp(-rate_deviation/TAU_PRESSURE);

    double intensity_activity_factor=
       1.0 - std::exp(-intensity_deviation/TAU_PRESSURE);
   

     double delta_F= F- ctx.best_level_old_unresolved_fragility;
 ctx.best_level_behavioral_pressure_rate =
       F*rate_activity_factor + (net_rate_pressure>=1 ? +1 : -1 ) *( rate_deviation * std::abs(ctx.best_level_fragility_delta_ema));

    ctx.best_level_inst_behavioral_pressure_rate=
     
       F*rate_activity_factor + (net_rate_pressure>=1 ? +1 : -1 ) *( rate_deviation * std::abs(delta_F));

ctx.best_level_behavioral_pressure_intensity =
         
        F*intensity_activity_factor + (net_intensity_pressure>=1 ? +1 : -1 ) *( intensity_deviation * std::abs(ctx.best_level_fragility_delta_ema));

   ctx.best_level_inst_behavioral_pressure_intensity=
                F*intensity_activity_factor + (net_intensity_pressure>=1 ? +1 : -1 ) *( intensity_deviation * std::abs(delta_F));

}









 void update_hit_rate_ema(
    BestLevelContext& ctx,
    double hit_qty,        // total hit quantity in this delta (BTC)
    double delta_time      // Δt in seconds
) {
    if (delta_time <= 0.0)
        return;



    // ---- silence: no hit ----
    if (hit_qty <= 0.0) {
        ctx.hit_rate_ema *= DECAY;
        return;
    }

    // ---- event: inject ----
    double hit_rate_inst =
       (1.0+ std::log1p(hit_qty / SIZE_UNIT))
        / (1.0+(1.0-std::exp(-delta_time/TIME_CONST)));

   

    ctx.hit_rate_ema =
        (1.0 - EMA_ALPHA) * ctx.hit_rate_ema
        + EMA_ALPHA * hit_rate_inst;
}



 void update_hit_intensity_ema(
    BestLevelContext& ctx,
    double hit_qty        // total hit quantity in this delta (BTC)
) {
       if (hit_qty <= 0.0) {
        ctx.hit_intensity_ema *= DECAY;
        return;
    }

    // ---- magnitude: what we learn ----
    double magnitude =
        std::log1p(hit_qty / SIZE_UNIT);

double intensity =
        compute_intensity(hit_qty, SIZE_INTENSITY_UNIT);

    // ---- learning weight: how strongly we learn ----
//    double w =
 //       1.0 - std::exp(-hit_intensity);

    // ---- EMA update ----
    ctx.hit_intensity_ema =
        (1.0 - intensity) * ctx.hit_intensity_ema
        + intensity * magnitude;
}





 double compute_intensity(double qty, double size_unit) {
    if (qty <= 0.0)
        return 0.0;
    return 1.0 - std::exp(-qty / size_unit);
}

 double compute_magnitude(double qty, double size_unit) {
    if (qty <= 0.0)
        return 0.0;
    return std::log1p(qty / size_unit);
}


 void update_cancel_rate_ema(
    BestLevelContext& ctx,
    double cancel_qty,
    double delta_time
) {
    if (delta_time <= 0.0)
        return;

  

    if (cancel_qty <= 0.0) {
        // ---- silence: pure decay ----
        ctx.cancel_rate_ema *=DECAY;
        return;
    }

    // ---- event: EMA injection ----
    
    double cancel_rate_inst =
        (1.0+std::log1p(cancel_qty / SIZE_UNIT))
        /(1.0+(1.0-std::exp(-delta_time/TIME_CONST)));

    

    ctx.cancel_rate_ema =
        (1.0 - EMA_ALPHA) * ctx.cancel_rate_ema
        + EMA_ALPHA * cancel_rate_inst;
}


 void update_refresh_rate_ema(
    BestLevelContext& ctx,
    double refresh_qty,
    double delta_time
) {
    if (delta_time <= 0.0)
        return;


    if (refresh_qty <= 0.0) {
        // ---- silence: pure decay ----
        ctx.refresh_rate_ema *=DECAY;
        return;
    }

    // ---- event: EMA injection ----

    double refresh_rate_inst =
        (1.0+std::log1p(refresh_qty / SIZE_UNIT))
        / (1.0+(1.0-std::exp(-delta_time/TIME_CONST)));

    

    ctx.refresh_rate_ema =
        (1.0 - EMA_ALPHA) * ctx.refresh_rate_ema
        + EMA_ALPHA * refresh_rate_inst;
}




 void update_cancel_intensity_ema(
    BestLevelContext& ctx,
    double cancel_qty
) {
    if (cancel_qty <= 0.0){
        ctx.cancel_intensity_ema*=DECAY;
        return;
    }
    double intensity =
        compute_intensity(cancel_qty, SIZE_INTENSITY_UNIT);

    double magnitude =
        compute_magnitude(cancel_qty, SIZE_UNIT);

    ctx.cancel_intensity_ema =
        (1.0 - intensity) * ctx.cancel_intensity_ema
        + intensity * magnitude;
}



 void update_refresh_intensity_ema(
    BestLevelContext& ctx,
    double refresh_qty
) {
    if (refresh_qty <= 0.0){
       ctx.refresh_intensity_ema*=DECAY;
        return;
    }
    double intensity =
        compute_intensity(refresh_qty, SIZE_INTENSITY_UNIT);

    double magnitude =
        compute_magnitude(refresh_qty, SIZE_UNIT);

    ctx.refresh_intensity_ema =
        (1.0 - intensity) * ctx.refresh_intensity_ema
        + intensity * magnitude;
}





 void update_willingness_ratio(
    BestLevelContext& ctx
) {
ctx.old_willingness_ratio=ctx.willingness_ratio;
    // ---- numerical floor ----
    

    ctx.willingness_ratio =
       (1+ ctx.refresh_rate_ema) /
        (1 + ctx.cancel_rate_ema);
}



 void update_commitment_balance(
    BestLevelContext& ctx
) {
ctx.old_commitment_balance=ctx.commitment_balance;

    

    ctx.commitment_balance =
       (1+ ctx.refresh_intensity_ema )/
        (1+ ctx.cancel_intensity_ema);
}


 void update_cancel_intensity_acceleration(
    BestLevelContext& ctx,
    double prev_cancel_intensity_ema
) {
    ctx.cancel_intensity_acceleration =
        ctx.cancel_intensity_ema
        - prev_cancel_intensity_ema;
}



 void update_refresh_intensity_acceleration(
    BestLevelContext& ctx,
    double prev_refresh_intensity_ema
) {
    ctx.refresh_intensity_acceleration =
        ctx.refresh_intensity_ema
        - prev_refresh_intensity_ema;
}





 void update_cancel_rate_acceleration(
    BestLevelContext& ctx,
    double prev_cancel_rate_ema
) {
    ctx.cancel_rate_acceleration =
        ctx.cancel_rate_ema - prev_cancel_rate_ema;
}




 void update_refresh_rate_acceleration(
    BestLevelContext& ctx,
    double prev_refresh_rate_ema
) {
    ctx.refresh_rate_acceleration =
        ctx.refresh_rate_ema - prev_refresh_rate_ema;
}






void update_refresh_stress_at_best(
BestLevelContext& ctx,
double refresh_qty
)
{
ctx.old_refresh_rate_stress=ctx.refresh_rate_stress;

ctx.old_refresh_intensity_stress=ctx.refresh_intensity_stress;

   double refresh_intensity=compute_intensity(refresh_qty,SIZE_INTENSITY_UNIT);

  double refresh_strength=std::log1p(refresh_qty*(1+refresh_intensity));



double pressure_rate=ctx.best_level_inst_behavioral_pressure_rate;

double 
pressure_intensity=ctx.best_level_inst_behavioral_pressure_intensity;

double 
  delta_F= ctx.best_level_unresolved_fragility-
ctx.best_level_old_unresolved_fragility;

double pressure_strength=((1+ctx.cancel_rate_ema)*(1+ctx.hit_rate_ema))/(1+ctx.refresh_rate_ema);

double intensity_strength=((1+ctx.cancel_intensity_ema)*(1+ctx.hit_intensity_ema))/(1+ctx.refresh_intensity_ema);

if(pressure_rate<0){

      double pressure_power= (std::abs(pressure_rate)-((1+refresh_strength)*delta_F));



    ctx.refresh_rate_stress =
        
         (pressure_strength)*
       
//          (ctx.refresh_rate_acceleration <=0?
// (1+std::log1p(std::abs(ctx.refresh_rate_acceleration))):
// (1-std::exp(-1/(1.0+ctx.refresh_rate_acceleration))))*

      (pressure_power>0 ?
      (1/(1+pressure_power)):
       std::max(1.0, std::abs(pressure_power)));
      
    }
else{

double pressure_power= (std::abs(pressure_rate)+((1+refresh_strength)*delta_F));

     ctx.refresh_rate_stress =
        
         (pressure_strength)*
       
//          (ctx.refresh_rate_acceleration <=0?
// (1+std::log1p(std::abs(ctx.refresh_rate_acceleration))):
// (1-std::exp(-1/(1.0+ctx.refresh_rate_acceleration))))*

        ( pressure_power>0?

            (pressure_power):
           (1/(1+std::abs(pressure_power)))
       );
}

if (pressure_intensity<0){

double pressure_power= (std::abs(pressure_intensity)-((1+refresh_strength)*delta_F));

ctx.refresh_intensity_stress=
   
   (intensity_strength)*
       
//          (ctx.refresh_intensity_acceleration <=0?
// (1+std::log1p(std::abs(ctx.refresh_intensity_acceleration))):
// (1-std::exp(-1/(1.0+ctx.refresh_intensity_acceleration))))*

            (pressure_power>0 ?
      (1/(1+pressure_power)):
       std::max(1.0, std::abs(pressure_power)));
}
       
else{

double pressure_power= (std::abs(pressure_intensity)+((1+refresh_strength)*delta_F));

    ctx.refresh_intensity_stress =
        
         (intensity_strength)*
       
//          (ctx.refresh_intensity_acceleration <=0?
// (1+std::log1p(std::abs(ctx.refresh_intensity_acceleration))):
// (1-std::exp(-1/(1.0+ctx.refresh_intensity_acceleration))))*

        ( pressure_power>0?

            (pressure_power):
           (1/(1+std::abs(pressure_power)))
       );
}
}




void update_refill_metrics(
    BestLevelContext& best,

    // ----- inputs -----
    double refill_latency_raw,        // Li (ms)
    double expected_refill_size,
    double total_refill_size,
    double hit_intensity_ema,
    double median_refill_latency,
    double median_hit_intensity
) {


    // ----------------------------------------------------
    // 1) Refill latency transform (bounded, stable)
    // ----------------------------------------------------
    double refill_latency =
        std::log1p(refill_latency_raw / TAU_FATIGUE);

 //double median_latency=std::log1p(median_refill_latency / TAU_FATIGUE);

    // ----------------------------------------------------
    // 2) EMA update (event-based ONLY)
    // ----------------------------------------------------
    double prev_ema = best.refill_latency_ema;

    best.refill_latency_ema =
        (1.0 - EMA_ALPHA) * best.refill_latency_ema +
        EMA_ALPHA * refill_latency;

    // ----------------------------------------------------
    // 3) Slope (fatigue direction)
    // ----------------------------------------------------
    best.refill_latency_slope =
        best.refill_latency_ema - prev_ema;

    // ----------------------------------------------------
    // 4) Remaining refill size (failure component)
    // ----------------------------------------------------
    if(total_refill_size>expected_refill_size)
    total_refill_size=expected_refill_size;

    double remaining_refill_size =
        (expected_refill_size - total_refill_size) /
        std::max(expected_refill_size ,0.001);

    

    // ----------------------------------------------------
    // 5) Latency stress (relative to regime)
    // ----------------------------------------------------
  //  double latency_ratio =
  //     (1+ refill_latency )/
  //      (1+median_latency );

    // ----------------------------------------------------
    // 6) Pressure amplifier A (external cause)
    // ----------------------------------------------------
    double A =
        std::log1p(
        ( hit_intensity_ema)
        );

    // ----------------------------------------------------
    // 7) Final refill stress
    // ----------------------------------------------------
best.old_refill_latency_stress=best.refill_latency_stress;

    best.refill_latency_stress =
        (1+ refill_latency) *
        (1.0 + GAMMA_FAIL * remaining_refill_size) *
       (1+ A);
}


void update_survival_strain_accumulator(
    BestLevelContext& ctx,
    double dt ,  // seconds since last update
    int swept_multiplier
) {
    // --- rate-based pressures ---
    double hit_rate     = ctx.hit_rate_ema;
    double cancel_rate  = ctx.cancel_rate_ema;
    double refresh_rate = ctx.refresh_rate_ema;

    // Net losing pressure (only count when defense is losing)
    double imbalance =
    (hit_rate + cancel_rate - refresh_rate);

    // Total activity (how hard the level is working)
    double activity =
      ( hit_rate + cancel_rate + refresh_rate);

    // Accumulate strain only when imbalance is positive
    if (imbalance > 0.0) {
        ctx.survival_strain_accumulator +=
             (1+std::log1p(  swept_multiplier)*  (imbalance * dt));
    }

    // Always accumulate activity (work done)
    ctx.survival_activity_accumulator +=
        (1+std::log1p(  swept_multiplier)*  ( activity * dt));

    
}



void finalize_survival_stress(
    BestLevelContext& ctx,
     double survival_time_at_best,
    bool is_front_run
) {
    ctx.old_raw_survival_stress=ctx.raw_survival_stress;
        constexpr double TAU_SURVIVAL = 1.0; // seconds, tune later
        constexpr double EPS = 1e-9;

    constexpr double MIN_SURVIVAL_TIME = 0.02; // one tick

    double effective_time =
        std::max(survival_time_at_best, MIN_SURVIVAL_TIME);

    double time_factor =
        1.0 / (1.0 + std::log1p(effective_time / TAU_SURVIVAL));

        // Activity modulation (fighting hard matters)
        double activity_factor =
            std::log1p(ctx.survival_activity_accumulator + EPS);

        ctx.raw_survival_stress =
        (1+ctx.survival_strain_accumulator)
        * (1+activity_factor)
        *(1 + time_factor);
        
     if(is_front_run){
ctx.raw_survival_stress=ctx.raw_survival_stress/(1+(1-std::exp(-(time_factor/((1+ctx.survival_strain_accumulator)*(1+activity_factor))))));
}

    ctx.survival_stress =
        std::log1p(ctx.raw_survival_stress);
    

    // Reset episode state
    ctx.survival_strain_accumulator = 0.0;
    ctx.survival_activity_accumulator = 0.0;
   
}

void update_cancel_stress_at_best(
BestLevelContext& ctx,
double cancel_qty
)
{
ctx.old_cancel_rate_stress=ctx.cancel_rate_stress;

ctx.old_cancel_intensity_stress=ctx.cancel_intensity_stress;

 
    double cancel_intensity=compute_intensity(cancel_qty, SIZE_INTENSITY_UNIT);
 
double 
  delta_F= ctx.best_level_unresolved_fragility-
ctx.best_level_old_unresolved_fragility;

   double cancel_strength=std::log1p(cancel_qty*(1+cancel_intensity));

double pressure_strength=((1+ctx.cancel_rate_ema)*(1+ctx.hit_rate_ema))/(1+ctx.refresh_rate_ema);

double intensity_strength=((1+ctx.cancel_intensity_ema)*(1+ctx.hit_intensity_ema))/(1+ctx.refresh_intensity_ema);

double pressure_rate=ctx.best_level_inst_behavioral_pressure_rate;

double 
pressure_intensity=ctx.best_level_inst_behavioral_pressure_intensity;


if(pressure_rate<0){

    double pressure_power=(std::abs(pressure_rate)- (1+cancel_strength)*delta_F);

    ctx.cancel_rate_stress =
     ((1 +ctx.hit_rate_ema)*
     (1+ctx.cancel_rate_ema))/(1+ctx.refresh_rate_ema)*

// (ctx.cancel_rate_acceleration >0?
// (1+ctx.cancel_rate_acceleration):
// (ctx.cancel_rate_acceleration!=0?
// (1-std::exp(-1/(1.0+std::abs(ctx.cancel_rate_acceleration)))): 1)*

(pressure_power>=0 ?
  (1/(1+pressure_power)):
std::max(1.0,std::abs(pressure_power))
);

}
else{

double pressure_power=(std::abs(pressure_rate)+ (1+cancel_strength)*delta_F);

    ctx.cancel_rate_stress =
     ((1 +ctx.hit_rate_ema)*
     (1+ctx.cancel_rate_ema))/(1+ctx.refresh_rate_ema)*

// (ctx.cancel_rate_acceleration >0?
// (1+ctx.cancel_rate_acceleration):
// (ctx.cancel_rate_acceleration!=0?
// (1-std::exp(1/ctx.cancel_rate_acceleration)): 1)*

     ( pressure_power>0?

            (pressure_power):
           (1/(1+std::abs(pressure_power)))
       );

}

if(pressure_intensity<0){

double pressure_power=(std::abs(pressure_intensity)- (1+cancel_strength)*delta_F);

ctx.cancel_intensity_stress =
     ( (1 +ctx.hit_intensity_ema)*
     (1+ctx.cancel_intensity_ema))/(1+ctx.refresh_intensity_ema)*

// (ctx.cancel_intensity_acceleration >0?
// (1+ctx.cancel_intensity_acceleration):
// (ctx.cancel_intensity_acceleration!=0?
// (1-std::exp(1/+ctx.cancel_intensity_acceleration)):1)*

(pressure_power>=0 ?
  (1/(1+pressure_power)):
std::max(1.0,std::abs(pressure_power))
);

}
else{

double pressure_power=(std::abs(pressure_intensity)+ (1+cancel_strength)*delta_F);

ctx.cancel_intensity_stress =
     ( (1 +ctx.hit_intensity_ema)*
     (1+ctx.cancel_intensity_ema))/(1+ctx.refresh_intensity_ema)*

// (ctx.cancel_intensity_acceleration >0?
// (1+ctx.cancel_intensity_acceleration):
// (ctx.cancel_intensity_acceleration!=0?
// (1-std::exp(1/+ctx.cancel_intensity_acceleration)):1)*

  ( pressure_power>0?

            (pressure_power):
           (1/(1+std::abs(pressure_power)))
       );
}
}


double calculate_deterioration_value(double delta_difference, double old_value){
constexpr double EPS = 1e-6;

double ratio= std::abs(delta_difference)/(std::max(old_value,EPS));

double gain=std::log1p(std::log1p(ratio));

double deterioration= gain*delta_difference;

return deterioration;


}

void update_deterioration_strike(
BestLevelContext& ctx){
double& deterioration=ctx.deterioration_strike;

double survival_stress_delta= ctx.raw_survival_stress-ctx.old_raw_survival_stress;

double deterioration_value=calculate_deterioration_value(survival_stress_delta, ctx.old_raw_survival_stress);

ctx.deterioration_strike=ctx.deterioration_strike+ deterioration_value;

double refill_latency_stress_delta= ctx.refill_latency_stress - ctx.old_refill_latency_stress;

deterioration_value=calculate_deterioration_value(refill_latency_stress_delta,ctx.old_refill_latency_stress);

ctx.deterioration_strike=ctx.deterioration_strike+ deterioration_value;

double c_rate_k=ctx.cancel_rate_stress;

double c_intensity_k=ctx.cancel_intensity_stress;

double c_rate_k_1= ctx.old_cancel_rate_stress;

double c_intensity_k_1= ctx.old_cancel_intensity_stress;

double cancel_stress_delta= ((1+c_rate_k)*(1+c_intensity_k))-((1+c_rate_k_1)*(1+c_intensity_k_1));

double cancel_stress_denom=(1+c_rate_k_1)*(1+c_intensity_k_1);

deterioration_value=calculate_deterioration_value(cancel_stress_delta,cancel_stress_denom);

ctx.deterioration_strike= ctx.deterioration_strike + deterioration_value;

double r_rate_k= ctx.refresh_rate_stress;

double r_intensity_k=ctx.refresh_intensity_stress;

double r_rate_k_1= ctx.old_refresh_rate_stress;

double r_intensity_k_1= ctx.old_refresh_intensity_stress;

double refresh_stress_delta=((1+r_rate_k)*(1+r_intensity_k))-((1+r_rate_k_1)*(1+r_intensity_k_1));


double refresh_stress_denom=(1+r_rate_k_1)*(1+r_intensity_k_1);

deterioration_value=calculate_deterioration_value(refresh_stress_delta, refresh_stress_denom);

ctx.deterioration_strike= ctx.deterioration_strike + deterioration_value;

double defense_delta = (((1+ctx.willingness_ratio)*(1+ctx.commitment_balance))-((1+ctx.old_willingness_ratio)*(1+ctx.old_commitment_balance)));

double defense_denom=(1+ctx.old_willingness_ratio)*(1+ctx.old_commitment_balance);

deterioration_value=calculate_deterioration_value(defense_delta,defense_denom);

ctx.deterioration_strike= ctx.deterioration_strike - deterioration_value;

}


