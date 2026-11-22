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
#include <thread>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace json = boost::json;
using tcp = net::ip::tcp;



 std::vector<TradeEvent> trade_buffer;
 int64_t last_delta_ts = 0;
 OrderBook book;

 RotatingFile perpetual_logger("C:/dev - Copy/bybit_ws/perpetual_pci_output.txt",500);
 RotatingFile spot_logger("C:/dev - Copy/bybit_ws/spot_pci_output.txt",500);
 RotatingFile logger2("C:/dev - Copy/bybit_ws/orderbook_output.txt",500);



  PCIEngine perpetual_pci_engine(perpetual_logger);
  PCIEngine spot_pci_engine(spot_logger);
  OrderbookEngine orderbook_engine;

std::atomic<websocket::stream<ssl::stream<tcp::socket>>*> perp_ws_ptr{nullptr};
std::atomic<websocket::stream<ssl::stream<tcp::socket>>*> spot_ws_ptr{nullptr};

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

std::atomic<int64_t> last_perp_msg_ts=now_ms();
std::atomic<int64_t> last_spot_msg_ts=now_ms();

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



inline int64_t to_int64(const boost::json::value& v){ if (v.is_int64()) return v.as_int64();
 if (v.is_uint64()) return static_cast<int64_t>(v.as_uint64());
 if (v.is_double()) return static_cast<int64_t>(v.as_double());
 if (v.is_string()) return std::stoll(v.as_string().c_str());
 throw std::runtime_error("Cannot convert JSON value to int64");}



 void process_perp(const std::string& msg_perp){

            json::object obj = json::parse(msg_perp).as_object();

            if (!obj.contains("topic")) {
                // ping / pong / subscribe ack → ignore
               return;
            }

           std::string topic = obj["topic"].as_string().c_str();

          //   std::cout<< obj["data"]<<"\n";

       
         
           if (topic.find("orderbook") != std::string::npos) {

    BookUpdate u = parse_book(msg_perp);

    if (u.is_snapshot) {
        book.apply_snapshot(u);

        last_delta_ts=u.book_ts;
          // 3️⃣ Clear any stale trades
        trade_buffer.clear();

        // ❌ NO analytics here
        return;
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
 
        
        if (!book.apply_delta(u)) {
            logger2.out << "Sequence broken — resync needed\n";
              std::cout << "Sequence broken — exiting loop\n";
            return ;
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

         
         if (topic.find("publicTrade") != std::string::npos) {

                auto trades = obj["data"].as_array();
                int64_t seq;

                if(trades[0].as_object().contains("seq")){

                seq=trades[0].as_object()["seq"].as_int64();

                }
                else{
                  seq= obj["ts"].as_int64();
                }

              std::cout << "[PERP] trades \n";

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

               //     trade_buffer.push_back({price, qty, is_buy, ts});

                    Trade pci_trade;
                    pci_trade.price=price;
                    pci_trade.qty=qty;
                    pci_trade.is_buy=is_buy;
                    pci_trade.ts=ts;

                    msg.trades.push_back(pci_trade);

                 
                }   
        }
                       if(!msg.trades.empty()){
                        perpetual_pci_engine.on_message(msg);
                       }

            }

 }


 void process_spot(const std::string& msg_spot){

        json::object obj = json::parse(msg_spot).as_object();

            if (!obj.contains("topic")) {
                // ping / pong / subscribe ack → ignore
               return;
            }

           std::string topic = obj["topic"].as_string().c_str();

          //   std::cout<< obj["data"]<<"\n";

       
              if (topic.find("publicTrade") != std::string::npos) {

                auto trades = obj["data"].as_array();
                int64_t seq;

                if(trades[0].as_object().contains("seq")){

                seq=trades[0].as_object()["seq"].as_int64();

                }
                else{
                  seq= obj["ts"].as_int64();
                }

              std::cout << "[SPOT] trades\n";

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

               //     trade_buffer.push_back({price, qty, is_buy, ts});

                    Trade pci_trade;
                    pci_trade.price=price;
                    pci_trade.qty=qty;
                    pci_trade.is_buy=is_buy;
                    pci_trade.ts=ts;

                    msg.trades.push_back(pci_trade);

                 
                }   
        }
                       if(!msg.trades.empty()){
                        spot_pci_engine.on_message(msg);
                       }

            }
 }


 void run_perp(websocket::stream<ssl::stream<tcp::socket>>& ws_perp){

        beast::flat_buffer perp_buffer;
         try{
          while (true) {

            ws_perp.read(perp_buffer);

            last_perp_msg_ts=now_ms();

            std::string msg_perp =
                beast::buffers_to_string(perp_buffer.data());
            perp_buffer.consume(perp_buffer.size());

            process_perp(msg_perp);
          }
        }
        catch(std::exception& e){
            std::cerr << "[PERP ERROR] " << e.what()<<"\n";
        }
 }

 void run_spot(websocket::stream<ssl::stream<tcp::socket>>& ws_spot){

        beast::flat_buffer spot_buffer;
       try{
        while(true){

             ws_spot.read(spot_buffer);
            
             last_spot_msg_ts=now_ms();

            std::string msg_spot =
                beast::buffers_to_string(spot_buffer.data());
            spot_buffer.consume(spot_buffer.size());

            process_spot(msg_spot);
          
        }
           }
        catch(std::exception& e){
            std::cerr << "[SPOT ERROR] " << e.what()<<"\n";
        }
 }


int perp_retry_count=0;

 void run_perp_safe() {
    while (true) {
        try {
            std::cout << "[PERP] Connecting...\n";

            net::io_context ioc;
            ssl::context ctx{ssl::context::tlsv12_client};
            ctx.set_default_verify_paths();

            tcp::resolver resolver{ioc};
            auto results = resolver.resolve("stream.bybit.com", "443");

            websocket::stream<ssl::stream<tcp::socket>> ws{ioc, ctx};

            net::connect(ws.next_layer().next_layer(),
                         results.begin(), results.end());

            SSL_set_tlsext_host_name(
                ws.next_layer().native_handle(),
                "stream.bybit.com");

            ws.next_layer().handshake(ssl::stream_base::client);

            ws.set_option(websocket::stream_base::decorator(
                [](websocket::request_type& req) {
                    req.set(beast::http::field::host, "stream.bybit.com");
                    req.set(beast::http::field::user_agent, "beast-client");
                }));

            ws.handshake("stream.bybit.com", "/v5/public/linear");

            
            // ws.control_callback(
            //     [&](websocket::frame_type kind, beast::string_view payload) {
            //         if (kind == websocket::frame_type::ping ||
            //             kind == websocket::frame_type::pong) {
            //                 last_perp_msg_ts = now_ms();
            //             }
            //         }
            //     );
            
                perp_ws_ptr = &ws;

            std::cout << "[PERP] Connected\n";

            
            // subscribe again
            std::string sub = R"({
                "op": "subscribe",
                "args": ["publicTrade.BTCUSDT"]
                })";
                
                ws.write(net::buffer(sub));
                
                // reuse your existing logic
                perp_retry_count=0;
                run_perp(ws);
                
            } catch (std::exception& e) {
                std::cerr << "[PERP ERROR] " << e.what() << "\n";
            }
            
        last_perp_msg_ts=now_ms();
        perp_ws_ptr=nullptr;

        perpetual_pci_engine.reset_seq();

        std::cout << "[PERP] Reconnecting...\n";
        perp_retry_count+=1;
        int delay_ms;
        if(perp_retry_count <=5 ) delay_ms=500;
        else if(perp_retry_count<=10) delay_ms=1000;
        else delay_ms =3000;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
}


int spot_retry_count=0;

void run_spot_safe() {
    while (true) {
        try {
            std::cout << "[SPOT] Connecting...\n";

            net::io_context ioc;
            ssl::context ctx{ssl::context::tlsv12_client};
            ctx.set_default_verify_paths();

            tcp::resolver resolver{ioc};
            auto results = resolver.resolve("stream.bybit.com", "443");

            websocket::stream<ssl::stream<tcp::socket>> ws{ioc, ctx};

            net::connect(ws.next_layer().next_layer(),
                         results.begin(), results.end());

            SSL_set_tlsext_host_name(
                ws.next_layer().native_handle(),
                "stream.bybit.com");

            ws.next_layer().handshake(ssl::stream_base::client);

            ws.set_option(websocket::stream_base::decorator(
                [](websocket::request_type& req) {
                    req.set(beast::http::field::host, "stream.bybit.com");
                    req.set(beast::http::field::user_agent, "beast-client");
                }));

            ws.handshake("stream.bybit.com", "/v5/public/spot");

            
            // ws.control_callback(
            //     [&](websocket::frame_type kind, beast::string_view payload) {
            //         if (kind == websocket::frame_type::ping ||
            //             kind == websocket::frame_type::pong) {
            //                 last_spot_msg_ts = now_ms();
            //             }
            //         }
            //     );

                spot_ws_ptr= &ws;

            std::cout << "[SPOT] Connected\n";

            
            std::string sub = R"({
                "op": "subscribe",
                "args": ["publicTrade.BTCUSDT"]
                })";
                
                ws.write(net::buffer(sub));
                
                spot_retry_count=0;
                run_spot(ws);
                
            } catch (std::exception& e) {
                std::cerr << "[SPOT ERROR] " << e.what() << "\n";
            }
            
        last_spot_msg_ts=now_ms();
        spot_ws_ptr=nullptr;

        spot_pci_engine.reset_seq();
        std::cout << "[SPOT] Reconnecting...\n";
        spot_retry_count+=1;
        int delay_ms;
        if(spot_retry_count<=5) delay_ms=500;
        else if(spot_retry_count<=10) delay_ms=1000;
        else delay_ms=3000;

        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
}


void monitor_perp() {
    while (true) {
        int64_t now = now_ms();

        if (now - last_perp_msg_ts > 50000) {
            std::cerr << "[PERP] STALE CONNECTION → FORCE CLOSE\n";

            auto ws = perp_ws_ptr.load();
            if (ws !=nullptr) {
                try{
                beast::error_code ec;
                ws->next_layer().next_layer().close(ec);
                }
                catch(...){}
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void monitor_spot() {
    while (true) {
        int64_t now = now_ms();

        if (now - last_spot_msg_ts > 50000) {
            std::cerr << "[SPOT] STALE CONNECTION → FORCE CLOSE\n";

            auto ws = spot_ws_ptr.load();

            if (ws !=nullptr) {
                try{
                beast::error_code ec;
                ws->next_layer().next_layer().close(ec);
                }
                catch(...){}
            }
        } 

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    try {
        std::cout << "PROGRAM STARTED\n";

        std::thread perp_thread(run_perp_safe);
        std::thread spot_thread(run_spot_safe);

        std::thread monitor_perp_thread(monitor_perp);
        
        std::thread monitor_spot_thread(monitor_spot);
        
        monitor_perp_thread.detach();
        monitor_spot_thread.detach();
        perp_thread.join();
        spot_thread.join();


    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}