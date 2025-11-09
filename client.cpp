
#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace std;

static const bool USE_XOR = true;
static const unsigned char XOR_KEY = 0xAA;

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

static void xorBuf(vector<char>& buf) {
    if (!USE_XOR) return;
    for (auto& ch : buf) ch ^= XOR_KEY;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <server-ip> <port>\n";
        return 1;
    }
    string ip = argv[1];
    int port = stoi(argv[2]);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        cerr << "Invalid server IP\n"; return 1;
    }

    if (connect(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }
    cerr << "[*] Connected to " << ip << ":" << port << endl;

    system("mkdir -p downloads");

    string line;
    while (true) {
        cout << "> ";
        string cmd;
        if (!getline(cin, cmd)) break;

        if (!sendLine(s, cmd)) { cerr << "Disconnected\n"; break; }

        {
            stringstream ss(cmd);
            string op; ss >> op;
            if (op == "PUT") {
                string name; uint64_t sz;
                ss >> name >> sz;
                if (name.empty()) { cerr << "Usage: PUT <name> <size>\n"; continue; }
                ifstream fin(name, ios::binary);
                if (!fin) {
                    cerr << "Cannot open local file for PUT: " << name << endl;
                    continue;
                }
                const size_t CHUNK = 64*1024;
                vector<char> buf; buf.resize(CHUNK);
                uint64_t remaining = sz;
                while (remaining > 0) {
                    size_t want = (size_t)min<uint64_t>(remaining, buf.size());
                    fin.read(buf.data(), want);
                    streamsize got = fin.gcount();
                    if ((uint64_t)got != want) { cerr << "Local file shorter than size\n"; break; }
                    buf.resize(want);
                    xorBuf(buf);
                    if (!sendAll(s, buf.data(), buf.size())) { cerr << "Disconnected\n"; break; }
                    remaining -= want;
                    buf.resize(CHUNK);
                }
            }
        }

        string resp;
        if (!readLine(s, resp)) { cerr << "Disconnected\n"; break; }
        cout << resp << "\n";

        stringstream rs(resp);
        string tag; rs >> tag;
        if (tag == "FILE" || tag == "END" || tag == "ERR") {
            if (tag == "FILE") {
                while (true) {
                    string r2;
                    if (!readLine(s, r2)) { cerr << "Disconnected\n"; break; }
                    cout << r2 << "\n";
                    if (r2 == "END") break;
                }
            }
        } else if (tag == "OK") {
            string maybeSize; rs >> maybeSize;
            if (!maybeSize.empty() && all_of(maybeSize.begin(), maybeSize.end(), ::isdigit)) {
                uint64_t sz = stoull(maybeSize);
                string fname;
                {
                    stringstream cs(cmd);
                    string op; cs >> op; cs >> fname;
                }
                if (fname.empty()) fname = "download.bin";
                string outpath = "downloads/" + fname;
                ofstream fout(outpath, ios::binary);
                if (!fout) { cerr << "Cannot open output file\n"; break; }
                const size_t CHUNK = 64*1024;
                vector<char> buf; buf.resize(CHUNK);
                uint64_t remaining = sz;
                while (remaining > 0) {
                    size_t want = (size_t)min<uint64_t>(remaining, buf.size());
                    if (!recvAll(s, buf.data(), want)) { cerr << "Disconnected\n"; break; }
                    buf.resize(want);
                    xorBuf(buf);
                    fout.write(buf.data(), buf.size());
                    remaining -= want;
                    buf.resize(CHUNK);
                }
                fout.close();
                cout << "Saved to " << outpath << " (" << sz << " bytes)\n";
            }
        } else if (tag == "BYE") {
            break;
        }
    }

    close(s);
    return 0;
}
