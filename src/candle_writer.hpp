#pragma once
#include <mongoc/mongoc.h>
#include <string>
#include "candle.hpp"
#include "mongo_candle_helpers.hpp"
class CandleWriter {
public:
    CandleWriter(mongoc_client_t* client,
                 const std::string& db_name);

    void write_candle(const Candle& c, int64_t tf_ms);

private:
    mongoc_client_t* client_;
    std::string db_name_;

    std::string collection_name(int64_t tf_ms);
};
