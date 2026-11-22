#pragma once
#include <mongoc/mongoc.h>
#include <string>

class MongoClient {
public:
    MongoClient(const std::string& uri);
    ~MongoClient();

    mongoc_client_t* client() const { return client_; }

private:
    mongoc_client_t* client_{nullptr};
};
