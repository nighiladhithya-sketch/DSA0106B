// WordBank.h - loads categorised words + clues and picks a fresh word for each round
#pragma once
#include <deque>
#include <map>
#include <random>
#include <string>
#include <vector>
#include "Difficulty.h"

struct WordEntry {
    std::string word;      // upper case, letters and single spaces only
    std::string category;
    std::string clue;
};

class WordBank {
public:
    WordBank();                                        // starts with a small built-in list
    void loadFromFile(const std::string &path);        // throws WordBankException
    // wordLength: 0 = any (difficulty decides), 3..8 = exactly that many letters, 9 = nine or more
    WordEntry pick(const std::string &category, const Difficulty &level, int wordLength = 0);

    std::vector<std::string> categories() const;
    std::size_t size() const { return words_.size(); }
    std::map<std::string, std::map<int, int>> lengthCounts() const;   // category -> length -> words

    static constexpr int MIN_LENGTH_OPTION = 3;
    static constexpr int LONG_WORDS = 9;                    // the "9+" option
    static int lengthBucket(const std::string &word);

    static std::size_t letterCount(const std::string &word);
    static std::string normalize(const std::string &text);   // upper case, trimmed, single spaces
    static bool isValidWord(const std::string &word);
    static std::mt19937 &rng();                        // shared random engine

private:
    std::vector<WordEntry> words_;
    std::deque<std::string> recent_;                   // avoids repeating recent words
    static const std::size_t RECENT_LIMIT = 15;
};
