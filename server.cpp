
#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <thread>

using namespace std;

static const bool USE_XOR = true;
static const unsigned char XOR_KEY = 0xAA;
static const string SHARED_DIR = "shared";
static const string USERS_FILE = "users.txt";

static bool sendAll(int fd, const void* buf, size_t len) {
    const char* p = (const char*)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

static bool recvAll(int fd, void* buf, size_t len) {
    char* p = (char*)buf;
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = recv(fd, p + recvd, len - recvd, 0);
        if (n <= 0) return false;
        recvd += (size_t)n;
    }
    return true;
}

static bool readLine(int fd, string& out) {
    out.clear();
    char c;
    while (true) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) return false;
        if (c == '\n') break;
        out.push_back(c);
        if (out.size() > 1000000) return false;
    }
    return true;
}

static bool sendLine(int fd, const string& s) {
    string line = s + "\n";
    return sendAll(fd, line.data(), line.size());
}

static string trim(const string& s) {
    size_t i = 0, j = s.size();
    while (i < j && isspace((unsigned char)s[i])) ++i;
    while (j > i && isspace((unsigned char)s[j-1])) --j;
    return s.substr(i, j - i);
}

static void xorBuf(vector<char>& buf) {
    if (!USE_XOR) return;
    for (auto& ch : buf) ch ^= XOR_KEY;
}

static unordered_map<string,string> load_users() {
    unordered_map<string,string> m;
    ifstream fin(USERS_FILE);
    string u,p;
    while (fin >> u >> p) m[u]=p;
    return m;
}

static bool is_regular_file(const string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static uint64_t file_size(const string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) return 0;
    return (uint64_t)st.st_size;
}

static void handle_client(int cfd, const sockaddr_in addr) {
    string who = inet_ntoa(addr.sin_addr);
    cerr << "[*] Client connected: " << who << endl;

    auto users = load_users();
    bool authed = false;

    string line;
    while (readLine(cfd, line)) {
        line = trim(line);
        if (line.empty()) continue;

        stringstream ss(line);
        string cmd; ss >> cmd;
        if (cmd == "AUTH") {
            string u, p; ss >> u >> p;
            if (!u.empty() && !p.empty() && users.count(u) && users[u]==p) {
                authed = true;
                sendLine(cfd, "OK Authenticated");
            } else {
                sendLine(cfd, "ERR Invalid credentials");
            }
        } else if (cmd == "LIST") {
            if (!authed) { sendLine(cfd, "ERR Not authenticated"); continue; }
            DIR* d = opendir(SHARED_DIR.c_str());
            if (!d) { sendLine(cfd, "ERR Cannot open shared dir"); continue; }
            struct dirent* ent;
            while ((ent = readdir(d)) != nullptr) {
                if (ent->d_name[0]=='.') continue;
                string path = SHARED_DIR + "/" + ent->d_name;
                if (is_regular_file(path)) {
                    sendLine(cfd, string("FILE ") + ent->d_name + " " + to_string(file_size(path)));
                }
            }
            closedir(d);
            sendLine(cfd, "END");
        } else if (cmd == "GET") {
            if (!authed) { sendLine(cfd, "ERR Not authenticated"); continue; }
            string name; ss >> ws; getline(ss, name);
            name = trim(name);
            if (name.empty()) { sendLine(cfd, "ERR Missing filename"); continue; }
            if (name.find("..") != string::npos || name.find('/') != string::npos) {
                sendLine(cfd, "ERR Invalid filename"); continue;
            }
            string path = SHARED_DIR + "/" + name;
            if (!is_regular_file(path)) { sendLine(cfd, "ERR Not found"); continue; }
            uint64_t sz = file_size(path);
            sendLine(cfd, string("OK ") + to_string(sz));
            ifstream fin(path, ios::binary);
            const size_t CHUNK = 64*1024;
            vector<char> buf; buf.resize(CHUNK);
            uint64_t sentBytes = 0;
            while (fin) {
                fin.read(buf.data(), buf.size());
                streamsize got = fin.gcount();
                if (got <= 0) break;
                buf.resize((size_t)got);
                xorBuf(buf);
                if (!sendAll(cfd, buf.data(), buf.size())) break;
                sentBytes += buf.size();
                buf.resize(CHUNK);
            }
            cerr << "[GET] Sent " << sentBytes << " bytes for " << name << " to " << who << endl;
        } else if (cmd == "PUT") {
            if (!authed) { sendLine(cfd, "ERR Not authenticated"); continue; }
            string name; uint64_t sz;
            ss >> name >> sz;
            if (name.empty()) { sendLine(cfd, "ERR Missing filename"); continue; }
            if (name.find("..") != string::npos || name.find('/') != string::npos) {
                sendLine(cfd, "ERR Invalid filename"); continue;
            }
            string path = SHARED_DIR + "/" + name;
            ofstream fout(path, ios::binary);
            if (!fout) { sendLine(cfd, "ERR Cannot write"); continue; }
            const size_t CHUNK = 64*1024;
            vector<char> buf; buf.resize(CHUNK);
            uint64_t remaining = sz;
            while (remaining > 0) {
                size_t want = (size_t)min<uint64_t>(remaining, buf.size());
                if (!recvAll(cfd, buf.data(), want)) { fout.close(); remove(path.c_str()); close(cfd); return; }
                buf.resize(want);
                xorBuf(buf);
                fout.write(buf.data(), buf.size());
                remaining -= want;
                buf.resize(CHUNK);
            }
            fout.close();
            sendLine(cfd, "OK Stored");
            cerr << "[PUT] Received " << sz << " bytes for " << name << " from " << who << endl;
        } else if (cmd == "QUIT") {
            sendLine(cfd, "BYE");
            break;
        } else {
            sendLine(cfd, "ERR Unknown command");
        }
    }

    cerr << "[*] Client disconnected: " << who << endl;
    close(cfd);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <bind-ip> <port>\n";
        return 1;
    }
    string ip = argv[1];
    int port = stoi(argv[2]);

    mkdir(SHARED_DIR.c_str(), 0755);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }

    int opt=1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        cerr << "Invalid bind IP\n"; return 1;
    }

    if (bind(sfd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(sfd, 16) < 0) { perror("listen"); return 1; }

    cerr << "[*] Listening on " << ip << ":" << port << endl;

    while (true) {
        sockaddr_in caddr{}; socklen_t clen = sizeof(caddr);
        int cfd = accept(sfd, (sockaddr*)&caddr, &clen);
        if (cfd < 0) { perror("accept"); continue; }
        std::thread(handle_client, cfd, caddr).detach();
    }
    close(sfd);
    return 0;
}
