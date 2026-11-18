// Game.h - one round of Hangman: the core rules engine
#pragma once
#include <chrono>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <vector>
#include "Difficulty.h"
#include "WordBank.h"

enum class GameStatus { Playing, Won, Lost };
enum class GuessResult { Correct, Wrong };

class Game {
public:
    Game(const WordEntry &entry, std::unique_ptr<Difficulty> level, bool customWord = false);
    Game(const Game &) = delete;                       // a round owns its Difficulty object
    Game &operator=(const Game &) = delete;

    // Function overloading: guess one letter, or try to solve the whole word.
    GuessResult guess(char letter);
    GuessResult guess(const std::string &wholeWord);

    char useLetterHint();                  // reveals one hidden letter, costs a hint
    const std::string &useClueHint();      // shows the clue, costs a hint (free on Easy)
    void forfeit();
    void checkTimer();                     // ends the round when the time limit passes

    // ---- read-only state ----
    std::string maskedWord() const;        // e.g. "C _ _ _ _ _"  (no spaces between letters)
    const std::string &word() const { return word_; }
    const std::string &category() const { return category_; }
    const Difficulty &difficulty() const { return *difficulty_; }
    const std::set<char> &correctLetters() const { return correct_; }
    const std::set<char> &wrongLetters() const { return wrong_; }
    const std::vector<std::string> &wrongWords() const { return wrongWords_; }
    GameStatus status() const { return status_; }
    const std::string &endReason() const { return endReason_; }
    bool isOver() const { return status_ != GameStatus::Playing; }
    bool isCustomWord() const { return custom_; }
    bool clueVisible() const { return clueShown_; }
    bool hasClue() const { return !clue_.empty(); }
    const std::string &clue() const { return clue_; }
    int livesLost() const { return livesLost_; }
    int livesLeft() const { return difficulty_->maxLives() - livesLost_; }
    int hintsLeft() const { return hintsLeft_; }
    int hintsUsed() const { return hintsUsed_; }
    int secondsLeft() const;               // -1 when untimed
    int elapsedSeconds() const;
    int roundScore() const;                // 0 unless the round was won

    static int gamesCreated() { return gamesCreated_; }

    friend std::ostream &operator<<(std::ostream &out, const Game &game);

private:
    using Clock = std::chrono::steady_clock;

    void ensurePlaying();
    void finish(GameStatus result, const std::string &reason);
    bool isSolved() const;

    std::string word_, category_, clue_;
    std::unique_ptr<Difficulty> difficulty_;
    std::set<char> correct_, wrong_;
    std::vector<std::string> wrongWords_;
    int livesLost_ = 0;
    int hintsLeft_ = 0;
    int hintsUsed_ = 0;
    int timeLeftAtEnd_ = 0;
    bool clueShown_ = false;
    bool custom_ = false;
    GameStatus status_ = GameStatus::Playing;
    std::string endReason_;
    Clock::time_point start_, end_;

    static int gamesCreated_;              // static data member shared by all rounds
};
