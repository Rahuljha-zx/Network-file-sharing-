# Network File Sharing — C++ (Linux, TCP)

A multithreaded client–server file sharing system written in C++17 using POSIX sockets.
Supports login, list, download (GET), upload (PUT), and graceful quit with a simple text protocol and size-prefixed file transfers.
Basic XOR "encryption" is included as a learning aid (can be toggled).

> This project aligns with Assignment 4 — Network File Sharing (Server & Client) from your capstone PDF.

---

## Features
- Multi-client server (thread per client)
- Username/password authentication (from users.txt)
- Commands: AUTH, LIST, GET <file>, PUT <file> <size>, QUIT
- Robust framing: line-based control + exact byte transfers for files
- Simple XOR transformation for file payloads (optional)
- Clean, readable C++17 with utilities for sendAll, recvAll, readLine

---

## Quickstart

### 1) Build
$ make

### 2) Prepare users and shared folder
# Add users (format: username password)
$ cat users.txt

# Put some files to share
$ ls shared/

### 3) Run
Terminal 1 (server):
$ ./server 0.0.0.0 8080

Terminal 2 (client):
$ ./client 127.0.0.1 8080

### 4) Client usage example
> AUTH user1 password1
OK Authenticated
> LIST
FILE sample1.txt 31
END
> GET sample1.txt
OK 31
# receives 31 bytes and saves to downloads/sample1.txt
> PUT notes.txt 123
OK Stored
> QUIT
BYE

---

## Protocol (Human-Readable Control, Binary Files)

- Lines end with \n
- Server replies start with either OK, ERR, or domain lines like FILE

### AUTH
C: AUTH <username> <password>\n
S: OK Authenticated\n
or
S: ERR Invalid credentials\n

### LIST
C: LIST\n
S: FILE <name> <size>\n  (repeat for each regular file)
S: END\n

### GET
C: GET <name>\n
S: OK <size>\n
S: <size bytes of payload>
or
S: ERR Not found\n

### PUT
C: PUT <name> <size>\n
C: <size bytes of payload>
S: OK Stored\n

### QUIT
C: QUIT\n
S: BYE\n

---

## Project Structure
.
├── Makefile
├── README.md
├── LICENSE
├── .gitignore
├── server.cpp
├── client.cpp
├── users.txt
├── shared/
│   └── sample1.txt
└── docs/
    └── report.md

---

## Notes
- Tested on Linux (g++ >= 9). Uses <sys/socket.h>, <netinet/in.h>, <arpa/inet.h>, <unistd.h>, and <dirent.h>.
- XOR transformation is only for learning. For production, integrate OpenSSL and TLS.
- Error handling is pragmatic; feel free to extend with structured logging.

---

## License
MIT — see LICENSE.
