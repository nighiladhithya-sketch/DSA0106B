// Scoreboard.cpp
#include "Scoreboard.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>

Scoreboard::Scoreboard(const std::string &file) : file_(file) { load(); }

std::string Scoreboard::today() {
    std::time_t now = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof buf, "%d-%m-%Y", std::localtime(&now));
    return buf;
}

void Scoreboard::load() {
    std::ifstream in(file_);
    if (!in) return;                                   // first run: no file yet
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::stringstream ss(line);
        ScoreEntry e;
        std::string score;
        if (!std::getline(ss, e.name, '|') || !std::getline(ss, score, '|')) continue;
        std::getline(ss, e.difficulty, '|');
        std::getline(ss, e.word, '|');
        std::getline(ss, e.date);
        try { e.score = std::stoi(score); } catch (const std::exception &) { continue; }
        entries_.push_back(e);
    }
    std::sort(entries_.begin(), entries_.end());
    if (entries_.size() > MAX_ENTRIES) entries_.resize(MAX_ENTRIES);
}

void Scoreboard::save() const {
    std::ofstream out(file_);
    for (const ScoreEntry &e : entries_)
        out << e.name << '|' << e.score << '|' << e.difficulty << '|' << e.word << '|' << e.date << '\n';
}

int Scoreboard::add(const ScoreEntry &entry) {
    // entries_ is kept sorted (highest first); insert after any equal scores.
    auto pos = std::upper_bound(entries_.begin(), entries_.end(), entry);
    const std::size_t index = static_cast<std::size_t>(pos - entries_.begin());
    if (index >= MAX_ENTRIES) return 0;
    entries_.insert(pos, entry);
    if (entries_.size() > MAX_ENTRIES) entries_.resize(MAX_ENTRIES);
    save();
    return static_cast<int>(index) + 1;
}
