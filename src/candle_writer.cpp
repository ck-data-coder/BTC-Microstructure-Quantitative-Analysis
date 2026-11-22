#include "candle_writer.hpp"
#include <bson/bson.h>
#include <stdexcept>
#include "candle_manager.hpp"

CandleWriter::CandleWriter(
    mongoc_client_t* client,
    const std::string& db_name
)
    : client_(client), db_name_(db_name) {}

std::string CandleWriter::collection_name(int64_t tf_ms) {
    switch (tf_ms) {
        case 60'000:     return "candles_1m";
        case 300'000:    return "candles_5m";
        case 900'000:    return "candles_15m";
        case 1'800'000:  return "candles_30m";
        case 3'600'000:  return "candles_1h";
        case 14'400'000: return "candles_4h";
        default:         return "candles_unknown";
    }
}


void CandleWriter::write_candle(const Candle& c, int64_t tf_ms) {
    bson_t doc;
    bson_init(&doc);

    // ---- core candle ----
    BSON_APPEND_INT64(&doc, "start", c.start);
    BSON_APPEND_INT64(&doc, "end",   c.end);
  
    
    BSON_APPEND_DOUBLE(&doc, "open",  c.open);
    BSON_APPEND_DOUBLE(&doc, "high",  c.high);
    BSON_APPEND_DOUBLE(&doc, "low",   c.low);
    BSON_APPEND_DOUBLE(&doc, "close", c.close);
    BSON_APPEND_DOUBLE(&doc, "volume", c.volume);

    // ---- trade flow ----
    BSON_APPEND_DOUBLE(&doc, "buy_vol",  c.buy_vol);
    BSON_APPEND_DOUBLE(&doc, "sell_vol", c.sell_vol);
    BSON_APPEND_DOUBLE(&doc, "total_calc_vol", c.buy_vol + c.sell_vol);


   // ===============================
    // Ticker metrics
    // ===============================
    const auto& T = c.ticker;
    bson_t ticker;
    BSON_APPEND_DOCUMENT_BEGIN(&doc, "ticker", &ticker);

    BSON_APPEND_DOUBLE(&ticker, "start_last", T.start_last);
    BSON_APPEND_DOUBLE(&ticker, "end_last",   T.end_last);
    BSON_APPEND_DOUBLE(&ticker, "delta_last",   T.end_last-T.start_last);
    BSON_APPEND_DOUBLE(&ticker, "start_mark", T.start_mark);
    BSON_APPEND_DOUBLE(&ticker, "end_mark",   T.end_mark);
    BSON_APPEND_DOUBLE(&ticker, "delta_mark",   T.end_mark-T.start_mark);
    BSON_APPEND_DOUBLE(&ticker, "start_oi",   T.start_oi);
    BSON_APPEND_DOUBLE(&ticker, "end_oi",     T.end_oi);
    BSON_APPEND_DOUBLE(&ticker, "delta_oi",     T.end_oi-T.start_oi);
    BSON_APPEND_DOUBLE(&ticker, "start_funding", T.start_funding);
    BSON_APPEND_DOUBLE(&ticker, "end_funding",   T.end_funding);
    BSON_APPEND_DOUBLE(&ticker, "delta_funding",   T.end_funding-T.start_funding);

    BSON_APPEND_DOUBLE(&ticker, "abs_last",    T.abs_last);
    BSON_APPEND_DOUBLE(&ticker, "abs_oi",      T.abs_oi);
    BSON_APPEND_DOUBLE(&ticker, "abs_funding", T.abs_funding);
    BSON_APPEND_DOUBLE(&ticker, "abs_basis",   T.abs_basis);

    bson_append_document_end(&doc, &ticker);

    // ===============================
    // Bid / Ask hit maps
    // ===============================
    bson_t hits;
    BSON_APPEND_DOCUMENT_BEGIN(&doc, "hits", &hits);
    append_price_map(&hits, "bid", c.bid_hit);
    append_price_map(&hits, "ask", c.ask_hit);
    bson_append_document_end(&doc, &hits);

    // ===============================
    // Orderbook metrics
    // ===============================
    bson_t ob, bid, ask;
    BSON_APPEND_DOCUMENT_BEGIN(&doc, "orderbook", &ob);

    BSON_APPEND_DOCUMENT_BEGIN(&ob, "bid", &bid);
    append_level(&bid, "best",   c.bid.best);
    append_level(&bid, "best5",  c.bid.best5);
    append_level(&bid, "best10", c.bid.best10);
    bson_append_document_end(&ob, &bid);

    BSON_APPEND_DOCUMENT_BEGIN(&ob, "ask", &ask);
    append_level(&ask, "best",   c.ask.best);
    append_level(&ask, "best5",  c.ask.best5);
    append_level(&ask, "best10", c.ask.best10);
    bson_append_document_end(&ob, &ask);

    bson_append_document_end(&doc, &ob);

    // ===============================
    // Buckets
    // ===============================
    bson_t buckets, bidb, askb;
    BSON_APPEND_DOCUMENT_BEGIN(&doc, "buckets", &buckets);

    BSON_APPEND_DOCUMENT_BEGIN(&buckets, "bid", &bidb);
    append_depth_buckets(&bidb, "best",   c.bid.best.buckets);
    append_depth_buckets(&bidb, "best5",  c.bid.best5.buckets);
    append_depth_buckets(&bidb, "best10", c.bid.best10.buckets);
    bson_append_document_end(&buckets, &bidb);

    BSON_APPEND_DOCUMENT_BEGIN(&buckets, "ask", &askb);
    append_depth_buckets(&askb, "best",   c.ask.best.buckets);
    append_depth_buckets(&askb, "best5",  c.ask.best5.buckets);
    append_depth_buckets(&askb, "best10", c.ask.best10.buckets);
    bson_append_document_end(&buckets, &askb);

    bson_append_document_end(&doc, &buckets);
   
   
     // ===============================
    // Ratios (core signals)
     // ===============================
    append_ratios(&doc, c);

   
    // ---- insert ----
    auto coll_name = collection_name(tf_ms);
    mongoc_collection_t* coll =
        mongoc_client_get_collection(
            client_,
            db_name_.c_str(),
            coll_name.c_str()
        );

    bson_error_t error;
    if (!mongoc_collection_insert_one(
            coll, &doc, nullptr, nullptr, &error)) {
        mongoc_collection_destroy(coll);
        bson_destroy(&doc);
        throw std::runtime_error(error.message);
    }

    mongoc_collection_destroy(coll);
    bson_destroy(&doc);
}
