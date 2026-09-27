#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include <ctime>

using namespace std;
namespace fs = std::filesystem;

const string REPO_DIR = ".minigit";

// --------------------------------------------------
// INIT
// --------------------------------------------------

void init() {
    if (fs::exists(REPO_DIR)) {
        cout << "Repository already initialized." << endl;
        return;
    }

    fs::create_directory(REPO_DIR);
    fs::create_directory(REPO_DIR + "/objects");

    ofstream indexFile(REPO_DIR + "/index");
    indexFile.close();

    cout << "Initialized empty MiniGit repository." << endl;
}

// --------------------------------------------------
// READ INDEX
// --------------------------------------------------

vector<string> readIndex() {
    vector<string> lines;

    ifstream in(REPO_DIR + "/index");

    string line;

    while (getline(in, line)) {
        lines.push_back(line);
    }

    in.close();

    return lines;
}

// --------------------------------------------------
// ADD
// --------------------------------------------------

void add(const string &filename) {
    if (!fs::exists(REPO_DIR)) {
        cout << "Not a minigit repository. Run 'minigit init' first." << endl;
        return;
    }

    if (!fs::exists(filename)) {
        cout << "File not found: " << filename << endl;
        return;
    }

    vector<string> staged = readIndex();

    for (const string &file : staged) {
        if (file == filename) {
            cout << "Already staged: " << filename << endl;
            return;
        }
    }

    ofstream out(REPO_DIR + "/index", ios::app);

    out << filename << endl;

    out.close();

    cout << "Staged: " << filename << endl;
}

// --------------------------------------------------
// STATUS
// --------------------------------------------------

void status() {
    if (!fs::exists(REPO_DIR)) {
        cout << "Not a minigit repository. Run 'minigit init' first." << endl;
        return;
    }

    vector<string> staged = readIndex();

    if (staged.empty()) {
        cout << "No files staged for commit." << endl;
        return;
    }

    cout << "Staged files:" << endl;

    for (const string &file : staged) {
        cout << "  " << file << endl;
    }
}

// --------------------------------------------------
// COMMIT
// --------------------------------------------------

void commit(const string &message) {
    if (!fs::exists(REPO_DIR)) {
        cout << "Not a minigit repository. Run 'minigit init' first." << endl;
        return;
    }

    vector<string> staged = readIndex();

    if (staged.empty()) {
        cout << "Nothing to commit." << endl;
        return;
    }

    // Find the next commit ID
    int count = 0;

    for (const auto &entry :
         fs::directory_iterator(REPO_DIR + "/objects")) {
        count++;
    }

    int id = count + 1;

    // Create commit directory
    string commitDir =
        REPO_DIR + "/objects/" + to_string(id);

    fs::create_directory(commitDir);

    // Copy staged files
    for (const string &filename : staged) {

        fs::copy_file(
            filename,
            commitDir + "/" + filename,
            fs::copy_options::overwrite_existing
        );
    }

    // Get timestamp
    time_t now = time(0);

    string timestamp = ctime(&now);

    if (!timestamp.empty() &&
        timestamp.back() == '\n') {
        timestamp.pop_back();
    }

    // Create metadata file
    ofstream metadata(commitDir + "/metadata.txt");

    metadata << "ID: " << id << endl;
    metadata << "Message: " << message << endl;
    metadata << "Timestamp: " << timestamp << endl;

    metadata.close();

    // Clear index
    ofstream clearIndex(REPO_DIR + "/index");
    clearIndex.close();

    cout << "Committed as " << id
         << ": " << message << endl;
}

// --------------------------------------------------
// LOG
// --------------------------------------------------

void log() {
    if (!fs::exists(REPO_DIR)) {
        cout << "Not a minigit repository. Run 'minigit init' first." << endl;
        return;
    }

    string objectsDir = REPO_DIR + "/objects";

    // Count the number of commits
    int count = 0;

    for (const auto &entry :
         fs::directory_iterator(objectsDir)) {

        if (fs::is_directory(entry.path())) {
            count++;
        }
    }

    // No commits
    if (count == 0) {
        cout << "No commits yet." << endl;
        return;
    }

    // Print newest commit first
    for (int id = count; id >= 1; id--) {

        string metadataPath =
            objectsDir + "/" + to_string(id) + "/metadata.txt";

        ifstream metadata(metadataPath);

        if (!metadata) {
            continue;
        }

        string line;

        while (getline(metadata, line)) {
            cout << line << endl;
        }

        metadata.close();

        cout << endl;
    }
}

// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main(int argc, char *argv[]) {

    if (argc < 2) {
        cout << "Usage: minigit <command>" << endl;
        return 1;
    }

    string command = argv[1];

    // INIT
    if (command == "init") {
        init();
    }

    // ADD
    else if (command == "add") {

        if (argc < 3) {
            cout << "Usage: minigit add <filename>" << endl;
            return 1;
        }

        add(argv[2]);
    }

    // STATUS
    else if (command == "status") {
        status();
    }

    // COMMIT
    else if (command == "commit") {

        if (argc < 3) {
            cout << "Usage: minigit commit <message>" << endl;
            return 1;
        }

        commit(argv[2]);
    }

    // LOG
    else if (command == "log") {
        log();
    }

    // UNKNOWN COMMAND
    else {
        cout << "Unknown command: " << command << endl;
    }

    return 0;
}