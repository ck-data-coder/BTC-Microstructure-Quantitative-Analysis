
#include "pci_engine.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

constexpr double SIZE_UNIT = 0.01;
constexpr double TICK_SIZE = 0.1;

PCIEngine::PCIEngine(RotatingFile& logger): logger(logger){}

double PCIEngine::publish_fragility(){
    return pci_indicators.fragility_score;
}

void PCIEngine::on_message(const TradeMessage& msg) {
    if (current_seq == -1) {
        current_seq = msg.seq;
        int64_t ts = msg.trades.front().ts;
        double ltp = msg.trades.front().price;
        anchor=ltp;
        start_new_pci(msg.seq, ts, ts, ltp);
        for (const auto& t : msg.trades) update_with_trade(t);
        return;
    }

    if (msg.seq != current_seq) {
        close_and_finalize_pci();
        try_anchor_update(current_pci);

        int64_t prev_end = current_pci.end_ts;
        double prev_ltp = current_pci.final_ltp;
        current_seq = msg.seq;
        start_new_pci(msg.seq, prev_end, msg.trades.front().ts, prev_ltp);
    }

    for (const auto& t : msg.trades) update_with_trade(t);
}

void PCIEngine::start_new_pci(int64_t seq, int64_t start_ts, int64_t first_trade_ts, double prev_ltp) {
    current_pci = PCI{};
    current_pci.seq = seq;
    current_pci.pde_id = current_pde_id;
    current_pci.start_ts = start_ts;
    current_pci.last_trade_ts = first_trade_ts;
     current_pci.previous_pci_last_price=prev_ltp;

current_pci.is_trade_start_from_anchor=   anchor==prev_ltp;

current_pci.is_trade_start_above_anchor= anchor<prev_ltp;

current_pci.is_trade_start_below_anchor= anchor>prev_ltp;



    last_price = prev_ltp;
    has_seen_first_trade = false;
    pci_active = true;
}

void PCIEngine::update_with_trade(const Trade& t) {
    if (t.is_buy) current_pci.buy_vol += t.qty;
    else current_pci.sell_vol += t.qty;

    double w = 1.0 - std::exp(-(t.qty / SIZE_UNIT));
    if (t.is_buy) current_pci.buy_participation += w;
    else current_pci.sell_participation += w;

    double dt;
    if (!has_seen_first_trade) {
          dt =std::log1p(t.ts - current_pci.start_ts);
        has_seen_first_trade = true;
    } else {
        dt = t.ts - current_pci.last_trade_ts;
    }

    if(dt==0){
dt=0.1;
}

    if (dt > 0) {
        if (t.is_buy) current_pci.buy_active_time += dt;
        else current_pci.sell_active_time += dt;

        if (t.price > anchor) current_pci.up_survival += dt;
        else if (t.price < anchor) current_pci.down_survival += dt;
        else current_pci.anchor_survival += dt;
    }

    double dp = t.price - last_price;
    if (dp > 0) current_pci.abs_up_delta += dp;
    else if (dp < 0) current_pci.abs_down_delta += -dp;

    if (t.price > anchor) current_pci.abs_up_effort_from_anchor += std::max(-dp, dp);
    else if (t.price < anchor) current_pci.abs_down_effort_from_anchor += std::max(dp, -dp);

    last_price = t.price;
    current_pci.last_trade_ts = t.ts;
}

void PCIEngine::close_and_finalize_pci() {
    current_pci.end_ts = current_pci.last_trade_ts;
    current_pci.final_ltp = last_price;
    current_pci.effective_delta = std::fabs(current_pci.final_ltp - current_pci.previous_pci_last_price);
    current_pci.isvalid = true;

  if(current_pci.final_ltp==anchor && current_pci.is_trade_start_from_anchor){
    if(current_pci.up_survival==0.0 && current_pci.down_survival==0.0){

if(current_pci.buy_vol>0.0){

current_pci.pure_up_pci= (1/std::log1p((1+current_pci.buy_vol)*(1+ current_pci.buy_participation)))*(1/std::sqrt(1+(current_pci.buy_active_time/1000.0)));
}

if(current_pci.sell_vol>0.0){

current_pci.pure_down_pci= (1/std::log1p((1+current_pci.sell_vol)*(1+ current_pci.sell_participation)))*(1/std::sqrt(1+(current_pci.sell_active_time/1000.0)));

}
}

else if(current_pci.up_survival !=0.0 && current_pci.down_survival==0.0){

if(current_pci.buy_vol>0.0){

current_pci.pure_up_pci=( 1/std::log1p((1+current_pci.buy_vol)*(1+ current_pci.buy_participation)))
*(1/(1+(current_pci.abs_up_delta+current_pci.abs_down_delta)))
*(1/std::sqrt(1+(current_pci.buy_active_time/1000.0)));
}

if(current_pci.sell_vol>0.0){

current_pci.resistance_down_pci = std::log1p((1+current_pci.sell_vol)*(1+ current_pci.sell_participation))*(
(1+current_pci.abs_down_delta)/std::sqrt(1+
(current_pci.sell_active_time/1000.0)));

}

}

else if(current_pci.down_survival !=0.0 && 
    current_pci.up_survival==0.0){


if(current_pci.sell_vol>0.0){

current_pci.pure_down_pci=( 1/std::log1p((1+current_pci.sell_vol)*(1+ current_pci.sell_participation)))*(1/(1+(current_pci.abs_up_delta+current_pci.abs_down_delta)))
*(1/std::sqrt(1+(current_pci.sell_active_time/1000.0)));
}

if(current_pci.buy_vol>0.0){

current_pci.support_up_pci = std::log1p((1+current_pci.buy_vol)*(1+ current_pci.buy_participation))*(
(1+current_pci.abs_up_delta)/std::sqrt(1+(current_pci.buy_active_time/1000.0)));

}
}

else{

if(current_pci.buy_vol>0.0){

current_pci.support_up_pci = std::log1p((1+current_pci.buy_vol)*(1+ current_pci.buy_participation))*(
(1+current_pci.abs_up_delta)/std::sqrt(1+(current_pci.buy_active_time/1000.0)));

if(current_pci.sell_vol>0.0){

current_pci.resistance_down_pci = std::log1p((1+current_pci.sell_vol)*(1+ current_pci.sell_participation))*(
(1+current_pci.abs_down_delta)/std::sqrt(1+(current_pci.sell_active_time/1000.0)));

}


}


}
}


else if( current_pci.final_ltp > anchor && current_pci.is_trade_start_from_anchor  ){

       
 if(current_pci.buy_vol>0.0){

current_pci.pure_up_pci=( 1/std::log1p((1+current_pci.buy_vol)*(1+ current_pci.buy_participation)))*((1+ current_pci.effective_delta)/(1+(current_pci.abs_up_delta+current_pci.abs_down_delta)))
*(1/std::sqrt(1+(current_pci.buy_active_time/1000.0)));
}

if(current_pci.sell_vol>0.0){

current_pci.resistance_down_pci = std::log1p(((1+current_pci.sell_vol)*(1+ current_pci.sell_participation)) / (1+ current_pci.effective_delta))
*(
(1+current_pci.abs_down_delta)/std::sqrt(1+(current_pci.sell_active_time/1000.0)));

}
}


else if( current_pci.final_ltp < anchor && current_pci.is_trade_start_from_anchor  ){


if(current_pci.sell_vol>0.0){

current_pci.pure_down_pci=( 1/std::log1p((1+current_pci.sell_vol)*(1+ current_pci.sell_participation)))*((1+current_pci.effective_delta)/(1+(current_pci.abs_up_delta+current_pci.abs_down_delta)))
*(1/std::sqrt(1+(current_pci.sell_active_time/1000.0)));
}

if(current_pci.buy_vol>0.0){

current_pci.support_up_pci = std::log1p(((1+current_pci.buy_vol)*(1+ current_pci.buy_participation))/(1+current_pci.effective_delta))*(
(1+current_pci.abs_up_delta)/std::sqrt(1+
(current_pci.buy_active_time/1000.0)));

}
}


else if( current_pci.final_ltp >= anchor && current_pci.is_trade_start_above_anchor  ){


if( current_pci.final_ltp> current_pci.previous_pci_last_price || current_pci.effective_delta==0.0){


if(current_pci.buy_vol>0.0){

current_pci.pure_up_pci=( 1/std::log1p((1+current_pci.buy_vol)*(1+ current_pci.buy_participation)))*((1+ current_pci.effective_delta)/(1+(current_pci.abs_up_delta+current_pci.abs_down_delta)))
*(1/std::sqrt(1+(current_pci.buy_active_time/1000.0)));
}

if(current_pci.sell_vol>0.0){

current_pci.resistance_down_pci = std::log1p(((1+current_pci.sell_vol)*(1+ current_pci.sell_participation)) / (1+ current_pci.effective_delta))
*(
(1+current_pci.abs_down_delta)/std::sqrt(1+(current_pci.sell_active_time/1000.0)));

}
}


else if(current_pci.final_ltp<current_pci.previous_pci_last_price){



if(current_pci.buy_vol>0.0){

current_pci.pure_up_pci=( 1/std::log1p((1+current_pci.buy_vol)*(1+ current_pci.buy_participation)))*(1/(1+(current_pci.abs_up_delta+current_pci.abs_down_delta + current_pci.effective_delta)))
*(1/std::sqrt(1+
(current_pci.buy_active_time/1000.0)));
}

if(current_pci.sell_vol>0.0){

current_pci.resistance_down_pci = std::log1p(((1+current_pci.sell_vol)*(1+ current_pci.sell_participation)) *(1+ current_pci.effective_delta))
*(
(1+current_pci.abs_down_delta)/std::sqrt(1+
(current_pci.sell_active_time/1000.0)));

}
}
}


else if( current_pci.final_ltp < anchor && current_pci.is_trade_start_above_anchor  ){


if(current_pci.buy_vol>0.0){

current_pci.support_up_pci = std::log1p(((1+current_pci.buy_vol)*(1+ current_pci.buy_participation))/(1+current_pci.effective_delta))*(
(1+current_pci.abs_up_delta)/std::sqrt(1+
(current_pci.buy_active_time/1000.0)));

}

if(current_pci.sell_vol>0.0){

current_pci.resistance_down_pci = std::log1p(((1+current_pci.sell_vol)*(1+ current_pci.sell_participation)) *(1+ current_pci.effective_delta))
*(
(1+current_pci.abs_down_delta)/std::sqrt(1+
(current_pci.sell_active_time/1000.0)));

}
}

else if( current_pci.final_ltp <= anchor && current_pci.is_trade_start_below_anchor  ){


if( current_pci.final_ltp< current_pci.previous_pci_last_price || current_pci.effective_delta==0.0){


if(current_pci.sell_vol>0.0){

current_pci.pure_down_pci=( 1/std::log1p((1+current_pci.sell_vol)*(1+ current_pci.sell_participation)))*((1+current_pci.effective_delta)/(1+(current_pci.abs_up_delta+current_pci.abs_down_delta)))
*(1/std::sqrt(1+
(current_pci.sell_active_time/1000.0)));
}

if(current_pci.buy_vol>0.0){

current_pci.support_up_pci = std::log1p(((1+current_pci.buy_vol)*(1+ current_pci.buy_participation))/(1+current_pci.effective_delta))*(
(1+current_pci.abs_up_delta)/std::sqrt(1+
(current_pci.buy_active_time/1000.0)));

}
}

else if(current_pci.final_ltp > current_pci.previous_pci_last_price){


if(current_pci.sell_vol>0.0){

current_pci.pure_down_pci=( 1/std::log1p((1+current_pci.sell_vol)*(1+ current_pci.sell_participation)))*(1/(1+(current_pci.abs_up_delta+current_pci.abs_down_delta + current_pci.effective_delta)))
*(1/std::sqrt(1+
(current_pci.sell_active_time/1000.0)));
}

if(current_pci.buy_vol>0.0){

current_pci.support_up_pci = std::log1p(((1+current_pci.buy_vol)*(1+ current_pci.buy_participation))*(1+current_pci.effective_delta))*(
(1+current_pci.abs_up_delta)/std::sqrt(1+
(current_pci.buy_active_time/1000.0)));

}

}
}


else if( current_pci.final_ltp > anchor && current_pci.is_trade_start_below_anchor  ){


if(current_pci.buy_vol>0.0){

current_pci.support_up_pci = std::log1p(((1+current_pci.buy_vol)*(1+ current_pci.buy_participation))*(1+current_pci.effective_delta))*(
(1+current_pci.abs_up_delta)/std::sqrt(1+
(current_pci.buy_active_time/1000.0)));

}
if(current_pci.sell_vol>0.0){

current_pci.resistance_down_pci = std::log1p(((1+current_pci.sell_vol)*(1+ current_pci.sell_participation)) / (1+ current_pci.effective_delta))
*(
(1+current_pci.abs_down_delta)/std::sqrt(1+
(current_pci.sell_active_time/1000.0)));

}

}




    if (current_pci.abs_up_effort_from_anchor > 0)
        current_pci.relative_survival_up =
            (double)(1+current_pci.up_survival) / (1+current_pci.abs_up_effort_from_anchor);

    if (current_pci.abs_down_effort_from_anchor > 0)
        current_pci.relative_survival_down =
            (double) (1+current_pci.down_survival) / (1+current_pci.abs_down_effort_from_anchor);

    if (current_pci.buy_active_time > 0) {
        current_pci.buy_volume_speed = (1+current_pci.buy_vol) / (1+current_pci.buy_active_time);
        current_pci.buy_participation_speed = (1+current_pci.buy_participation) / (1+current_pci.buy_active_time);
    }

    if (current_pci.sell_active_time > 0) {
        current_pci.sell_volume_speed = (1+current_pci.sell_vol) / (1+current_pci.sell_active_time);
        current_pci.sell_participation_speed = (1+current_pci.sell_participation) / (1+current_pci.sell_active_time);
    }

    constexpr double TAU = 2.0;
    double up_den = std::max(current_pci.abs_up_delta, TICK_SIZE);
    double down_den = std::max(current_pci.abs_down_delta, TICK_SIZE);

      double R_up =
      std::log1p(( (1+(current_pci.buy_active_time/1000.0 ))* current_pci.buy_participation)/ (1+up_den));

    double R_down =
       std::log1p(((1+(current_pci.sell_active_time /1000.0))*current_pci.sell_participation)/ (1+down_den));

    current_pci.buy_resistance = 1.0 - std::exp(-R_up / TAU);
    current_pci.sell_resistance = 1.0 - std::exp(-R_down / TAU);

    current_pci.buy_compression = current_pci.buy_vol * current_pci.buy_resistance;
    current_pci.sell_compression = current_pci.sell_vol * current_pci.sell_resistance;

    update_pci_indicators(current_pci);

   // pci_history.push_back(current_pci);
    //   print_pci(out,current_pci);
    logger.rotate_if_needed();
       print_pci_indicators(logger.out,pci_indicators);

}

void PCIEngine::update_market_inventory(const PCI& pci) {
   pci_indicators.sell_inventory += pci.buy_compression;
   pci_indicators.buy_inventory += pci.sell_compression;
}

double PCIEngine::net_inventory() const {
    return pci_indicators.buy_inventory - pci_indicators.sell_inventory;
}

double PCIEngine::compute_fragility() const {
    constexpr double CAP = 1.0;
    double net = net_inventory();
    double mag = std::fabs(net);
    double f =  std::log1p(mag / CAP);
    return net >= 0 ? f : -f;
}

bool PCIEngine::accept_up(const PCI& p) const {
    double comp_ratio = p.buy_compression / (p.buy_compression + p.sell_compression + 1e-9);
    double delta_ratio = p.abs_up_delta / (p.abs_up_delta + p.abs_down_delta + 1e-9);
    return comp_ratio >= 0.65 && delta_ratio >= 0.70;
}

bool PCIEngine::accept_down(const PCI& p) const {
    double comp_ratio = p.sell_compression / (p.buy_compression + p.sell_compression + 1e-9);
    double delta_ratio = p.abs_down_delta / (p.abs_up_delta + p.abs_down_delta + 1e-9);
    return comp_ratio >= 0.65 && delta_ratio >= 0.70;
}

void PCIEngine::try_anchor_update(const PCI& p) {
    if (p.final_ltp > anchor && accept_up(p)) {
        anchor = p.final_ltp;
        current_pde_id++;
    } else if (p.final_ltp < anchor && accept_down(p)) {
        anchor = p.final_ltp;
        current_pde_id++;
    }
}

inline void PCIEngine::update_ema_with_decay(double& ema, double value, double alpha,double reducing_factor){
    if(value==0.0){
     ema *= reducing_factor;
    }
    else{

       ema= (1.0-alpha)*ema + alpha*value;
    }
}

void PCIEngine::update_pci_indicators(const PCI& p){
    
    double pci_time=(double)(p.end_ts-p.start_ts)/1000.0;
    double pci_activity= ((1+p.buy_vol)*(1+p.buy_participation))+((1+p.sell_vol)*(1+p.sell_participation));

    double pci_activity_per_unit_time= pci_activity/(1+pci_time);

    double alpha = 1.0 - std::exp(-(pci_activity_per_unit_time /4.0));
    double reducing_factor=std::exp(-(pci_activity_per_unit_time/16.0));

    pci_indicators.ltp= p.final_ltp;
    pci_indicators.ts_ms=p.end_ts;
    pci_indicators.ist_time=millis_to_ist(p.end_ts);
  update_ema_with_decay(
        pci_indicators.pure_up_pci_ema,
        p.pure_up_pci,
        alpha,
        reducing_factor);

    update_ema_with_decay(
        pci_indicators.pure_down_pci_ema,
        p.pure_down_pci,
        alpha,
        reducing_factor);

    update_ema_with_decay(
        pci_indicators.resistance_down_pci_ema,
        p.resistance_down_pci,
        alpha,
        reducing_factor);

    update_ema_with_decay(
        pci_indicators.support_up_pci_ema,
        p.support_up_pci,
        alpha,
        reducing_factor);

    // ---- Survival ----
    update_ema_with_decay(
        pci_indicators.relative_survival_up_ema,
        p.relative_survival_up,
        alpha,
        reducing_factor);

    update_ema_with_decay(
        pci_indicators.relative_survival_down_ema,
        p.relative_survival_down,
        alpha,
        reducing_factor);

    // ---- Volume speed ----
    update_ema_with_decay(
        pci_indicators.buy_volume_speed_ema,
        p.buy_volume_speed,
        alpha,
        reducing_factor);

    update_ema_with_decay(
        pci_indicators.sell_volume_speed_ema,
        p.sell_volume_speed,
        alpha,
        reducing_factor);

    // ---- Participation speed ----
    update_ema_with_decay(
        pci_indicators.buy_participation_speed_ema,
        p.buy_participation_speed,
        alpha,
        reducing_factor);

    update_ema_with_decay(
        pci_indicators.sell_participation_speed_ema,
        p.sell_participation_speed,
        alpha,
        reducing_factor);


        double buy_inventory=p.sell_compression;
        double sell_inventory=p.buy_compression;
        double buy_inventory_per_unit_time=(1+buy_inventory)/(1+pci_time);
        double sell_inventory_per_unit_time=(1+sell_inventory)/(1+pci_time);

        pci_indicators.local_buy_inventory=buy_inventory;
        pci_indicators.local_sell_inventory=sell_inventory;

         update_ema_with_decay(
        pci_indicators.buy_inventory_acceleration_ema,
        buy_inventory,
        alpha,
        reducing_factor);

         update_ema_with_decay(
        pci_indicators.buy_inventory_acceleration_per_unit_time_ema,
        buy_inventory_per_unit_time,
        alpha,
        reducing_factor);

         update_ema_with_decay(
        pci_indicators.sell_inventory_acceleration_ema,
        sell_inventory,
        alpha,
        reducing_factor);

         update_ema_with_decay(
        pci_indicators.sell_inventory_acceleration_per_unit_time_ema,
        sell_inventory_per_unit_time,
        alpha,
        reducing_factor);

        update_market_inventory(p);
       pci_indicators.fragility_score= compute_fragility();
      

   

}


std::string PCIEngine::millis_to_ist(int64_t millis)
{
    using namespace std::chrono;

    system_clock::time_point tp = system_clock::time_point(milliseconds(millis));

    // IST offset
    tp += hours(5) + minutes(30);

    std::time_t tt = system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&tt);

    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

    return ss.str();
}

  void PCIEngine::print_pci(std::ofstream& out, const PCI& p)
{
    out << "================ PCI ================\n";

    out << "pde_id: " << p.pde_id << "\n";
    out << "seq: " << p.seq << "\n";

    out << "---- timestamps ----\n";
    out << "start_ts: " << p.start_ts << "\n";
    out << "last_trade_ts: " << p.last_trade_ts << "\n";
    out << "end_ts: " << p.end_ts << "\n";

    out << "---- price ----\n";
    out << "final_ltp: " << p.final_ltp << "\n";
    out << "effective_delta: " << p.effective_delta << "\n";

    out << "---- aggression volume ----\n";
    out << "buy_vol: " << p.buy_vol << "\n";
    out << "sell_vol: " << p.sell_vol << "\n";

    out << "---- participation ----\n";
    out << "buy_participation: " << p.buy_participation << "\n";
    out << "sell_participation: " << p.sell_participation << "\n";

    out << "---- active time ----\n";
    out << "buy_active_time: " << p.buy_active_time << "\n";
    out << "sell_active_time: " << p.sell_active_time << "\n";

    out << "---- survival ----\n";
    out << "up_survival: " << p.up_survival << "\n";
    out << "down_survival: " << p.down_survival << "\n";
    out << "anchor_survival: " << p.anchor_survival << "\n";

    out << "---- path deltas ----\n";
    out << "abs_up_delta: " << p.abs_up_delta << "\n";
    out << "abs_down_delta: " << p.abs_down_delta << "\n";

    out << "---- anchor effort ----\n";
    out << "abs_up_effort_from_anchor: " << p.abs_up_effort_from_anchor << "\n";
    out << "abs_down_effort_from_anchor: " << p.abs_down_effort_from_anchor << "\n";

    out << "---- PCI projections ----\n";
    out << "pure_up_pci: " << p.pure_up_pci << "\n";
    out << "resistance_down_pci: " << p.resistance_down_pci << "\n";
    out << "pure_down_pci: " << p.pure_down_pci << "\n";
    out << "support_up_pci: " << p.support_up_pci << "\n";

    out << "---- relative survival ----\n";
    out << "relative_survival_up: " << p.relative_survival_up << "\n";
    out << "relative_survival_down: " << p.relative_survival_down << "\n";

    out << "---- aggression speed ----\n";
    out << "buy_volume_speed: " << p.buy_volume_speed << "\n";
    out << "sell_volume_speed: " << p.sell_volume_speed << "\n";
    out << "buy_participation_speed: " << p.buy_participation_speed << "\n";
    out << "sell_participation_speed: " << p.sell_participation_speed << "\n";

    out << "---- resistance ----\n";
    out << "buy_resistance: " << p.buy_resistance << "\n";
    out << "sell_resistance: " << p.sell_resistance << "\n";

    out << "---- compression ----\n";
    out << "buy_compression: " << p.buy_compression << "\n";
    out << "sell_compression: " << p.sell_compression << "\n";

    out << "isvalid: " << p.isvalid << "\n";

    out << "=====================================\n\n";
}


void PCIEngine::print_pci_indicators(std::ofstream& out, const PCIIndicators& ind)
{
  out << "{";

    out << "\"ts_ms\":" << ind.ts_ms << ",";
    out << "\"ist_time\":\"" << ind.ist_time << "\",";

    out << "\"ltp\":" << ind.ltp << ",";

    out << "\"pure_up_pci_ema\":" << ind.pure_up_pci_ema << ",";
    out << "\"pure_down_pci_ema\":" << ind.pure_down_pci_ema << ",";
    out << "\"resistance_down_pci_ema\":" << ind.resistance_down_pci_ema << ",";
    out << "\"support_up_pci_ema\":" << ind.support_up_pci_ema << ",";

    out << "\"relative_survival_up_ema\":" << ind.relative_survival_up_ema << ",";
    out << "\"relative_survival_down_ema\":" << ind.relative_survival_down_ema << ",";

    out << "\"buy_volume_speed_ema\":" << ind.buy_volume_speed_ema << ",";
    out << "\"sell_volume_speed_ema\":" << ind.sell_volume_speed_ema << ",";

    out << "\"buy_participation_speed_ema\":" << ind.buy_participation_speed_ema << ",";
    out << "\"sell_participation_speed_ema\":" << ind.sell_participation_speed_ema << ",";

    
    out << "\"buy_inventory_acceleration_ema\":" << ind.buy_inventory_acceleration_ema << ",";
    out << "\"sell_inventory_acceleration_ema\":" << ind.sell_inventory_acceleration_ema << ",";
    out << "\"buy_inventory_acceleration_per_unit_time_ema\":" << ind.buy_inventory_acceleration_per_unit_time_ema << ",";
    out << "\"sell_inventory_acceleration_per_unit_time_ema\":" << ind.sell_inventory_acceleration_per_unit_time_ema << ",";
    
    out << "\"buy_inventory\":" << ind.buy_inventory << ",";
    out << "\"sell_inventory\":" << ind.sell_inventory << ",";
    out << "\"local_buy_inventory\":" << ind.local_buy_inventory << ",";
    out << "\"local_sell_inventory\":" << ind.local_sell_inventory << ",";

    out << "\"fragility_score\":" << ind.fragility_score;

    out << "}\n";
}