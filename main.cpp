#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <map>
#include <ctime>
#include <cstdio>

using namespace std;
namespace fs = std::filesystem;

const string REPO_DIR = ".minigit";

// ==================================================
// SHA-1
// --------------------------------------------------
// Content addressing is the core idea of Git: an object
// is named by the hash of its own contents. Two identical
// files therefore produce the same name and get stored
// exactly once, no matter how many commits reference them.
// ==================================================

typedef unsigned int uint32;

string sha1(const string &data) {
    uint32 h0 = 0x67452301;
    uint32 h1 = 0xEFCDAB89;
    uint32 h2 = 0x98BADCFE;
    uint32 h3 = 0x10325476;
    uint32 h4 = 0xC3D2E1F0;

    // Padding: append 0x80, pad with zeros until length % 64 == 56,
    // then append the original length in bits as a 64-bit big-endian value.
    string msg = data;
    unsigned long long bitLength = (unsigned long long)data.size() * 8ULL;

    msg += (char)0x80;

    while (msg.size() % 64 != 56) {
        msg += (char)0x00;
    }

    for (int i = 7; i >= 0; i--) {
        msg += (char)((bitLength >> (i * 8)) & 0xFF);
    }

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {

        uint32 w[80];

        for (int i = 0; i < 16; i++) {
            size_t p = chunk + i * 4;

            w[i] = ((uint32)(unsigned char)msg[p]     << 24) |
                   ((uint32)(unsigned char)msg[p + 1] << 16) |
                   ((uint32)(unsigned char)msg[p + 2] << 8)  |
                   ((uint32)(unsigned char)msg[p + 3]);
        }

        for (int i = 16; i < 80; i++) {
            uint32 v = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (v << 1) | (v >> 31);
        }

        uint32 a = h0, b = h1, c = h2, d = h3, e = h4;

        for (int i = 0; i < 80; i++) {

            uint32 f, k;

            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            }
            else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32 temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];

            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    char buffer[41];

    snprintf(buffer, sizeof(buffer), "%08x%08x%08x%08x%08x",
             h0, h1, h2, h3, h4);

    return string(buffer);
}

// ==================================================
// FILE HELPERS
// ==================================================

bool repoExists() {
    if (!fs::exists(REPO_DIR)) {
        cout << "Not a minigit repository. Run 'minigit init' first." << endl;
        return false;
    }
    return true;
}

string readFileBytes(const string &path) {
    ifstream in(path, ios::binary);

    ostringstream contents;
    contents << in.rdbuf();

    in.close();

    return contents.str();
}

void writeFileBytes(const string &path, const string &data) {
    fs::path target(path);

    if (target.has_parent_path() && !target.parent_path().empty()) {
        fs::create_directories(target.parent_path());
    }

    ofstream out(path, ios::binary);
    out.write(data.data(), (streamsize)data.size());
    out.close();
}

// ==================================================
// OBJECT STORE
// --------------------------------------------------
// Objects live at .minigit/objects/<first 2 chars>/<rest>.
// Splitting on the first two characters keeps any single
// directory from holding tens of thousands of entries,
// which is the same fan-out real Git uses.
// ==================================================

string objectPath(const string &hash) {
    return REPO_DIR + "/objects/" + hash.substr(0, 2) + "/" + hash.substr(2);
}

string writeObject(const string &content) {
    string hash = sha1(content);
    string path = objectPath(hash);

    // Deduplication: if this exact content is already stored,
    // there is nothing to write.
    if (!fs::exists(path)) {
        writeFileBytes(path, content);
    }

    return hash;
}

string readObject(const string &hash) {
    return readFileBytes(objectPath(hash));
}

// ==================================================
// HEAD
// ==================================================

string readHead() {
    string path = REPO_DIR + "/HEAD";

    if (!fs::exists(path)) {
        return "";
    }

    ifstream in(path);

    string head;
    getline(in, head);

    in.close();

    return head;
}

void writeHead(const string &commitId) {
    ofstream out(REPO_DIR + "/HEAD");
    out << commitId << endl;
    out.close();
}

// ==================================================
// INDEX (staging area)
// --------------------------------------------------
// Each line is "<blob hash> <path>".
// ==================================================

map<string, string> readIndex() {
    map<string, string> staged;

    ifstream in(REPO_DIR + "/index");

    string line;

    while (getline(in, line)) {

        if (line.empty()) {
            continue;
        }

        size_t space = line.find(' ');

        if (space == string::npos) {
            continue;
        }

        string hash = line.substr(0, space);
        string path = line.substr(space + 1);

        staged[path] = hash;
    }

    in.close();

    return staged;
}

void writeIndex(const map<string, string> &staged) {
    ofstream out(REPO_DIR + "/index");

    for (const auto &entry : staged) {
        out << entry.second << " " << entry.first << endl;
    }

    out.close();
}

void clearIndex() {
    ofstream out(REPO_DIR + "/index");
    out.close();
}

// ==================================================
// COMMIT OBJECTS
// --------------------------------------------------
// A commit is itself an object in the store. Its content is:
//
//   parent <commit id or "none">
//   timestamp <...>
//   message <...>
//   file <blob hash> <path>
//   file <blob hash> <path>
//
// The parent line is what turns a pile of commits into a
// history you can walk backwards.
// ==================================================

struct Commit {
    string id;
    string parent;
    string timestamp;
    string message;
    map<string, string> files;   // path -> blob hash
    bool valid = false;
};

Commit readCommit(const string &commitId) {
    Commit commit;

    if (commitId.empty() || !fs::exists(objectPath(commitId))) {
        return commit;
    }

    commit.id = commitId;

    istringstream in(readObject(commitId));

    string line;

    while (getline(in, line)) {

        if (line.rfind("parent ", 0) == 0) {
            string parent = line.substr(7);
            commit.parent = (parent == "none") ? "" : parent;
        }
        else if (line.rfind("timestamp ", 0) == 0) {
            commit.timestamp = line.substr(10);
        }
        else if (line.rfind("message ", 0) == 0) {
            commit.message = line.substr(8);
        }
        else if (line.rfind("file ", 0) == 0) {
            string rest = line.substr(5);

            size_t space = rest.find(' ');

            if (space != string::npos) {
                string hash = rest.substr(0, space);
                string path = rest.substr(space + 1);
                commit.files[path] = hash;
            }
        }
    }

    commit.valid = true;

    return commit;
}

map<string, string> headFiles() {
    Commit head = readCommit(readHead());

    if (!head.valid) {
        return map<string, string>();
    }

    return head.files;
}

// Allow a short commit id, the way "git checkout a1b2c3" works.
string resolveCommit(const string &prefix) {
    string objectsDir = REPO_DIR + "/objects";

    if (!fs::exists(objectsDir)) {
        return "";
    }

    if (prefix.size() == 40 && fs::exists(objectPath(prefix))) {
        return prefix;
    }

    if (prefix.size() < 2) {
        return "";
    }

    vector<string> matches;

    for (const auto &dir : fs::directory_iterator(objectsDir)) {

        if (!dir.is_directory()) {
            continue;
        }

        string prefixDir = dir.path().filename().string();

        for (const auto &file : fs::directory_iterator(dir.path())) {

            string hash = prefixDir + file.path().filename().string();

            if (hash.rfind(prefix, 0) == 0) {
                matches.push_back(hash);
            }
        }
    }

    if (matches.size() == 1) {
        return matches[0];
    }

    if (matches.size() > 1) {
        cout << "Ambiguous commit id: " << prefix << endl;
    }

    return "";
}

// ==================================================
// INIT
// ==================================================

void init() {
    if (fs::exists(REPO_DIR)) {
        cout << "Repository already initialized." << endl;
        return;
    }

    fs::create_directory(REPO_DIR);
    fs::create_directory(REPO_DIR + "/objects");

    clearIndex();
    writeHead("");

    cout << "Initialized empty MiniGit repository." << endl;
}

// ==================================================
// ADD
// ==================================================

void add(const string &filename) {
    if (!repoExists()) {
        return;
    }

    if (!fs::exists(filename) || fs::is_directory(filename)) {
        cout << "File not found: " << filename << endl;
        return;
    }

    string contents = readFileBytes(filename);
    string hash = writeObject(contents);

    map<string, string> staged = readIndex();

    // Re-staging an edited file updates its hash instead of
    // being rejected.
    staged[filename] = hash;

    writeIndex(staged);

    cout << "Staged: " << filename << " (" << hash.substr(0, 7) << ")" << endl;
}

// ==================================================
// STATUS
// ==================================================

void status() {
    if (!repoExists()) {
        return;
    }

    map<string, string> staged = readIndex();
    map<string, string> tracked = headFiles();

    vector<string> stagedList;
    vector<string> modifiedList;
    vector<string> deletedList;

    // Every path we know about, from either the last commit
    // or the staging area.
    map<string, bool> paths;

    for (const auto &entry : tracked) {
        paths[entry.first] = true;
    }

    for (const auto &entry : staged) {
        paths[entry.first] = true;
    }

    for (const auto &entry : paths) {

        string path = entry.first;

        bool isStaged = staged.count(path) > 0;
        bool isTracked = tracked.count(path) > 0;

        if (isStaged && (!isTracked || tracked[path] != staged[path])) {
            stagedList.push_back(path);
        }

        if (!fs::exists(path)) {
            deletedList.push_back(path);
            continue;
        }

        // Compare the working file against whatever version is
        // currently recorded for it.
        string recorded = isStaged ? staged[path] : tracked[path];
        string working = sha1(readFileBytes(path));

        if (working != recorded) {
            modifiedList.push_back(path);
        }
    }

    if (stagedList.empty() && modifiedList.empty() && deletedList.empty()) {
        cout << "Nothing to commit, working tree clean." << endl;
        return;
    }

    if (!stagedList.empty()) {
        cout << "Staged for commit:" << endl;

        for (const string &path : stagedList) {
            cout << "  " << path << endl;
        }
    }

    if (!modifiedList.empty()) {
        cout << "Modified (not staged):" << endl;

        for (const string &path : modifiedList) {
            cout << "  " << path << endl;
        }
    }

    if (!deletedList.empty()) {
        cout << "Missing from working directory:" << endl;

        for (const string &path : deletedList) {
            cout << "  " << path << endl;
        }
    }
}

// ==================================================
// COMMIT
// ==================================================

void commit(const string &message) {
    if (!repoExists()) {
        return;
    }

    map<string, string> staged = readIndex();

    if (staged.empty()) {
        cout << "Nothing to commit." << endl;
        return;
    }

    string parent = readHead();

    // A commit is a full snapshot: start from the parent's file
    // list and overwrite the entries that were staged. Files that
    // were not touched this time stay in the snapshot, pointing at
    // the same blob as before.
    map<string, string> snapshot = headFiles();

    for (const auto &entry : staged) {
        snapshot[entry.first] = entry.second;
    }

    time_t now = time(0);
    string timestamp = ctime(&now);

    if (!timestamp.empty() && timestamp.back() == '\n') {
        timestamp.pop_back();
    }

    ostringstream content;

    content << "parent " << (parent.empty() ? "none" : parent) << "\n";
    content << "timestamp " << timestamp << "\n";
    content << "message " << message << "\n";

    for (const auto &entry : snapshot) {
        content << "file " << entry.second << " " << entry.first << "\n";
    }

    string commitId = writeObject(content.str());

    writeHead(commitId);
    clearIndex();

    cout << "Committed " << commitId.substr(0, 7)
         << ": " << message << endl;
    cout << snapshot.size() << " file(s) in this snapshot." << endl;
}

// ==================================================
// LOG
// --------------------------------------------------
// Walks the parent chain from HEAD backwards.
// ==================================================

void log() {
    if (!repoExists()) {
        return;
    }

    string current = readHead();

    if (current.empty()) {
        cout << "No commits yet." << endl;
        return;
    }

    while (!current.empty()) {

        Commit commit = readCommit(current);

        if (!commit.valid) {
            break;
        }

        cout << "commit " << commit.id << endl;
        cout << "Date:    " << commit.timestamp << endl;
        cout << "Files:   " << commit.files.size() << endl;
        cout << endl;
        cout << "    " << commit.message << endl;
        cout << endl;

        current = commit.parent;
    }
}

// ==================================================
// CHECKOUT
// --------------------------------------------------
// Restores the working directory to a past snapshot.
// Without this, the repository is write-only.
// ==================================================

void checkout(const string &target) {
    if (!repoExists()) {
        return;
    }

    string commitId = resolveCommit(target);

    if (commitId.empty()) {
        cout << "Commit not found: " << target << endl;
        return;
    }

    Commit commit = readCommit(commitId);

    if (!commit.valid) {
        cout << "Commit not found: " << target << endl;
        return;
    }

    for (const auto &entry : commit.files) {

        string path = entry.first;
        string hash = entry.second;

        if (!fs::exists(objectPath(hash))) {
            cout << "Missing object for " << path << endl;
            continue;
        }

        writeFileBytes(path, readObject(hash));
    }

    writeHead(commitId);
    clearIndex();

    cout << "Restored " << commit.files.size()
         << " file(s) from commit " << commitId.substr(0, 7) << endl;
}

// ==================================================
// MAIN
// ==================================================

int main(int argc, char *argv[]) {

    if (argc < 2) {
        cout << "Usage: minigit <command>" << endl;
        cout << endl;
        cout << "  init                  Create a repository" << endl;
        cout << "  add <file>            Stage a file" << endl;
        cout << "  status                Show staged and modified files" << endl;
        cout << "  commit <message>      Record a snapshot" << endl;
        cout << "  log                   Show commit history" << endl;
        cout << "  checkout <commit>     Restore a past snapshot" << endl;
        return 1;
    }

    string command = argv[1];

    if (command == "init") {
        init();
    }

    else if (command == "add") {

        if (argc < 3) {
            cout << "Usage: minigit add <filename>" << endl;
            return 1;
        }

        add(argv[2]);
    }

    else if (command == "status") {
        status();
    }

    else if (command == "commit") {

        if (argc < 3) {
            cout << "Usage: minigit commit <message>" << endl;
            return 1;
        }

        commit(argv[2]);
    }

    else if (command == "log") {
        log();
    }

    else if (command == "checkout") {

        if (argc < 3) {
            cout << "Usage: minigit checkout <commit>" << endl;
            return 1;
        }

        checkout(argv[2]);
    }

    else {
        cout << "Unknown command: " << command << endl;
    }

    return 0;
}
