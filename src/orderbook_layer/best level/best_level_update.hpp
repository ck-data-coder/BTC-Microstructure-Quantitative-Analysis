#pragma once 

#include <cmath>
#include <cstdint>
#include "median.hpp"
#include "best_level_context.hpp"
#include "best_level_state.hpp"
#include <map>
#include <unordered_map>
#include "trade_aggregate.h"
#include "best_level_parameters.hpp"

double MEDIAN_HIT_INTENSITY_EMA=0;
RollingMedian100 refill_latency_median;
double MEDIAN_REFILL_LATENCY=0;


inline double compute_net_delta_size(
    double trade_qty,
    double old_size,
    double new_size
) {
    // trades consume size first
    double expected_after_trade =
        old_size - trade_qty;

    // net book-driven change
    return new_size - expected_after_trade;
}



double calculate_expected_refill_size(
double pci_fragility,
double trade_qty,
double old_size,
double new_size,
double hit_intensity_ema,
bool is_bid_side
){
double size_factor=old_size-trade_qty;

if(size_factor<0){

size_factor=std::abs(size_factor)+new_size;
}
if(is_bid_side){
if(pci_fragility<0){
       double aggression_factor= 1.0/(1.0+std::log1p((1.0+std::log1p(std::abs(pci_fragility)))*(1.0+std::log1p(hit_intensity_ema))*(1.0+ std::log1p(size_factor))));

        return trade_qty*aggression_factor;
}
else{

        return trade_qty;
}
}
else{
    if(pci_fragility>0){
double aggression_factor= 1.0/(1.0+std::log1p((1.0+std::log1p(std::abs(pci_fragility)))*(1.0+std::log1p(hit_intensity_ema))*(1.0+ std::log1p(size_factor))));

        return trade_qty*aggression_factor;
}
else{
     return trade_qty;
}

}

}


double calculate_effective_refill_latency(
BestLevelState& bst,
double hit_intensity_ema

){

double refill_fraction =
    (1+ bst.total_refill_size )/ (1+bst.expected_refill_size );   // ∈ [0,1]

double failure_severity =
    (1.0 - refill_fraction) *
    (1+std::log1p((1+hit_intensity_ema) / (1 + MEDIAN_HIT_INTENSITY_EMA)));

double effective_refill_latency =
    (1+refill_latency_median.median()) * (1.0 + std::abs(failure_severity));

return effective_refill_latency;
}



void reinitialise_best_level_state(BestLevelState& bst){

bst.price=0.0;
 bst.size=0.0; 
bst.survival_time_at_best=0;

 bst.is_hit=false;
  bst.is_awaiting_refill=false;
   bst.refill_latency_raw=0;      
  bst.expected_refill_size=0;
    bst.total_refill_size=0;
    
}







template<typename Compare>
void update_best_level_state(
BestLevelContext& ctx,
BestLevelState&  bst,
const  std::map<double,double, Compare>& old_side,
const  std::map<double,double, Compare>& new_side,
    const std::map<double,double>& delta_side,
 const    std:: unordered_map<double, TradeAgg>& trade_agg,
double pci_fragility,
    int64_t now,
    int64_t last_ts,
    bool is_bid_side
){

double delta_ts = std::max( (now - last_ts) /1000.0, 0.000001 );
double total_traded_qty = 0.0;
std::vector<std::pair<double,double>> traded_price_not_in_old_side;

for (const auto& [price, agg] : trade_agg) {
    total_traded_qty += agg.qty;
if(old_side.find(price) == old_side.end()){
traded_price_not_in_old_side.emplace_back(price,agg.qty);
}
}

const double previous_hit_intensity_ema=ctx.hit_intensity_ema;

update_hit_rate_ema(
    ctx,
    total_traded_qty,      
    delta_ts    
);
update_hit_intensity_ema(
    ctx,
    total_traded_qty
);

double total_trade_qty=0;
double total_refresh_qty=0;
double total_cancel_qty=0;

double old_book_non_zero_best_price=old_side.begin()->first;




 for (const auto& [price, old_size] : old_side) {

bool in_delta = delta_side.count(price);  
double new_size = in_delta? delta_side.at(price) : old_size;


const TradeAgg* trade_agg_data = nullptr;
if (auto it = trade_agg.find(price); it != trade_agg.end()) {
    trade_agg_data = &it->second;
}

double trade_qty=trade_agg_data != nullptr? trade_agg_data->qty : 0.0;

double delta_size_change= compute_net_delta_size(trade_qty,old_size,new_size);

double refresh_qty= delta_size_change>0.0? delta_size_change : 0.0;

double cancel_qty= delta_size_change<0.0? std::abs(delta_size_change): 0.0;



update_best_level_unresolved_fragility(
 ctx , 
  old_size,
trade_qty ,
old_size-trade_qty , 
new_size , 
cancel_qty ,
 refresh_qty, 
old_size-trade_qty
);

total_trade_qty+=trade_qty;
total_refresh_qty+=refresh_qty; total_cancel_qty+=cancel_qty;



if(new_size==0.0){
continue;
}
else{
old_book_non_zero_best_price=price;
break;
}
}

for (const auto& [price, traded_size] : traded_price_not_in_old_side) {
bool price_found=false;
for (const auto& [delta_price, new_size] : delta_side){

if(price==delta_price){
price_found=true;

update_best_level_unresolved_fragility(
 ctx , 
 0.0,
 traded_size ,
 new_size , 
 0.0, 
 0.0 ,
 0.0, 
 0.0
);
}
}

if(!price_found){
update_best_level_unresolved_fragility(
 ctx , 
 0.0,
 traded_size ,
 0.0 , 
 0.0, 
 0.0 ,
 0.0, 
 0.0
);
}
}

const double cancel_rate_ema=ctx.cancel_rate_ema;
const double cancel_intensity_ema=ctx.cancel_intensity_ema;
const double refresh_rate_ema=ctx.refresh_rate_ema;
const double refresh_intensity_ema=ctx.refresh_intensity_ema;

update_cancel_rate_ema(
    ctx,
    total_cancel_qty,      
    delta_ts    
);
update_cancel_intensity_ema(
    ctx,
    total_cancel_qty
);


update_refresh_rate_ema(
    ctx,
    total_refresh_qty,      
    delta_ts    
);
update_refresh_intensity_ema(
    ctx,
    total_refresh_qty
);
update_willingness_ratio(ctx);
update_commitment_balance(ctx);
update_cancel_intensity_acceleration(ctx, cancel_intensity_ema);
update_cancel_rate_acceleration(ctx, cancel_rate_ema);
update_refresh_rate_acceleration(ctx, refresh_rate_ema);
update_refresh_intensity_acceleration(ctx, refresh_intensity_ema);

update_best_level_behavioral_pressure_rate_intensity(ctx);

update_refresh_stress_at_best(ctx,total_refresh_qty);

update_cancel_stress_at_best(ctx,total_cancel_qty);

ctx.old_raw_survival_stress=ctx.raw_survival_stress;

ctx.old_refill_latency_stress= ctx.refill_latency_stress;



update_survival_strain_accumulator(ctx,delta_ts,0);
int swept_index=1;
for (const auto& [price, old_size] : old_side) {
if(bst.price==0.0){
bst.price=price;
bst.size=old_size;
if(ctx.survival_strain_accumulator ==0.0 && ctx.survival_activity_accumulator==0.0){
update_survival_strain_accumulator(ctx,delta_ts,swept_index);
}
}

bool in_delta = delta_side.count(price);  
double new_size = in_delta? delta_side.at(price) : old_size;


const TradeAgg* trade_agg_data = nullptr;
if (auto it = trade_agg.find(price); it != trade_agg.end()) {
    trade_agg_data = &it->second;
}


if(bst.price==price){

bst.survival_time_at_best+=delta_ts;
if(swept_index>1){
bst.survival_time_at_best=0;
}

if(trade_agg_data != nullptr){

if(bst.is_awaiting_refill){
if(bst.expected_refill_size>bst.total_refill_size){
double expected_refill_latency=calculate_effective_refill_latency(bst, previous_hit_intensity_ema);

update_refill_metrics(
    ctx,
    expected_refill_latency,        
    bst.expected_refill_size,
    bst.total_refill_size,
    previous_hit_intensity_ema,
    MEDIAN_REFILL_LATENCY,
    MEDIAN_HIT_INTENSITY_EMA
);
refill_latency_median.add(expected_refill_latency);
bst.total_refill_size=0.0;
bst.expected_refill_size=calculate_expected_refill_size(
pci_fragility,
trade_agg_data->qty,
old_size,
new_size,
ctx.hit_intensity_ema,
is_bid_side
);

double delta_size_change= compute_net_delta_size(trade_agg_data->qty,old_size,new_size);

if(delta_size_change<0){
bst.expected_refill_size-=delta_size_change;
}
else{
bst.total_refill_size+=delta_size_change;
}


bst.refill_latency_raw= now- trade_agg_data->last_hit_ts;

if(bst.expected_refill_size<=bst.total_refill_size){

update_refill_metrics(
    ctx,
    bst.refill_latency_raw,        
    bst.expected_refill_size,
    bst.total_refill_size,
    ctx.hit_intensity_ema,
    MEDIAN_REFILL_LATENCY,
    MEDIAN_HIT_INTENSITY_EMA
);
refill_latency_median.add(bst.refill_latency_raw);
bst.refill_latency_raw=0;
bst.expected_refill_size=0;
bst.total_refill_size=0;
bst.is_awaiting_refill=false;
}
}
}

else{
if(!bst.is_hit)
bst.is_hit=true;
bst.is_awaiting_refill=true;

bst.expected_refill_size=calculate_expected_refill_size(
pci_fragility,
trade_agg_data->qty,
old_size,
new_size,
ctx.hit_intensity_ema,
is_bid_side
);


double delta_size_change= compute_net_delta_size(trade_agg_data->qty,old_size,new_size);

if(delta_size_change<0){
bst.expected_refill_size-=delta_size_change;
}
else{
bst.total_refill_size+=delta_size_change;
}

bst.refill_latency_raw= now- trade_agg_data->last_hit_ts;

if(bst.expected_refill_size<=bst.total_refill_size){

update_refill_metrics(
    ctx,
    bst.refill_latency_raw,        
    bst.expected_refill_size,
    bst.total_refill_size,
    ctx.hit_intensity_ema,
    MEDIAN_REFILL_LATENCY,
    MEDIAN_HIT_INTENSITY_EMA
);
refill_latency_median.add(bst.refill_latency_raw);
bst.refill_latency_raw=0;
bst.expected_refill_size=0;
bst.total_refill_size=0;
bst.is_awaiting_refill=false;
}
}
}
else{
if(bst.is_awaiting_refill){

double delta_size_change= compute_net_delta_size(0.0,old_size,new_size);

if(delta_size_change<0){
bst.expected_refill_size-=delta_size_change;
}
else{
bst.total_refill_size+=delta_size_change;
}

bst.refill_latency_raw+=(now-last_ts);

if(bst.expected_refill_size<=bst.total_refill_size){

update_refill_metrics(
    ctx,
    bst.refill_latency_raw,        
    bst.expected_refill_size,
    bst.total_refill_size,
    ctx.hit_intensity_ema,
    MEDIAN_REFILL_LATENCY,
    MEDIAN_HIT_INTENSITY_EMA
);
refill_latency_median.add(bst.refill_latency_raw);
bst.refill_latency_raw=0;
bst.expected_refill_size=0;
bst.total_refill_size=0;
bst.is_awaiting_refill=false;
}

}


}




bst.last_update_ts=now;

}

if(new_size==0.0){
finalize_survival_stress(
ctx, 
bst.survival_time_at_best, 
false
);

if(bst.is_awaiting_refill){

double expected_refill_latency=calculate_effective_refill_latency(bst, ctx.hit_intensity_ema);

update_refill_metrics(
    ctx,
    expected_refill_latency,        
    bst.expected_refill_size,
    bst.total_refill_size,
    ctx.hit_intensity_ema,
    MEDIAN_REFILL_LATENCY,
    MEDIAN_HIT_INTENSITY_EMA
);
refill_latency_median.add(expected_refill_latency);
}


reinitialise_best_level_state(bst);
swept_index++;
continue;
}
else{
bst.size=new_size;
break;
}
}

std::vector<std::pair<double,double>> front_run_prices;

for (const auto& [delta_price, new_size] : delta_side){
if(is_bid_side){
if(delta_price>old_book_non_zero_best_price && new_size>0.0){

front_run_prices.emplace_back(delta_price,new_size);

}
}
else{
if(delta_price<old_book_non_zero_best_price && new_size>0.0){

front_run_prices.emplace_back(delta_price,new_size);

}

}

}

if(is_bid_side){
std::sort(front_run_prices.begin(), front_run_prices.end(), 
    [](const auto& a, const auto& b) {
        return a.first < b.first; 
    });


}
else{

std::sort(front_run_prices.begin(), front_run_prices.end(), 
    [](const auto& a, const auto& b) {
        return a.first > b.first; // Note the '>' for descending
    });

}
size_t length = front_run_prices.size();
if(length>0){
if(bst.price==old_book_non_zero_best_price){

finalize_survival_stress(
ctx, 
bst.survival_time_at_best, 
true
);

if(bst.is_awaiting_refill){

double expected_refill_latency=calculate_effective_refill_latency(bst, ctx.hit_intensity_ema);

update_refill_metrics(
    ctx,
    expected_refill_latency,        
    bst.expected_refill_size,
    bst.total_refill_size,
    ctx.hit_intensity_ema,
    MEDIAN_REFILL_LATENCY,
    MEDIAN_HIT_INTENSITY_EMA
);
refill_latency_median.add(expected_refill_latency);
}

reinitialise_best_level_state(bst);
}

}


for (const auto& pair : front_run_prices) {
    double price = pair.first;
    double size = pair.second; 

    if(bst.price==0.0){
bst.price=price;
bst.size=size;
bst.survival_time_at_best=0;
update_survival_strain_accumulator(ctx,delta_ts,0);
}

const TradeAgg* trade_agg_data = nullptr;
if (auto it = trade_agg.find(price); it != trade_agg.end()) {
    trade_agg_data = &it->second;
}

if(trade_agg_data !=nullptr){

if(!bst.is_hit)
bst.is_hit=true;
if(!bst.is_awaiting_refill){
bst.is_awaiting_refill=true;

bst.expected_refill_size=calculate_expected_refill_size(
pci_fragility,
trade_agg_data->qty,
trade_agg_data->qty+size,
size,
ctx.hit_intensity_ema,
is_bid_side
);
bst.total_refill_size=0;
bst.refill_latency_raw= now- trade_agg_data->last_hit_ts;

}
}


double new_best_price=new_side.begin()->
first;
if(price!=new_best_price){

finalize_survival_stress(
ctx, 
bst.survival_time_at_best, 
true
);


if(bst.is_awaiting_refill){

double expected_refill_latency=calculate_effective_refill_latency(bst, ctx.hit_intensity_ema);

update_refill_metrics(
    ctx,
    expected_refill_latency,        
    bst.expected_refill_size,
    bst.total_refill_size,
    ctx.hit_intensity_ema,
    MEDIAN_REFILL_LATENCY,
    MEDIAN_HIT_INTENSITY_EMA
);
refill_latency_median.add(expected_refill_latency);
}


reinitialise_best_level_state(bst);
}
}

update_deterioration_strike(ctx);
}



