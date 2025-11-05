#pragma once
#include <string>
#include <optional>
#include <mutex>
#include <mysql/mysql.h>

class DBHandler
{
public:
    DBHandler(const std::string &host, const std::string &user,
              const std::string &password, const std::string &dbname, unsigned int port = 3306);
    ~DBHandler();

    bool put(const std::string &key, const std::string &value); // insert or update
    std::optional<std::string> get(const std::string &key);
    bool remove(const std::string &key);

private:
    MYSQL *conn;
    bool initialized;
    std::mutex conn_mutex;                        // protect MySQL connection from concurrent access
    bool execute_non_query(const std::string &q); // helper
};
