// main.cpp - Hangman (C++ edition)
//   hangman            -> starts the web version and opens the browser
//   hangman --console  -> plays in the terminal
//   options: --port N  --no-browser
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "ConsoleGame.h"
#include "HttpServer.h"
#include "Scoreboard.h"
#include "WebApp.h"
#include "WordBank.h"

// Finds a project folder whether the program is run from the project root or from build/.
static std::string findFolder(const std::string &name, const std::string &markerFile, const std::string &exePath) {
    std::string exeDir = ".";
    const std::size_t slash = exePath.find_last_of("/\\");
    if (slash != std::string::npos) exeDir = exePath.substr(0, slash);
    const std::vector<std::string> candidates = {name, "../" + name, exeDir + "/" + name, exeDir + "/../" + name};
    for (const std::string &dir : candidates)
        if (std::ifstream(dir + "/" + markerFile)) return dir;
    return name;
}

static void openBrowser(const std::string &url) {
    int result = 0;
#if defined(_WIN32)
    result = std::system(("start \"\" \"" + url + "\"").c_str());
#elif defined(__APPLE__)
    result = std::system(("open \"" + url + "\"").c_str());
#else
    result = std::system(("xdg-open \"" + url + "\" >/dev/null 2>&1 &").c_str());
#endif
    if (result != 0) std::cout << "  Open this address in your browser: " << url << "\n";
}

int main(int argc, char *argv[]) {
    bool console = false, browser = true;
    int port = 8080;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--console" || arg == "-c") console = true;
        else if (arg == "--no-browser") browser = false;
        else if (arg == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: hangman [--console] [--port N] [--no-browser]\n";
            return 0;
        }
    }

    const std::string dataDir = findFolder("data", "words.txt", argv[0]);
    WordBank bank;
    try {
        bank.loadFromFile(dataDir + "/words.txt");
    } catch (const WordBankException &e) {
        std::cerr << "Warning: " << e.what() << " - using the built-in word list.\n";
    }
    Scoreboard board(dataDir + "/scores.txt");

    if (console) {
        ConsoleGame(bank, board).run();
        return 0;
    }

    try {
        HttpServer server;
        const int actualPort = server.listen(port);
        const std::string url = "http://localhost:" + std::to_string(actualPort);
        WebApp app(bank, board, findFolder("web", "index.html", argv[0]));

        std::cout << "\n  HANGMAN - C++ Edition\n"
                  << "  " << bank.size() << " words in " << bank.categories().size() << " categories\n"
                  << "  Game running at: " << url << "\n"
                  << "  (Press Ctrl+C to stop the server)\n" << std::endl;
        if (browser) openBrowser(url);
        server.serve([&app](const HttpRequest &req) { return app.handle(req); });
    } catch (const std::exception &e) {
        std::cerr << "Server error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
