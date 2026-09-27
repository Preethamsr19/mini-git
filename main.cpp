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

    // Create .minigit
    fs::create_directory(REPO_DIR);

    // Create .minigit/objects
    fs::create_directory(REPO_DIR + "/objects");

    // Create empty index file
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

    // Read the current staging area
    vector<string> staged = readIndex();

    // Check if file is already staged
    for (const string &file : staged) {
        if (file == filename) {
            cout << "Already staged: " << filename << endl;
            return;
        }
    }

    // Add file to index
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

    // Read staged files
    vector<string> staged = readIndex();

    // Nothing to commit
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

    // Copy every staged file into the commit directory
    for (const string &filename : staged) {

        fs::copy_file(
            filename,
            commitDir + "/" + filename,
            fs::copy_options::overwrite_existing
        );
    }

    // Get current time
    time_t now = time(0);

    string timestamp = ctime(&now);

    // Remove newline from timestamp
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

    // Clear staging area
    // Opening without ios::app truncates the file
    ofstream clearIndex(REPO_DIR + "/index");
    clearIndex.close();

    cout << "Committed as " << id
         << ": " << message << endl;
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

    // UNKNOWN COMMAND
    else {
        cout << "Unknown command: " << command << endl;
    }

    return 0;
}