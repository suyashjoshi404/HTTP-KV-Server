#include "db_handler.h"
#include <iostream>
#include <vector>

DBHandler::DBHandler(const std::string &host, const std::string &user,
                     const std::string &password, const std::string &dbname, unsigned int port)
    : conn(nullptr)
{
    conn = mysql_init(nullptr);
    if (!conn)
    {
        std::cerr << "mysql_init failed\n";
        return;
    }

    if (!mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(),
                            dbname.c_str(), port, nullptr, 0))
    {
        std::cerr << "mysql_real_connect failed: " << mysql_error(conn) << "\n";
        mysql_close(conn);
        conn = nullptr;
        return;
    }

    // Ensure table exists
    const char *create_table = "CREATE TABLE IF NOT EXISTS kv_store ("
                               "k VARCHAR(255) PRIMARY KEY, v TEXT)";
    if (!execute_query(create_table))
    {
        std::cerr << "Failed to create table\n";
    }
}

DBHandler::~DBHandler()
{
    if (conn)
        mysql_close(conn);
}

std::string DBHandler::escape(const std::string &str)
{
    std::vector<char> buf(str.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn, buf.data(), str.c_str(), str.size());
    return std::string(buf.data(), len);
}

bool DBHandler::execute_query(const std::string &query)
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
    std::lock_guard<std::mutex> lock(conn_mutex);
    if (!conn)
        return false;

    std::string query = "INSERT INTO kv_store (k, v) VALUES ('" + escape(key) +
                        "', '" + escape(value) + "') ON DUPLICATE KEY UPDATE v = VALUES(v)";
    return execute_query(query);
}

std::optional<std::string> DBHandler::get(const std::string &key)
{
    std::lock_guard<std::mutex> lock(conn_mutex);
    if (!conn)
        return std::nullopt;

    std::string query = "SELECT v FROM kv_store WHERE k = '" + escape(key) + "' LIMIT 1";
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
    std::lock_guard<std::mutex> lock(conn_mutex);
    if (!conn)
        return false;

    std::string query = "DELETE FROM kv_store WHERE k = '" + escape(key) + "'";
    return execute_query(query);
}
