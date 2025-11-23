#include <iostream>
#include <string>
#include "lru_cache.h"
#include "db_handler.h"
#include "httplib.h"

int main(int argc, char **argv)
{
    // MySQL config
    std::string db_host = "127.0.0.1";
    std::string db_user = "kvuser";
    std::string db_pass = "kvpass";
    std::string db_name = "kvdb";
    unsigned int db_port = 3306;

    DBHandler db(db_host, db_user, db_pass, db_name, db_port);
    LRUCache<std::string, std::string> cache(1000);

    httplib::Server svr;

    // POST /kv
    svr.Post("/kv", [&](const httplib::Request &req, httplib::Response &res)
             {
        if (!req.has_param("key") || !req.has_param("value")) {
            res.status = 400;
            res.set_content("Bad request: missing key/value", "text/plain");
            return;
        }
        
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");
        
        if (db.put(key, value)) {
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
        std::string val;
        
        // Try cache first
        if (cache.get(key, val)) {
            res.status = 200;
            res.set_content(val, "text/plain");
            return;
        }

        // Fetch from DB
        auto opt = db.get(key);
        if (opt.has_value()) {
            cache.put(key, opt.value());
            res.status = 200;
            res.set_content(opt.value(), "text/plain");
        } else {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        } });

    // DELETE /kv/<key>
    svr.Delete(R"(/kv/([\w\-%\.]+))", [&](const httplib::Request &req, httplib::Response &res)
               {
        std::string key = req.matches[1];
        
        if (db.remove(key)) {
            cache.remove(key);
            res.status = 200;
            res.set_content("Deleted", "text/plain");
        } else {
            res.status = 500;
            res.set_content("Delete failed", "text/plain");
        } });

    std::cout << "Starting server at 0.0.0.0:8080\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}
