// ConsoleGame.cpp
#include "ConsoleGame.h"
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
#include "Game.h"

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

namespace {
const char *RESET = "\033[0m", *RED = "\033[91m", *GREEN = "\033[92m",
           *YELLOW = "\033[93m", *CYAN = "\033[96m", *BOLD = "\033[1m";

void enableColours() {
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) SetConsoleMode(out, mode | 0x0004);   // ENABLE_VIRTUAL_TERMINAL_PROCESSING
#endif
}

std::string readLine(const std::string &prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) { std::cout << "\n"; std::exit(0); }
    return line;
}
}  // namespace

ConsoleGame::ConsoleGame(WordBank &bank, Scoreboard &board) : bank_(bank), board_(board) {}

// The figure is built from 10 parts: base, pole, beam, rope, head, body, arms, legs.
std::string ConsoleGame::gallows(int parts) {
    std::vector<std::string> g = {
        "             ",
        "             ",
        "             ",
        "             ",
        "             ",
        "             ",
        "             "};
    auto put = [&](int row, int col, char c) { g[row][col] = c; };
    if (parts >= 1) g[6] = " ===========";
    if (parts >= 2) for (int r = 0; r <= 5; ++r) put(r, 2, '|');
    if (parts >= 3) { for (int c = 2; c <= 8; ++c) put(0, c, '-'); put(0, 2, '+'); put(0, 8, '+'); put(1, 3, '/'); }
    if (parts >= 4) put(1, 8, '|');
    if (parts >= 5) put(2, 8, 'O');
    if (parts >= 6) put(3, 8, '|');
    if (parts >= 7) put(3, 7, '/');
    if (parts >= 8) put(3, 9, '\\');
    if (parts >= 9) put(4, 7, '/');
    if (parts >= 10) put(4, 9, '\\');
    std::string out;
    for (const std::string &row : g) out += "   " + row + "\n";
    return out;
}

void ConsoleGame::draw(const Game &game) const {
    const int preDrawn = 10 - game.difficulty().maxLives();
    std::cout << "\n" << (game.livesLeft() <= 2 ? RED : CYAN) << gallows(preDrawn + game.livesLost()) << RESET;
    std::cout << "   Category: " << BOLD << game.category() << RESET
              << "   Difficulty: " << game.difficulty().name() << "\n";
    std::cout << "   Lives: " << RED;
    for (int i = 0; i < game.livesLeft(); ++i) std::cout << "<3 ";
    std::cout << RESET << "  Hints: " << game.hintsLeft();
    if (game.secondsLeft() >= 0) std::cout << "   Time left: " << game.secondsLeft() << " s";
    std::cout << "\n   Wrong: " << RED;
    for (char c : game.wrongLetters()) std::cout << c << ' ';
    std::cout << RESET << "\n";
    if (game.clueVisible()) std::cout << "   Clue: " << YELLOW << game.clue() << RESET << "\n";
    std::cout << "\n      " << BOLD << game << RESET << "\n\n";      // overloaded operator<<
}

std::string ConsoleGame::chooseDifficulty() {
    std::cout << "\n  Difficulty:\n";
    const char *names[] = {"Easy", "Medium", "Hard"};
    for (int i = 0; i < 3; ++i)
        std::cout << "    " << i + 1 << ". " << std::left << std::setw(7) << names[i]
                  << Difficulty::create(names[i])->description() << "\n";
    for (;;) {
        const std::string in = readLine("  Choose 1-3: ");
        if (in == "1" || in == "2" || in == "3") return names[in[0] - '1'];
    }
}

std::string ConsoleGame::chooseCategory() {
    const std::vector<std::string> cats = bank_.categories();
    std::cout << "\n  Category:\n    0. Random\n";
    for (std::size_t i = 0; i < cats.size(); ++i) std::cout << "    " << i + 1 << ". " << cats[i] << "\n";
    for (;;) {
        const std::string in = readLine("  Choose: ");
        try {
            const std::size_t n = std::stoul(in);
            if (n == 0) return "Random";
            if (n <= cats.size()) return cats[n - 1];
        } catch (const std::exception &) {}
    }
}

int ConsoleGame::chooseLength(const std::string &category) {
    const auto counts = bank_.lengthCounts();
    const auto it = counts.find(category);
    std::cout << "\n  Word length:\n    0. Any\n";
    for (int len = WordBank::MIN_LENGTH_OPTION; len <= WordBank::LONG_WORDS; ++len) {
        const int n = (it != counts.end() && it->second.count(len)) ? it->second.at(len) : 0;
        std::cout << "    " << len << ". " << len << (len == WordBank::LONG_WORDS ? "+ letters" : " letters")
                  << "  (" << n << " words)\n";
    }
    for (;;) {
        const std::string in = readLine("  Choose: ");
        try {
            const int n = std::stoi(in);
            if (n == 0) return 0;
            if (n >= WordBank::MIN_LENGTH_OPTION && n <= WordBank::LONG_WORDS) {
                const int available = (it != counts.end() && it->second.count(n)) ? it->second.at(n) : 0;
                if (available > 0) return n;
                std::cout << YELLOW << "  No words of that length in " << category << RESET << "\n";
            }
        } catch (const std::exception &) {}
    }
}

void ConsoleGame::playRound(const std::string &difficulty, const std::string &category, int wordLength) {
    std::unique_ptr<Difficulty> level = Difficulty::create(difficulty);
    const WordEntry entry = bank_.pick(category, *level, wordLength);   // pick first: argument order is unspecified
    Game game(entry, std::move(level));

    std::cout << "\n  Type a letter, a whole word to solve, ? for a letter hint, ! for the clue, 0 to give up.\n";
    while (!game.isOver()) {
        draw(game);
        const std::string in = WordBank::normalize(readLine("  Your guess: "));
        try {
            if (in == "?")      std::cout << GREEN << "  Hint: the word contains " << game.useLetterHint() << RESET << "\n";
            else if (in == "!") std::cout << YELLOW << "  Clue: " << game.useClueHint() << RESET << "\n";
            else if (in == "0") game.forfeit();
            else if (in.size() == 1) {
                const bool ok = game.guess(in[0]) == GuessResult::Correct;
                std::cout << (ok ? GREEN : RED) << (ok ? "  Correct!" : "  Wrong!") << RESET << "\n";
            } else {
                const bool ok = game.guess(in) == GuessResult::Correct;
                if (!ok) std::cout << RED << "  Not the word - that counts as a wrong guess!" << RESET << "\n";
            }
        } catch (const GameOverException &) {            // time ran out
        } catch (const HangmanException &e) {
            std::cout << YELLOW << "  " << e.what() << RESET << "\n";
        }
        game.checkTimer();
    }

    draw(game);
    if (game.status() == GameStatus::Won) {
        const int awarded = player_.recordWin(game.roundScore());
        const int rank = board_.add({player_.name(), awarded, game.difficulty().name(), game.word(), Scoreboard::today()});
        std::cout << GREEN << BOLD << "  YOU SAVED HIM!  " << RESET << "+" << awarded << " points";
        if (rank) std::cout << YELLOW << "   New leaderboard entry: #" << rank << RESET;
        std::cout << "\n";
    } else {
        player_.recordLoss();
        std::cout << RED << BOLD << "  HANGED! (" << game.endReason() << ")" << RESET
                  << "  The word was: " << BOLD << game.word() << RESET << "\n";
    }
    std::cout << "  Total: " << player_.totalScore() << "   Wins: " << player_.wins()
              << "   Losses: " << player_.losses() << "   Streak: " << player_.streak() << "\n";
}

void ConsoleGame::showLeaderboard() const {
    std::cout << "\n  " << BOLD << "TOP 10" << RESET << "\n";
    int rank = 1;
    for (const ScoreEntry &e : board_.entries())
        std::cout << "  " << std::right << std::setw(2) << rank++ << ". " << std::left << std::setw(16) << e.name
                  << std::right << std::setw(6) << e.score << "  " << std::left << std::setw(7) << e.difficulty
                  << e.word << std::right << "\n";
    if (board_.entries().empty()) std::cout << "  No scores yet.\n";
}

void ConsoleGame::run() {
    enableColours();
    std::cout << CYAN << BOLD << "\n  ==== H A N G M A N  (console edition) ====\n" << RESET;
    player_.setName(readLine("  Your name: "));
    for (;;) {
        std::cout << "\n  1. Play   2. Leaderboard   3. Quit\n";
        const std::string choice = readLine("  > ");
        if (choice == "1") {
            const std::string difficulty = chooseDifficulty();
            const std::string category = chooseCategory();
            const int wordLength = chooseLength(category);
            for (std::string again = "y"; again == "y" || again == "Y";) {
                playRound(difficulty, category, wordLength);
                again = readLine("  Next word? (y/n): ");
            }
        } else if (choice == "2") showLeaderboard();
        else if (choice == "3") break;
    }
    std::cout << "  Thanks for playing, " << player_.name() << "!\n";
}
