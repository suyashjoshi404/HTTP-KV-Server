#pragma once
#include <string>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <cstddef>
#include <mysql/mysql.h>

class DBHandler
{
public:
    DBHandler(const std::string &host, const std::string &user,
              const std::string &password, const std::string &dbname, unsigned int port = 3306,
              std::size_t pool_size = 8);
    ~DBHandler();

    bool put(const std::string &key, const std::string &value);
    std::optional<std::string> get(const std::string &key);
    bool remove(const std::string &key);

private:
    struct ConnectionHandle
    {
        DBHandler *handler;
        MYSQL *conn;

        ConnectionHandle(DBHandler *handler_, MYSQL *conn_);
        ConnectionHandle(const ConnectionHandle &) = delete;
        ConnectionHandle &operator=(const ConnectionHandle &) = delete;
        ConnectionHandle(ConnectionHandle &&other) noexcept;
        ConnectionHandle &operator=(ConnectionHandle &&other) noexcept;
        ~ConnectionHandle();
        MYSQL *get() const { return conn; }
    };

    ConnectionHandle acquire_connection();
    void release_connection(MYSQL *conn);
    MYSQL *create_connection();

    std::string escape(MYSQL *conn, const std::string &str);
    bool execute_query(MYSQL *conn, const std::string &query);

    std::string host_;
    std::string user_;
    std::string password_;
    std::string dbname_;
    unsigned int port_;
    std::vector<MYSQL *> all_connections;
    std::queue<MYSQL *> available_connections;
    std::mutex pool_mutex;
    std::condition_variable pool_cv;
    bool pool_valid;
    std::size_t pool_size;
};
