#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <sstream>
#include <fstream>
#include "httplib.h"

using namespace std::chrono;

struct Stats
{
    std::atomic<uint64_t> success{0};
    std::atomic<uint64_t> failure{0};
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> total_latency_us{0};
};

void run_client_thread(int id, const std::string &server_host, int server_port,
                       int duration_seconds, double read_ratio,
                       int key_space, Stats &stats, int think_ms)
{

    httplib::Client cli(server_host.c_str(), server_port);
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);

    std::mt19937_64 rng(id + std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> keydist(1, key_space);
    std::uniform_real_distribution<double> opdist(0.0, 1.0);

    auto end_time = steady_clock::now() + seconds(duration_seconds);
    while (steady_clock::now() < end_time)
    {
        double op = opdist(rng);
        int k = keydist(rng);
        std::string key = "key" + std::to_string(k);
        auto start = high_resolution_clock::now();
        bool success = false;

        if (op <= read_ratio)
        {
            // GET
            auto res = cli.Get(("/kv/" + key).c_str());
            if (res && res->status == 200)
                success = true;
        }
        else
        {
            // PUT (create/update) with small payload
            std::string value = "value_from_thread_" + std::to_string(id) + "_" + std::to_string(k);
            httplib::Params params;
            params.emplace("key", key);
            params.emplace("value", value);
            auto res = cli.Post("/kv", params);
            if (res && (res->status == 200 || res->status == 201))
                success = true;
        }

        auto stop = high_resolution_clock::now();
        uint64_t lat = duration_cast<microseconds>(stop - start).count();
        stats.total_requests++;
        stats.total_latency_us += lat;
        if (success)
            stats.success++;
        else
            stats.failure++;

        if (think_ms > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(think_ms));
    }
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cout << "Usage: load_generator <server_host> <server_port> <clients> <duration_seconds> <read_ratio> [key_space] [think_ms] [output_file]\n";
        std::cout << "       load_generator <server_host> <server_port>  # remaining parameters via stdin\n";
        std::cout << "example: ./load_generator 127.0.0.1 8080 50 60 0.9 1000 0\n";
        return 1;
    }
    std::string host = argv[1];
    int port = std::stoi(argv[2]);
    int clients = 0;
    int duration_seconds = 0;
    double read_ratio = 0.0;
    int key_space = 1000;
    int think_ms = 0;
    std::string output_file;

    if (argc >= 6)
    {
        clients = std::stoi(argv[3]);
        duration_seconds = std::stoi(argv[4]);
        read_ratio = std::stod(argv[5]);
        if (argc >= 7)
            key_space = std::stoi(argv[6]);
        if (argc >= 8)
            think_ms = std::stoi(argv[7]);
        if (argc >= 9)
            output_file = argv[8];
    }
    else
    {
        // Support the bash script by reading remaining parameters from stdin when not supplied via argv.
        if (!(std::cin >> clients >> duration_seconds >> read_ratio >> key_space >> think_ms))
        {
            std::cerr << "Failed to read clients, duration, read ratio, key space, and think ms from stdin.\n";
            return 1;
        }
        if (!(std::cin >> output_file))
        {
            output_file.clear();
        }
    }

    Stats stats;

    std::vector<std::thread> threads;
    for (int i = 0; i < clients; ++i)
    {
        threads.emplace_back(run_client_thread, i, host, port, duration_seconds, read_ratio, key_space, std::ref(stats), think_ms);
    }
    auto t0 = high_resolution_clock::now();
    for (auto &t : threads)
        t.join();
    auto t1 = high_resolution_clock::now();

    double total_time_s = duration_cast<duration<double>>(t1 - t0).count();
    uint64_t succ = stats.success.load();
    uint64_t fail = stats.failure.load();
    uint64_t total = stats.total_requests.load();
    double avg_latency_ms = (total ? (double)stats.total_latency_us.load() / total / 1000.0 : 0.0);
    double throughput = total / total_time_s;

    std::ostringstream summary;
    summary << "RESULTS:\n";
    summary << " Total time (s): " << total_time_s << "\n";
    summary << " Clients: " << clients << "\n";
    summary << " Requests: " << total << "  Success: " << succ << " Fail: " << fail << "\n";
    summary << " Throughput (req/s): " << throughput << "\n";
    summary << " Avg latency (ms): " << avg_latency_ms << "\n";

    const std::string summary_text = summary.str();
    std::cout << summary_text;

    if (!output_file.empty())
    {
        std::ofstream out(output_file, std::ios::app);
        if (out.is_open())
        {
            out << summary_text;
        }
        else
        {
            std::cerr << "Unable to write results to " << output_file << "\n";
        }
    }
    return 0;
}
