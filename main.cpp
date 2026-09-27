#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
using namespace std;
namespace fs = std::filesystem;

const string REPO_DIR = ".minigit";

void init() {
    if (fs::exists(REPO_DIR)) {
        cout << "Repository already initialized." << endl;
        return;
    }

    // TODO 1: create the .minigit directory
    fs::create_directory(REPO_DIR);

    // TODO 2: create .minigit/objects
    fs::create_directory(REPO_DIR + "/objects");

    // TODO 3: create an empty file .minigit/index
    ofstream indexFile(REPO_DIR + "/index");
    indexFile.close();

    cout << "Initialized empty MiniGit repository." << endl;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "Usage: minigit <command>" << endl;
        return 1;
    }

    string command = argv[1];

    if (command == "init") {
        init();
    } else {
        cout << "Unknown command: " << command << endl;
    }

    return 0;
}