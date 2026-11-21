#pragma once

#include <bson/bson.h>
#include <string>
#include <unordered_map>
#include "candle.hpp"

// ------------------------------
// Append price → qty map
// (bid_hit / ask_hit)
// ------------------------------

inline void append_price_map(
    bson_t* parent,
    const char* key,
    const std::unordered_map<double, double>& mp
) {
    bson_t doc;
    BSON_APPEND_DOCUMENT_BEGIN(parent, key, &doc);

    for (const auto& [price, qty] : mp) {
        BSON_APPEND_DOUBLE(
            &doc,
            std::to_string(price).c_str(),
            qty
        );
    }

    bson_append_document_end(parent, &doc);
}

// ------------------------------
// Append cancel/refill level
// ------------------------------
inline void append_level(
    bson_t* parent,
    const char* key,
    const LevelMetrics& lvl
) {
    bson_t doc;
    BSON_APPEND_DOCUMENT_BEGIN(parent, key, &doc);

    BSON_APPEND_DOUBLE(&doc, "cancel", lvl.cancel_vol);
    BSON_APPEND_DOUBLE(&doc, "refill", lvl.refill_vol);

    bson_append_document_end(parent, &doc);
}

// ------------------------------
// Append bucket counts
// ------------------------------
inline void append_bucket_counts(
    bson_t* parent,
    const char* key,
    const Buckets& b,
    bool is_refill
) {
    bson_t doc;
    BSON_APPEND_DOCUMENT_BEGIN(parent, key, &doc);

    if (is_refill) {
        BSON_APPEND_INT32(&doc, "0.001_0.01", b.refill_001_01);
        BSON_APPEND_INT32(&doc, "0.011_0.1",  b.refill_01_1);
        BSON_APPEND_INT32(&doc, "0.11_0.5",   b.refill_1_5);
        BSON_APPEND_INT32(&doc, "0.51_1",     b.refill_5_10);
        BSON_APPEND_INT32(&doc, "1.01_5",     b.refill_10_50);
        BSON_APPEND_INT32(&doc, "gt_5",       b.refill_50p);
    } else {
        BSON_APPEND_INT32(&doc, "0.001_0.01", b.cancel_001_01);
        BSON_APPEND_INT32(&doc, "0.011_0.1",  b.cancel_01_1);
        BSON_APPEND_INT32(&doc, "0.11_0.5",   b.cancel_1_5);
        BSON_APPEND_INT32(&doc, "0.51_1",     b.cancel_5_10);
        BSON_APPEND_INT32(&doc, "1.01_5",     b.cancel_10_50);
        BSON_APPEND_INT32(&doc, "gt_5",       b.cancel_50p);
    }

    bson_append_document_end(parent, &doc);
}

// ------------------------------
// Append buckets for one depth
// ------------------------------
inline void append_depth_buckets(
    bson_t* parent,
    const char* key,
    const Buckets& b
) {
    bson_t doc;
    BSON_APPEND_DOCUMENT_BEGIN(parent, key, &doc);

    append_bucket_counts(&doc, "cancel", b, false);
    append_bucket_counts(&doc, "refill", b, true);

    bson_append_document_end(parent, &doc);
}

inline double safe_ratio(double num, double den) {
    return den > 0.0 ? num / den : 0.0;
}


inline void append_ratios(
    bson_t* parent,
    const Candle& c
) {
    bson_t doc;
    BSON_APPEND_DOCUMENT_BEGIN(parent, "ratios", &doc);

    const auto& B = c.bid;
    const auto& A = c.ask;

    // ---- Refill dominance ----
    BSON_APPEND_DOUBLE(&doc, "bid_best_refill_over_best5_cancel",
        safe_ratio(B.best.refill_vol, B.best5.cancel_vol));

    BSON_APPEND_DOUBLE(&doc, "bid_best_refill_over_best10_cancel",
        safe_ratio(B.best.refill_vol, B.best10.cancel_vol));

    BSON_APPEND_DOUBLE(&doc, "ask_best_refill_over_best5_cancel",
        safe_ratio(A.best.refill_vol, A.best5.cancel_vol));

    BSON_APPEND_DOUBLE(&doc, "ask_best_refill_over_best10_cancel",
        safe_ratio(A.best.refill_vol, A.best10.cancel_vol));

    // ---- Cancel pressure ----
    BSON_APPEND_DOUBLE(&doc, "bid_best_cancel_over_best5_refill",
        safe_ratio(B.best.cancel_vol, B.best5.refill_vol));

    BSON_APPEND_DOUBLE(&doc, "bid_best_cancel_over_best10_refill",
        safe_ratio(B.best.cancel_vol, B.best10.refill_vol));

    BSON_APPEND_DOUBLE(&doc, "ask_best_cancel_over_best5_refill",
        safe_ratio(A.best.cancel_vol, A.best5.refill_vol));

    BSON_APPEND_DOUBLE(&doc, "ask_best_cancel_over_best10_refill",
        safe_ratio(A.best.cancel_vol, A.best10.refill_vol));

    bson_append_document_end(parent, &doc);
}
