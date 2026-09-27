#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

const string REPO_DIR = ".minigit";

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

void add(const string &filename) {
    if (!fs::exists(REPO_DIR)) {
        cout << "Not a minigit repository. Run 'minigit init' first." << endl;
        return;
    }

    if (!fs::exists(filename)) {
        cout << "File not found: " << filename << endl;
        return;
    }

    // 1. Read the index into a vector<string>
    vector<string> staged;
    ifstream in(REPO_DIR + "/index");

    string line;
    while (getline(in, line)) {
        staged.push_back(line);
    }

    in.close();

    // 2. Check if filename is already staged
    for (const string &file : staged) {
        if (file == filename) {
            cout << "Already staged: " << filename << endl;
            return;
        }
    }

    // 3. Append filename to the index file
    ofstream out(REPO_DIR + "/index", ios::app);
    out << filename << endl;
    out.close();

    cout << "Staged: " << filename << endl;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "Usage: minigit <command>" << endl;
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
    else {
        cout << "Unknown command: " << command << endl;
    }

    return 0;
}