// Player.cpp
#include "Player.h"
#include <algorithm>
#include <cctype>

Player::Player(const std::string &name) : name_(sanitize(name)) {}

std::string Player::sanitize(const std::string &raw) {
    std::string clean;
    for (unsigned char c : raw) {
        if (std::isalnum(c) || c == ' ' || c == '_' || c == '-' || c == '.') clean += static_cast<char>(c);
        if (clean.size() >= 16) break;
    }
    while (!clean.empty() && clean.back() == ' ') clean.pop_back();
    while (!clean.empty() && clean.front() == ' ') clean.erase(clean.begin());
    return clean.empty() ? "Player" : clean;
}

void Player::setName(const std::string &name) { name_ = sanitize(name); }

int Player::streakBonusPercent() const { return std::min(streak_, 5) * 10; }

int Player::recordWin(int roundScore) {
    const int awarded = roundScore * (100 + streakBonusPercent()) / 100;   // bonus earned by the streak so far
    ++wins_;
    ++played_;
    ++streak_;
    bestStreak_ = std::max(bestStreak_, streak_);
    *this += awarded;                       // uses the overloaded operator
    return awarded;
}

void Player::recordLoss() {
    ++losses_;
    ++played_;
    streak_ = 0;
}

Player &Player::operator+=(int points) {
    totalScore_ += points;
    return *this;
}
