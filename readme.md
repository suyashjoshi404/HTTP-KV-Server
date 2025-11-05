# CS744 Project: HTTP-based Key-Value Server (C++)

## Overview

This project implements a multi-tier HTTP key-value server with:

- A multi-threaded C++ HTTP server using cpp-httplib
- An in-memory LRU cache
- A persistent MySQL database
- A load generator for performance testing

It supports POST (create/update), GET (read), and DELETE operations on key-value pairs using REST APIs.

---

## System Requirements

- Ubuntu 20.04+ with CMake ≥ 3.10
- C++17
- MySQL Server

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
cd HTTP-KV-Server
mkdir build && cd build
cmake ..
make -j
```

This will then generate two executables 1) kv_server and 2) load_generator

---

## Running the Server

Run the server (default port 8080):

```bash
./kv_server
# Expected output: Starting server at 0.0.0.0:8080
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
mysql -u kvuser -pkvpass kvdb -e "SELECT * FROM kv_store LIMIT 10;"
```

---

## Running the Load Generator

```
./load_generator <server_host> <server_port> <clients> <duration_seconds> <read_ratio> [key_space] [think_ms]
```

**Parameters:**

| Parameter        | Meaning                               |
| ---------------- | ------------------------------------- |
| server_host      | IP or hostname (e.g. 127.0.0.1)       |
| server_port      | Server port (default 8080)            |
| clients          | Number of client threads              |
| duration_seconds | Test duration                         |
| read_ratio       | Fraction of read requests (0.0 - 1.0) |
| key_space        | Distinct key range (default 1000)     |
| think_ms         | Optional think time between requests  |

---

### Example

#### 1. CPU-bound cache workload (Get Popular)

```bash
./load_generator 127.0.0.1 8080 20 60 1.0 10 0
```

#### 2. Disk-bound workload (Put All)

```bash
./load_generator 127.0.0.1 8080 20 60 0.0 10000 0
```

#### 3. Mixed workload

```bash
./load_generator 127.0.0.1 8080 50 120 0.8 5000 0
```

Expected output:

```
RESULTS:
 Total time (s): 60.002
 Clients: 20
 Requests: 120000  Success: 119995  Fail: 5
 Throughput (req/s): 1999.8
 Avg latency (ms): 2.34
```

---

## SQL Verification Steps

You can also verify persistence by doing:

```sql
SHOW TABLES;
SELECT * FROM kv_store;
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

## Example End-to-End Run

1. Start MySQL (`sudo systemctl start mysql`)
2. Run `./kv_server`
3. Test manually with `curl`
4. Run a load test:

   ```bash
   ./load_generator 127.0.0.1 8080 30 60 0.8 1000
   ```

5. Verify data in MySQL:

   ```bash
   mysql -u kvuser -pkvpass kvdb -e "SELECT COUNT(*) FROM kv_store;"
   ```

---
