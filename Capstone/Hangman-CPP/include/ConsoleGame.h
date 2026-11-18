// ConsoleGame.h - text-mode Hangman with ASCII art (same engine as the web version)
#pragma once
#include <string>
#include "Player.h"
#include "Scoreboard.h"
#include "WordBank.h"

class Game;

class ConsoleGame {
public:
    ConsoleGame(WordBank &bank, Scoreboard &board);
    void run();

private:
    void playRound(const std::string &difficulty, const std::string &category, int wordLength);
    void draw(const Game &game) const;
    void showLeaderboard() const;
    std::string chooseDifficulty();
    std::string chooseCategory();
    int chooseLength(const std::string &category);
    static std::string gallows(int partsShown);   // ASCII art for 0..10 parts

    WordBank &bank_;
    Scoreboard &board_;
    Player player_;
};
