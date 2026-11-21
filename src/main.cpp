#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include "orderbook.h"
#include "candle_manager.hpp"
#include "trade_processor.hpp"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <map>

#include "ticker.hpp"
#include "early_arrival_timeframe_buffer.hpp"
#include "ticker_previous_state.hpp"
#include "early_aggregate.hpp"
#include "mongo_client.hpp"
#include "candle_writer.hpp"
#include "orderbook_layer/best level/trade_aggregate.h"
#include "pci_layer/pci_engine.h"
#include "orderbook_layer/orderbook_engine.hpp"
#include "rotating_file.hpp"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace json = boost::json;
using tcp = net::ip::tcp;


std::unordered_map<int64_t, TimeframeBuffer> tf_buffers;
// key = timeframe ms (60_000, 300_000, etc)

//    struct TradeEvent {
//     double price;
//     double qty;
//     bool is_buy;     // Buy = hit ask, Sell = hit bid
//     int64_t ts;
// };


// JSON → BookUpdate
BookUpdate parse_book(const std::string& msg) {
    BookUpdate u;

    json::object obj = json::parse(msg).as_object();
    u.is_snapshot = (obj["type"].as_string() == "snapshot");

    json::object data = obj["data"].as_object();
    u.seq = data["seq"].as_int64();
    u.book_ts=obj["ts"].as_int64();
    if (data.contains("b")) {
        for (auto& v : data["b"].as_array()) {
            auto& a = v.as_array();
            u.bids.emplace_back(
                std::stod(a[0].as_string().c_str()), // price
                std::stod(a[1].as_string().c_str())  // size
            );
        }
    }

    if (data.contains("a")) {
        for (auto& v : data["a"].as_array()) {
            auto& a = v.as_array();
            u.asks.emplace_back(
                std::stod(a[0].as_string().c_str()),
                std::stod(a[1].as_string().c_str())
            );
        }
    }

    return u;
}


// Swept price trackers
std::vector<double> swept_bid_prices;
std::vector<double> swept_ask_prices;


template<typename Compare>
void detect_swept_levels(
    const std::map<double,double, Compare>& old_side,
    const std::unordered_map<double,double>& trade_qty,
    const std::vector<std::pair<double,double>>& delta_side,
    bool is_bid
) {
    for (const auto& [price, new_size] : delta_side) {

        if (new_size != 0)
            continue;

        auto it = old_side.find(price);
        if (it == old_side.end())
            continue;

        double old_size = it->second;
        double traded   = trade_qty.count(price) ? trade_qty.at(price) : 0.0;

        // ⭐ STRICT RULE
        if (traded == old_size) {
            if (is_bid)
                swept_bid_prices.push_back(price);
            else
                swept_ask_prices.push_back(price);
        }
    }
}


template<typename MAP>
MAP prepare_virtual_book(
    const MAP& old_side,
    bool is_bid
) {
    const auto& swept =
        is_bid ? swept_bid_prices : swept_ask_prices;

    if (swept.empty())
        return old_side;   // no shift

    MAP vbook;

    for (const auto& [price, size] : old_side) {
        if (std::find(swept.begin(), swept.end(), price) == swept.end()) {
            vbook[price] = size;
        }
    }

    return vbook;
}


void analyze_cancel_and_refill(
    const OrderBook& old_book,
    const BookUpdate& u,
    const std::vector<TradeEvent>& trade_buffer,
    int64_t last_delta_ts,
    std::vector<CandleManager>& managers
) {
    // ==============================
    // STEP 1: aggregate trades by price
    // ==============================
    std::unordered_map<double, double> bid_trade_qty;
    std::unordered_map<double, double> ask_trade_qty;
    int64_t current_delta_ts=u.book_ts;

    for (const auto& t : trade_buffer) {
        if (t.ts < last_delta_ts || t.ts >= current_delta_ts)
            continue;

        if (t.is_buy)
            ask_trade_qty[t.price] += t.qty; // buy hits ask
        else
            bid_trade_qty[t.price] += t.qty; // sell hits bid
    }


    detect_swept_levels(
   old_book.bids,
    bid_trade_qty,
    u.bids,
    true
);

detect_swept_levels(
    old_book.asks,
    ask_trade_qty,
    u.asks,
    false
);

auto effective_bids = prepare_virtual_book(old_book.bids, true);
auto effective_asks = prepare_virtual_book( old_book.asks, false);


    // ==============================
    // STEP 2: process BID deltas
    // ==============================
    for (const auto& [price, new_size] : u.bids) {

        auto it_old = effective_bids.find(price);
        if (it_old == effective_bids.end())
            continue; // price not in old book → ignore

        double old_size = it_old->second;
        double traded   = bid_trade_qty.count(price)
              ? bid_trade_qty.at(price)
              : 0.0;
        double expected = old_size - traded;

        double refill = 0.0;
        double cancel = 0.0;

        if (new_size > expected)
            refill = new_size - expected;
        else
            cancel = expected - new_size;

        if (refill == 0 && cancel == 0)
            continue;

        // find level index in OLD book
        int level = 0;
        for (auto it = effective_bids.begin(); it != effective_bids.end(); ++it, ++level) {
            if (it->first == price)
                break;
        }

        if (level > 9)
            continue;

        // ==============================
        // STEP 3: update candle managers
        // ==============================
        for (auto& mgr : managers) {
            if (!mgr.in_candle(current_delta_ts)){


                tf_buffers[mgr.tf_ms].cancel_refill.push_back({
                current_delta_ts,
                Side::Bid,
                cancel,
                refill,
                level     
             });

                continue;
            }
            auto& side = mgr.current.bid;

            if (level == 0) {
                side.best.cancel_vol  += cancel;
                side.best.refill_vol  += refill;
                add_to_bucket(side.best.buckets, cancel, false);
                add_to_bucket(side.best.buckets, refill, true);
            }

            if (level <= 4) {
                side.best5.cancel_vol += cancel;
                side.best5.refill_vol += refill;
                add_to_bucket(side.best5.buckets, cancel, false);
                add_to_bucket(side.best5.buckets, refill, true);
            }

            if (level <= 9) {
                side.best10.cancel_vol += cancel;
                side.best10.refill_vol += refill;
                add_to_bucket(side.best10.buckets, cancel, false);
                add_to_bucket(side.best10.buckets, refill, true);
            }
        }
    }

    // ==============================
    // STEP 4: process ASK deltas
    // ==============================
    for (const auto& [price, new_size] : u.asks) {

        auto it_old =effective_asks.find(price);
        if (it_old == effective_asks.end())
            continue;

        double old_size = it_old->second;
        double traded   = ask_trade_qty.count(price)
              ? ask_trade_qty.at(price)
              : 0.0;
        double expected = old_size - traded;

        double refill = 0.0;
        double cancel = 0.0;

        if (new_size > expected)
            refill = new_size - expected;
        else
            cancel = expected - new_size;

        if (refill == 0 && cancel == 0)
            continue;

        int level = 0;
        for (auto it = effective_asks.begin(); it !=effective_asks.end(); ++it, ++level) {
            if (it->first == price)
                break;
        }

        if (level > 9)
            continue;

        for (auto& mgr : managers) {
            if (!mgr.in_candle(current_delta_ts)){

                tf_buffers[mgr.tf_ms].cancel_refill.push_back({
                current_delta_ts,
                Side::Ask,
                cancel,
                refill,
                level     
             });
                continue;
            }
            auto& side = mgr.current.ask;

            if (level == 0) {
                side.best.cancel_vol  += cancel;
                side.best.refill_vol  += refill;
                add_to_bucket(side.best.buckets, cancel, false);
                add_to_bucket(side.best.buckets, refill, true);
            }

            if (level <= 4) {
                side.best5.cancel_vol += cancel;
                side.best5.refill_vol += refill;
                add_to_bucket(side.best5.buckets, cancel, false);
                add_to_bucket(side.best5.buckets, refill, true);
            }

            if (level <= 9) {
                side.best10.cancel_vol += cancel;
                side.best10.refill_vol += refill;
                add_to_bucket(side.best10.buckets, cancel, false);
                add_to_bucket(side.best10.buckets, refill, true);
            }
        }
    }


    swept_bid_prices.clear();
swept_ask_prices.clear();

}


inline int64_t to_int64(const boost::json::value& v){ if (v.is_int64()) return v.as_int64();
 if (v.is_uint64()) return static_cast<int64_t>(v.as_uint64());
 if (v.is_double()) return static_cast<int64_t>(v.as_double());
 if (v.is_string()) return std::stoll(v.as_string().c_str());
 throw std::runtime_error("Cannot convert JSON value to int64");}


int main() {
    try {
        MongoClient mongo("mongodb://localhost:27017");
CandleWriter candle_writer(mongo.client(), "market_data");

        TickerPreviousState previous_state;

        double& prev_last = previous_state.last;
        double& prev_mark = previous_state.mark;
        double&  prev_index = previous_state.index;
        double& prev_oi = previous_state.oi;
        bool has_prev_ticker = false;

      

            // timeframes (ms)
    std::vector<CandleManager> managers = {
        CandleManager(60'000),
        CandleManager(300'000),
        CandleManager(900'000),
        CandleManager(1'800'000),
        CandleManager(3'600'000),
        CandleManager(14'400'000)
    };

 

std::vector<TradeEvent> trade_buffer;
int64_t last_delta_ts = 0;



        std::cout << "PROGRAM STARTED\n";
          std::cout.flush();
//    std::ofstream out("C:/dev/bybit_ws/pci_output.txt",
//                   std::ios::out | std::ios::app);
//    std::ofstream out2("C:/dev/bybit_ws/orderbook_output.txt",
//                   std::ios::out | std::ios::app);

                  RotatingFile perpetual_logger("C:/dev/bybit_ws/perpetual_pci_output.txt",200);
                  RotatingFile spot_logger("C:/dev/bybit_ws/spot_pci_output.txt",200);
                  RotatingFile logger2("C:/dev/bybit_ws/orderbook_output.txt",500);

// if (!out.is_open()) {
//     MessageBoxA(NULL,
//                 "Failed to open output file",
//                 "Error",
//                 MB_OK | MB_ICONERROR);
//     return 1;
// }
// if (!out2.is_open()) {
//     MessageBoxA(NULL,
//                 "Failed to open output file",
//                 "Error",
//                 MB_OK | MB_ICONERROR);
//     return 1;
// }

  PCIEngine perpetual_pci_engine(perpetual_logger);
  PCIEngine spot_pci_engine(spot_logger);
  OrderbookEngine orderbook_engine;

        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};

        ctx.set_default_verify_paths();

        tcp::resolver resolver{ioc};
        websocket::stream<ssl::stream<tcp::socket>> ws{ioc, ctx};
        

        // --- Resolve + TCP connect ---
        auto results = resolver.resolve(
            "stream.bybit.com", "443");

        net::connect(ws.next_layer().next_layer(),
                     results.begin(), results.end());


        // --- SNI (required) ---
        SSL_set_tlsext_host_name(
            ws.next_layer().native_handle(),
            "stream.bybit.com");

     

        // --- TLS handshake ---
        ws.next_layer().handshake(ssl::stream_base::client);
        

        // --- Required headers ---
        ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(beast::http::field::host,
                        "stream.bybit.com");
                req.set(beast::http::field::user_agent,
                        "beast-client");
            }));

 

        // --- WebSocket handshake ---
        ws.handshake(
            "stream.bybit.com",
            "/v5/public/linear"
        );


        std::cout << "WebSocket connected\n";

        // --- Subscribe to orderbook ---
       


        std::string sub = R"({
            "op": "subscribe",
            "args": [
              "tickers.BTCUSDT",
                 "orderbook.50.BTCUSDT",
                 "kline.1.BTCUSDT",
                 "kline.5.BTCUSDT",
                "kline.15.BTCUSDT",
                "kline.30.BTCUSDT",
                "kline.60.BTCUSDT",
                "kline.240.BTCUSDT",
            "publicTrade.BTCUSDT"
               
            ]
        })";

        ws.write(net::buffer(sub));
        
        std::cout << "Subscribed\n";

        OrderBook book;

        beast::flat_buffer buffer;

        while (true) {
            ws.read(buffer);
            
            std::string msg =
                beast::buffers_to_string(buffer.data());
            buffer.consume(buffer.size());

          

            json::object obj = json::parse(msg).as_object();

            if (!obj.contains("topic")) {
                // ping / pong / subscribe ack → ignore
               continue;
            }

           std::string topic = obj["topic"].as_string().c_str();

          //   std::cout<< obj["data"]<<"\n";

         if (topic.find("tickers") != std::string::npos) {
       auto& d = obj["data"].as_object();

double cur_last  = d.contains("lastPrice")? to_double(d["lastPrice"]): prev_last;
double cur_mark  = d.contains("markPrice")? to_double(d["markPrice"]): prev_mark ;
double cur_index = d.contains("indexPrice")? to_double(d["indexPrice"]):prev_index;
double cur_oi    = d.contains("openInterest")? to_double(d["openInterest"]):prev_oi;
int64_t ts       = obj["ts"].as_int64();

if (has_prev_ticker) {
    for (auto& mgr : managers) {
        if (mgr.in_candle(ts)) {
            update_ticker_metrics(
                mgr.current,
                prev_last, prev_mark, prev_index, prev_oi,
                cur_last, cur_mark, cur_index, cur_oi
            );
        }
       else {
              EarlyArrivalTickerEvent ev;
              ev.ts = ts;
                  
                  ev.last = cur_last;
            
                  ev.mark = cur_mark;
              
                  ev.index = cur_index;
            
                  ev.oi = cur_oi;
             

              tf_buffers[mgr.tf_ms].tickers.push_back(ev);

    }
}
}

prev_last = cur_last;
prev_mark = cur_mark;
prev_index = cur_index;
prev_oi = cur_oi;
previous_state.ts=ts;
has_prev_ticker = true;

}

         
           if (topic.find("orderbook") != std::string::npos) {

    BookUpdate u = parse_book(msg);

    if (u.is_snapshot) {
        book.apply_snapshot(u);

        last_delta_ts=u.book_ts;
          // 3️⃣ Clear any stale trades
        trade_buffer.clear();

        // ❌ NO analytics here
        continue;
    }
    else {
        orderbook_engine.process_orderbook_update(
            book,
            u,
            trade_buffer,
            last_delta_ts,
            perpetual_pci_engine.publish_fragility(),
            logger2
        );
            analyze_cancel_and_refill(
        book,              // OLD book
        u,                 // delta changes
        trade_buffer,      // trades since last delta
        last_delta_ts,     // time window start
        managers           // candles (all TFs)
    );
        
        if (!book.apply_delta(u)) {
            perpetual_logger.out << "Sequence broken — resync needed\n";
              std::cout << "Sequence broken — exiting loop\n";
            return -1;
        }
           
        last_delta_ts=u.book_ts;
    // after processing delta
        trade_buffer.erase(
           std::remove_if(
           trade_buffer.begin(),
           trade_buffer.end(),
           [&](const TradeEvent& t) {
            return t.ts < last_delta_ts - 5000; // 2s safety window
     }),
           trade_buffer.end()
);



    }
}



            if (topic.find("kline") != std::string::npos) {
                auto data = obj["data"].as_array();
                auto& k = data[0].as_object();
             
                int64_t start = k["start"].as_int64();
                int64_t end   = k["end"].as_int64();
                bool confirm  = k["confirm"].as_bool();

            

                int64_t tf_ms = (end - start);

                for (auto& mgr : managers) {
                    if (mgr.tf_ms-1 == tf_ms) {

                        if (mgr.current.start != start) {
                         //    std::cout << obj["data"] << "\n";
                            mgr.start_new(start, end);
                            aggregate_all(mgr,tf_buffers,previous_state);
                        }

                        mgr.current.open   = to_double(k["open"]);
                        mgr.current.high   = to_double(k["high"]);
                        mgr.current.low    = to_double(k["low"]);
                        mgr.current.close  = to_double(k["close"]);
                        mgr.current.volume = to_double(k["volume"]);
                        mgr.current.confirmed = confirm;

                        if (confirm) {
                          std::cout << "CONFIRMED CANDLE: " << tf_ms << " ms\n";
                         
                        //  mgr.finalize(out);
                          candle_writer.write_candle(mgr.current, mgr.tf_ms);
                         
                     
                        }
                    }
                }
            }

                        else if (topic.find("publicTrade") != std::string::npos) {

                auto trades = obj["data"].as_array();
                int64_t seq=trades[0].as_object()["seq"].as_int64();

              std::cout << "perpetual trades arrived \n";

                TradeMessage msg;
                msg.seq=seq;

                for (auto& t : trades) {
                    auto& trade = t.as_object();
                    bool is_block = trade.contains("BT") && trade["BT"].as_bool();
                    bool is_rpi   = trade.contains("RPI") && trade["RPI"].as_bool();
                    if(!is_block && !is_rpi){
                    double price = to_double(trade["p"]);
                    double qty   = to_double(trade["v"]);
                    bool is_buy  = (trade["S"].as_string() == "Buy");
                    int64_t ts   = trade["T"].as_int64();

                    trade_buffer.push_back({price, qty, is_buy, ts});

                    Trade pci_trade;
                    pci_trade.price=price;
                    pci_trade.qty=qty;
                    pci_trade.is_buy=is_buy;
                    pci_trade.ts=ts;

                    msg.trades.push_back(pci_trade);

                    for (auto& mgr : managers) {
                        apply_trade(mgr, trade,tf_buffers);
                    }
                }

          
        }
                       if(!msg.trades.empty()){
                        perpetual_pci_engine.on_message(msg);
                       }

            }




        }
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
   
    return 0;
}
