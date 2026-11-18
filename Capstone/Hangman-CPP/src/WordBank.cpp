// WordBank.cpp
#include "WordBank.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

std::mt19937 &WordBank::rng() {
    static std::mt19937 engine(std::random_device{}());
    return engine;
}

WordBank::WordBank() {
    // Fallback list, used only if data/words.txt cannot be read.
    words_ = {
        {"ELEPHANT", "Animals", "Largest land mammal with a trunk"},
        {"TIGER", "Animals", "Striped big cat, national animal of India"},
        {"PENGUIN", "Animals", "Flightless bird of the Antarctic"},
        {"MANGO", "Food", "King of fruits"},
        {"BIRYANI", "Food", "Spiced rice dish layered with meat or vegetables"},
        {"JUPITER", "Space", "Largest planet in the solar system"},
        {"ASTRONAUT", "Space", "Person trained to travel into space"},
        {"POLYMORPHISM", "Programming", "One interface, many forms"},
        {"DESTRUCTOR", "Programming", "Member function called when an object dies"},
        {"ENCAPSULATION", "Programming", "Bundling data with the methods that use it"},
        {"CHENNAI", "Indian Cities", "Capital of Tamil Nadu"},
        {"CRICKET", "Sports", "Bat-and-ball game with wickets"},
    };
}

std::string WordBank::normalize(const std::string &text) {
    std::string out;
    bool pendingSpace = false;
    for (unsigned char c : text) {
        if (std::isspace(c)) { pendingSpace = !out.empty(); continue; }
        if (pendingSpace) { out += ' '; pendingSpace = false; }
        out += static_cast<char>(std::toupper(c));
    }
    return out;
}

bool WordBank::isValidWord(const std::string &word) {
    if (word.empty()) return false;
    return std::all_of(word.begin(), word.end(), [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || c == ' ';
    });
}

std::size_t WordBank::letterCount(const std::string &word) {
    return static_cast<std::size_t>(std::count_if(word.begin(), word.end(),
                                                  [](char c) { return c != ' '; }));
}

void WordBank::loadFromFile(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw WordBankException("Cannot open word file: " + path);

    std::vector<WordEntry> loaded;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();     // Windows line endings
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string category, word, clue;
        std::getline(ss, category, '|');
        std::getline(ss, word, '|');
        std::getline(ss, clue);
        word = normalize(word);
        if (!isValidWord(word) || category.empty()) continue;           // skip bad lines
        loaded.push_back({word, category, clue});
    }
    if (loaded.empty()) throw WordBankException("No valid words found in " + path);
    words_ = std::move(loaded);
}

std::vector<std::string> WordBank::categories() const {
    std::vector<std::string> result;
    for (const WordEntry &e : words_)
        if (std::find(result.begin(), result.end(), e.category) == result.end())
            result.push_back(e.category);
    return result;
}

int WordBank::lengthBucket(const std::string &word) {
    return std::min(static_cast<int>(letterCount(word)), LONG_WORDS);
}

std::map<std::string, std::map<int, int>> WordBank::lengthCounts() const {
    std::map<std::string, std::map<int, int>> counts;
    for (const WordEntry &e : words_) {
        const int bucket = lengthBucket(e.word);
        ++counts[e.category][bucket];
        ++counts["Random"][bucket];
    }
    return counts;
}

WordEntry WordBank::pick(const std::string &category, const Difficulty &level, int wordLength) {
    if (wordLength != 0 && (wordLength < MIN_LENGTH_OPTION || wordLength > LONG_WORDS))
        throw WordBankException("Word length must be between 3 and 9+");

    auto matches = [&](const WordEntry &e, bool checkLength, bool checkRecent) {
        if (category != "Random" && e.category != category) return false;
        if (wordLength != 0) {                                     // the player chose a length
            if (lengthBucket(e.word) != wordLength) return false;
        } else {
            std::size_t len = letterCount(e.word);
            if (checkLength && (len < level.minLength() || len > level.maxLength())) return false;
        }
        if (checkRecent && std::find(recent_.begin(), recent_.end(), e.word) != recent_.end()) return false;
        return true;
    };

    // Relax the rules step by step if the category has too few suitable words.
    std::vector<const WordEntry *> pool;
    const bool passes[3][2] = {{true, true}, {true, false}, {false, false}};
    for (const auto &pass : passes) {
        for (const WordEntry &e : words_)
            if (matches(e, pass[0], pass[1])) pool.push_back(&e);
        if (!pool.empty()) break;
    }
    if (pool.empty()) {
        if (wordLength != 0)
            throw WordBankException("No " + std::string(wordLength == LONG_WORDS ? "9+" : std::to_string(wordLength)) +
                                    "-letter words in " + category + " - choose another length");
        throw WordBankException("No words available for category " + category);
    }

    std::uniform_int_distribution<std::size_t> dist(0, pool.size() - 1);
    WordEntry chosen = *pool[dist(rng())];
    recent_.push_back(chosen.word);
    if (recent_.size() > RECENT_LIMIT) recent_.pop_front();
    return chosen;
}
