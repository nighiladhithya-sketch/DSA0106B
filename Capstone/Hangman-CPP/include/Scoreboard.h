// Scoreboard.h - persistent top-10 leaderboard stored in data/scores.txt
#pragma once
#include <string>
#include <vector>

struct ScoreEntry {
    std::string name;
    int score = 0;
    std::string difficulty;
    std::string word;
    std::string date;
};

// Higher score ranks first (used by std::sort).
inline bool operator<(const ScoreEntry &a, const ScoreEntry &b) { return a.score > b.score; }

class Scoreboard {
public:
    explicit Scoreboard(const std::string &file);
    int add(const ScoreEntry &entry);      // returns rank 1..10, or 0 if it did not qualify
    const std::vector<ScoreEntry> &entries() const { return entries_; }
    static std::string today();

private:
    void load();
    void save() const;
    std::string file_;
    std::vector<ScoreEntry> entries_;
    static const std::size_t MAX_ENTRIES = 10;
};
