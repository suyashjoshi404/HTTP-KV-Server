#include <iostream>
#include <string>
#include <chrono>
#include <atomic>
#include <sstream>
#include "thread_pool.h"
#include "lru_cache.h"
#include "db_handler.h"
#include "httplib.h" // place cpp-httplib header in include/

using namespace std::chrono;

int main(int argc, char **argv)
{
    // config
    std::string host = "0.0.0.0";
    int port = 8080;
    size_t cache_capacity = 10000;
    size_t thread_pool_size = std::thread::hardware_concurrency();
    if (thread_pool_size == 0)
        thread_pool_size = 4;

    // MySQL config - adjust or read from args/env
    std::string db_host = "127.0.0.1";
    std::string db_user = "kvuser";
    std::string db_pass = "kvpass";
    std::string db_name = "kvdb";
    unsigned int db_port = 3306;

    // You may parse command-line args to change these values

    DBHandler db(db_host, db_user, db_pass, db_name, db_port);
    LRUCache<std::string, std::string> cache(cache_capacity);
    ThreadPool pool(thread_pool_size);

    httplib::Server svr;

    // POST /kv  body: JSON-like or form: key=...&value=...
    svr.Post("/kv", [&](const httplib::Request &req, httplib::Response &res)
             {
        // parse key,value from body (we expect form data or urlencoded)
        std::string key, value;
        if (req.has_param("key") && req.has_param("value")) {
            key = req.get_param_value("key");
            value = req.get_param_value("value");
        } else {
            // try raw body as key:value (simple)
            auto pos = req.body.find(':');
            if (pos != std::string::npos) {
                key = req.body.substr(0, pos);
                value = req.body.substr(pos+1);
            } else {
                res.status = 400;
                res.set_content("Bad request: missing key/value", "text/plain");
                return;
            }
        }

        // Execute directly instead of using thread pool to avoid deadlock
        bool ok = db.put(key, value);
        if (ok) {
            cache.put(key, value);
            res.status = 201;
            res.set_content("OK", "text/plain");
        } else {
            res.status = 500;
            res.set_content("DB error", "text/plain");
        } });

    // GET /kv/<key>
    svr.Get(R"(/kv/([\w\-%\.]+))", [&](const httplib::Request &req, httplib::Response &res)
            {
        std::string key = req.matches[1];

        // first try cache
        std::string val;
        if (cache.get(key, val)) {
            res.status = 200;
            res.set_content(val, "text/plain");
            return;
        }

        // else fetch from DB directly to avoid deadlock
        auto opt = db.get(key);
        if (!opt.has_value()) {
            res.status = 404;
            res.set_content("Not found", "text/plain");
            return;
        }
        cache.put(key, opt.value());
        res.status = 200;
        res.set_content(opt.value(), "text/plain"); });

    // DELETE /kv/<key>
    svr.Delete(R"(/kv/([\w\-%\.]+))", [&](const httplib::Request &req, httplib::Response &res)
               {
        std::string key = req.matches[1];
        
        // Execute directly to avoid deadlock
        bool ok = db.remove(key);
        if (ok) {
            cache.remove(key);
            res.status = 200;
            res.set_content("Deleted", "text/plain");
        } else {
            res.status = 500;
            res.set_content("Delete failed", "text/plain");
        } });

    std::cout << "Starting server at " << host << ":" << port << " with pool size " << thread_pool_size << "\n";
    svr.listen(host.c_str(), port);
    return 0;
}
