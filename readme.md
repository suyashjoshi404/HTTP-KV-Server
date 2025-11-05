# CS744 Project: HTTP-based Key-Value Server (C++)

## Overview

This project implements a **multi-tier HTTP key-value server** with:

- A multi-threaded C++ HTTP server using cpp-httplib
- An in-memory LRU cache
- A persistent MySQL database
- A load generator for performance testing

It supports POST (create/update), GET (read), and DELETE operations on key-value pairs using REST APIs.

---

## System Requirements

- Ubuntu 20.04+ or any Linux with CMake ≥ 3.10
- C++17 compatible compiler (g++, clang++)
- MySQL Server
- Libraries used:

  - libmysqlclient-dev
  - build-essential
  - cmake
  - curl

Install dependencies:

```bash
sudo apt update
sudo apt install build-essential cmake libmysqlclient-dev mysql-server curl
```

---

## Project Directory Structure

```
HTTP-KV-Server/
├── CMakeLists.txt
├── include/
│   └── httplib.h    # from https://github.com/yhirose/cpp-httplib
├── src/
│   ├── thread_pool.h
│   ├── lru_cache.h
│   ├── db_handler.h
│   ├── db_handler.cpp
│   ├── server.cpp
│   └── load_generator.cpp
└── README.md
```

---

## MySQL Setup

```bash
sudo systemctl enable mysql
sudo systemctl start mysql
sudo mysql_secure_installation
```

Then we create a dedicated database and user:

```bash
sudo mysql -u root -p
# If prompted for a password, simply press enter if you did not create a password
```

Then we use the MySQL prompt:

```sql
-- To see which databases exist already
-- SHOW DATABASES;
CREATE DATABASE kvdb;
-- If user already exists in the database use the below command
-- DROP USER 'kvuser'@'localhost';
CREATE USER 'kvuser'@'localhost' IDENTIFIED BY 'kvpass';
GRANT ALL PRIVILEGES ON kvdb.* TO 'kvuser'@'localhost';
FLUSH PRIVILEGES;
EXIT;
```

Testing the connection:

```bash
mysql -u kvuser -p kvdb
# password: kvpass
# You may then exit mysql
```

---

## Build Instructions

```bash
# I have already included the build directory for ease of use :)
cd HTTP-KV-Server
mkdir build && cd build
cmake ..
make -j
```

This will then generate two executables:

```
kv_server
load_generator
```

---

## Running the Server

Run the server (default port 8080):

```bash
./kv_server
```

Expected output:

```
Starting server at 0.0.0.0:8080 with pool size <n>
```

---

## Testing the API

### Create (POST)

```bash
curl -X POST -d "key=foo" -d "value=bar" http://127.0.0.1:8080/kv
# Output: OK
```

### Read (GET)

```bash
curl http://127.0.0.1:8080/kv/foo
# Output: bar
```

### Delete

```bash
curl -X DELETE http://127.0.0.1:8080/kv/foo
# Output: Deleted
```

### Verify in MySQL

```bash
mysql -u kvuser -pkvpass kvdb -e "SELECT * FROM kv_store;"
```

---

## Running the Load Generator

```
./load_generator <server_host> <server_port> <clients> <duration_seconds> <read_ratio> [key_space] [think_ms]
```

**Parameters:**

| Parameter          | Meaning                              |
| ------------------ | ------------------------------------ |
| `server_host`      | IP or hostname (e.g. `127.0.0.1`)    |
| `server_port`      | Server port (default `8080`)         |
| `clients`          | Number of client threads             |
| `duration_seconds` | Test duration                        |
| `read_ratio`       | Fraction of read requests (0.0–1.0)  |
| `key_space`        | Distinct key range (default 1000)    |
| `think_ms`         | Optional think time between requests |

---

### Example

#### 1. CPU-bound cache workload (Get Popular)

Repeated reads on a small key set:

```bash
./load_generator 127.0.0.1 8080 20 60 1.0 10 0
```

#### 2. Disk-bound workload (Put All)

All writes/deletes hitting DB:

```bash
./load_generator 127.0.0.1 8080 20 60 0.0 10000 0
```

#### 3. Mixed workload

Random read/write mix:

```bash
./load_generator 127.0.0.1 8080 50 120 0.8 5000 0
```

The program prints:

```
RESULTS:
 Total time (s): 60.002
 Clients: 20
 Requests: 120000  Success: 119995  Fail: 5
 Throughput (req/s): 1999.8
 Avg latency (ms): 2.34
```

---

## Performance Evaluation

- Throughput (req/s) and average latency (ms) from load generator output
- Resource utilization using:

  ```bash
  top, htop, vmstat, iostat, perf top
  ```

- Run at increasing client counts (e.g. 5, 10, 20, 50, 100)
- Plot throughput vs concurrency and latency vs concurrency

### Example workloads

| Workload      | Requests             | Expected Bottleneck |
| ------------- | -------------------- | ------------------- |
| `Put All`     | Only create/delete   | Disk (DB I/O)       |
| `Get All`     | Reads on unique keys | Disk (DB I/O)       |
| `Get Popular` | Hot small key set    | CPU or cache        |
| `Get+Put`     | Mixed read/write     | Depends on mix      |

---

## SQL Verification Steps

You can manually verify persistence using:

```sql
SHOW TABLES;
SELECT * FROM kv_store LIMIT 10;
SELECT COUNT(*) FROM kv_store;
```

To check updates:

```bash
curl -X POST -d "key=mykey" -d "value=updated_value" http://127.0.0.1:8080/kv
mysql -u kvuser -pkvpass kvdb -e "SELECT * FROM kv_store WHERE k='mykey';"
```

To check delete:

```bash
curl -X DELETE http://127.0.0.1:8080/kv/mykey
mysql -u kvuser -pkvpass kvdb -e "SELECT * FROM kv_store WHERE k='mykey';"
```

---

## Experiment Guidelines

1. Use two separate machines or cores for server and load generator.
2. Keep each experiment running ≥ 5 minutes for steady-state throughput.
3. Collect metrics for at least 5 load levels per workload.
4. Identify:

   - When throughput saturates
   - When latency sharply increases
   - Which resource (CPU, disk, network) reaches ~100% utilization

---

## Submission Checklist

✅ Source code (server, cache, DB handler, client)
✅ CMake build instructions
✅ MySQL setup guide
✅ Throughput & latency graphs for 2 workloads
✅ Bottleneck analysis and explanation

---

## Troubleshooting

| Issue                             | Likely Cause                       | Fix                                                |
| --------------------------------- | ---------------------------------- | -------------------------------------------------- |
| `DB error` on server              | Wrong credentials / DB not running | Check MySQL status and credentials in `server.cpp` |
| `curl` gives “Connection refused” | Server not started or wrong port   | Run `./kv_server` and verify port 8080             |
| MySQL “Access denied”             | Wrong user privileges              | Re-run GRANT commands                              |
| Empty `kv_store` table            | POST didn’t reach server           | Check `./kv_server` logs                           |
| High latency under low load       | MySQL sync I/O bottleneck          | Use SSD or reduce key space                        |

---

## Example End-to-End Run

1. Start MySQL (`sudo systemctl start mysql`)
2. Run `./kv_server`
3. Test manually with `curl`
4. Run a load test:

   ```bash
   ./load_generator 127.0.0.1 8080 30 60 0.8 1000
   ```

5. Observe throughput/latency output
6. Verify data in MySQL:

   ```bash
   mysql -u kvuser -pkvpass kvdb -e "SELECT COUNT(*) FROM kv_store;"
   ```

7. Plot graphs using recorded outputs.

---

## Authors / Notes

- Implemented by: Suyash Joshi
- Language: C++17
- Libraries: cpp-httplib, libmysqlclient
- Designed for CS744: DECS (Autumn 2025)

---
