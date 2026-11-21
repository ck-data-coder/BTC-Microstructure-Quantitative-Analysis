#include "mongo_client.hpp"
#include <stdexcept>

MongoClient::MongoClient(const std::string& uri) {
    mongoc_init();

    client_ = mongoc_client_new(uri.c_str());
    if (!client_) {
        throw std::runtime_error("Failed to create MongoDB client");
    }
}

MongoClient::~MongoClient() {
    if (client_) {
        mongoc_client_destroy(client_);
    }
    mongoc_cleanup();
}
