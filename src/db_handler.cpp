#include "db_handler.h"
#include <iostream>
#include <vector>

DBHandler::ConnectionHandle::ConnectionHandle(DBHandler *handler_, MYSQL *conn_)
    : handler(handler_), conn(conn_)
{
}

DBHandler::ConnectionHandle::ConnectionHandle(ConnectionHandle &&other) noexcept
    : handler(other.handler), conn(other.conn)
{
    other.handler = nullptr;
    other.conn = nullptr;
}

DBHandler::ConnectionHandle &DBHandler::ConnectionHandle::operator=(ConnectionHandle &&other) noexcept
{
    if (this != &other)
    {
        if (conn && handler)
        {
            handler->release_connection(conn);
        }
        handler = other.handler;
        conn = other.conn;
        other.handler = nullptr;
        other.conn = nullptr;
    }
    return *this;
}

DBHandler::ConnectionHandle::~ConnectionHandle()
{
    if (conn && handler)
    {
        handler->release_connection(conn);
    }
}

DBHandler::DBHandler(const std::string &host, const std::string &user,
                     const std::string &password, const std::string &dbname, unsigned int port,
                     std::size_t pool_size_in)
    : host_(host), user_(user), password_(password), dbname_(dbname), port_(port), pool_valid(false), pool_size(pool_size_in ? pool_size_in : 1)
{
    const std::size_t requested_pool_size = pool_size;
    for (std::size_t i = 0; i < requested_pool_size; ++i)
    {
        MYSQL *conn = create_connection();
        if (!conn)
        {
            std::cerr << "mysql_real_connect failed while building pool\n";
            pool_valid = false;
            break;
        }
        all_connections.push_back(conn);
        available_connections.push(conn);
    }

    pool_size = all_connections.size();

    pool_valid = !available_connections.empty();
    if (!pool_valid)
    {
        std::cerr << "Failed to initialize MySQL connection pool\n";
        pool_cv.notify_all();
        return;
    }

    // Ensure table exists using one of the pooled connections.
    auto conn_handle = acquire_connection();
    if (MYSQL *conn = conn_handle.get())
    {
        const char *create_table = "CREATE TABLE IF NOT EXISTS kv_store ("
                                   "k VARCHAR(255) PRIMARY KEY, v TEXT)";
        if (!execute_query(conn, create_table))
        {
            std::cerr << "Failed to create table\n";
        }
    }
}

DBHandler::~DBHandler()
{
    {
        std::lock_guard<std::mutex> lock(pool_mutex);
        pool_valid = false;
    }
    pool_cv.notify_all();

    for (MYSQL *conn : all_connections)
    {
        if (conn)
        {
            mysql_close(conn);
        }
    }
}

DBHandler::ConnectionHandle DBHandler::acquire_connection()
{
    std::unique_lock<std::mutex> lock(pool_mutex);
    pool_cv.wait(lock, [this]
                 { return !available_connections.empty() || !pool_valid; });
    if (!pool_valid)
    {
        return ConnectionHandle(nullptr, nullptr);
    }
    MYSQL *conn = available_connections.front();
    available_connections.pop();
    lock.unlock();
    return ConnectionHandle(this, conn);
}

void DBHandler::release_connection(MYSQL *conn)
{
    if (!conn)
        return;
    std::unique_lock<std::mutex> lock(pool_mutex);
    available_connections.push(conn);
    lock.unlock();
    pool_cv.notify_one();
}

MYSQL *DBHandler::create_connection()
{
    MYSQL *conn = mysql_init(nullptr);
    if (!conn)
    {
        std::cerr << "mysql_init failed\n";
        return nullptr;
    }

    if (!mysql_real_connect(conn, host_.c_str(), user_.c_str(), password_.c_str(),
                            dbname_.c_str(), port_, nullptr, 0))
    {
        std::cerr << "mysql_real_connect failed: " << mysql_error(conn) << "\n";
        mysql_close(conn);
        return nullptr;
    }

    return conn;
}

std::string DBHandler::escape(MYSQL *conn, const std::string &str)
{
    if (!conn)
        return {};
    std::vector<char> buf(str.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn, buf.data(), str.c_str(), str.size());
    return std::string(buf.data(), len);
}

bool DBHandler::execute_query(MYSQL *conn, const std::string &query)
{
    if (!conn)
        return false;
    if (mysql_query(conn, query.c_str()))
    {
        std::cerr << "Query failed: " << mysql_error(conn) << "\n";
        return false;
    }
    return true;
}

bool DBHandler::put(const std::string &key, const std::string &value)
{
    auto handle = acquire_connection();
    MYSQL *conn = handle.get();
    if (!conn)
        return false;

    std::string query = "INSERT INTO kv_store (k, v) VALUES ('" + escape(conn, key) +
                        "', '" + escape(conn, value) + "') ON DUPLICATE KEY UPDATE v = VALUES(v)";
    return execute_query(conn, query);
}

std::optional<std::string> DBHandler::get(const std::string &key)
{
    auto handle = acquire_connection();
    MYSQL *conn = handle.get();
    if (!conn)
        return std::nullopt;

    std::string query = "SELECT v FROM kv_store WHERE k = '" + escape(conn, key) + "' LIMIT 1";
    if (mysql_query(conn, query.c_str()))
    {
        std::cerr << "Select query failed: " << mysql_error(conn) << "\n";
        return std::nullopt;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return std::nullopt;

    MYSQL_ROW row = mysql_fetch_row(res);
    std::optional<std::string> result;
    if (row && row[0])
    {
        result = std::string(row[0]);
    }
    mysql_free_result(res);
    return result;
}

bool DBHandler::remove(const std::string &key)
{
    auto handle = acquire_connection();
    MYSQL *conn = handle.get();
    if (!conn)
        return false;

    std::string query = "DELETE FROM kv_store WHERE k = '" + escape(conn, key) + "'";
    return execute_query(conn, query);
}
