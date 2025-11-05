#include "db_handler.h"
#include <iostream>
#include <sstream>

DBHandler::DBHandler(const std::string &host, const std::string &user,
                     const std::string &password, const std::string &dbname, unsigned int port)
    : conn(nullptr), initialized(false)
{

    conn = mysql_init(nullptr);
    if (!conn)
    {
        std::cerr << "mysql_init failed\n";
        return;
    }
    // Connect
    if (!mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), port, nullptr, 0))
    {
        std::cerr << "mysql_real_connect failed: " << mysql_error(conn) << "\n";
        mysql_close(conn);
        conn = nullptr;
        return;
    }
    initialized = true;

    // Ensure table exists
    std::string q = "CREATE TABLE IF NOT EXISTS kv_store ("
                    "k VARCHAR(255) PRIMARY KEY, "
                    "v TEXT)";
    if (!execute_non_query(q))
    {
        std::cerr << "Failed to create table\n";
    }
}

DBHandler::~DBHandler()
{
    if (conn)
        mysql_close(conn);
}

bool DBHandler::execute_non_query(const std::string &q)
{
    if (!conn)
        return false;
    if (mysql_query(conn, q.c_str()))
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
    // Use INSERT ... ON DUPLICATE KEY UPDATE
    std::string esc_key = mysql_real_escape_string ? "" : ""; // placeholder
    std::string k_esc, v_esc;
    // escape key and value
    char *kbuf = new char[key.size() * 2 + 1];
    char *vbuf = new char[value.size() * 2 + 1];
    unsigned long klen = mysql_real_escape_string(conn, kbuf, key.c_str(), key.size());
    unsigned long vlen = mysql_real_escape_string(conn, vbuf, value.c_str(), value.size());
    k_esc.assign(kbuf, klen);
    v_esc.assign(vbuf, vlen);
    delete[] kbuf;
    delete[] vbuf;

    std::ostringstream ss;
    ss << "INSERT INTO kv_store (k, v) VALUES ('" << k_esc << "', '" << v_esc << "') "
       << "ON DUPLICATE KEY UPDATE v = VALUES(v)";

    return execute_non_query(ss.str());
}

std::optional<std::string> DBHandler::get(const std::string &key)
{
    std::lock_guard<std::mutex> lock(conn_mutex);
    if (!conn)
        return std::nullopt;

    // escape key
    char *kbuf = new char[key.size() * 2 + 1];
    unsigned long klen = mysql_real_escape_string(conn, kbuf, key.c_str(), key.size());
    std::string k_esc(kbuf, klen);
    delete[] kbuf;

    std::string q = "SELECT v FROM kv_store WHERE k = '" + k_esc + "' LIMIT 1";
    if (mysql_query(conn, q.c_str()))
    {
        std::cerr << "Select query failed: " << mysql_error(conn) << "\n";
        return std::nullopt;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return std::nullopt;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row)
    {
        mysql_free_result(res);
        return std::nullopt;
    }
    std::string val = row[0] ? row[0] : "";
    mysql_free_result(res);
    return val;
}

bool DBHandler::remove(const std::string &key)
{
    std::lock_guard<std::mutex> lock(conn_mutex);
    if (!conn)
        return false;
    char *kbuf = new char[key.size() * 2 + 1];
    unsigned long klen = mysql_real_escape_string(conn, kbuf, key.c_str(), key.size());
    std::string k_esc(kbuf, klen);
    delete[] kbuf;
    std::string q = "DELETE FROM kv_store WHERE k = '" + k_esc + "'";
    return execute_non_query(q);
}
