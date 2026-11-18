// Player.h - session statistics, streaks and score bonuses
#pragma once
#include <string>

class Player {
public:
    explicit Player(const std::string &name = "Player");

    void setName(const std::string &name);
    int recordWin(int roundScore);         // returns points awarded (with streak bonus)
    void recordLoss();
    Player &operator+=(int points);        // operator overloading: add points

    const std::string &name() const { return name_; }
    int totalScore() const { return totalScore_; }
    int gamesPlayed() const { return played_; }
    int wins() const { return wins_; }
    int losses() const { return losses_; }
    int streak() const { return streak_; }
    int bestStreak() const { return bestStreak_; }
    int streakBonusPercent() const;        // bonus for the NEXT win
    static std::string sanitize(const std::string &raw);

private:
    std::string name_;
    int totalScore_ = 0, played_ = 0, wins_ = 0, losses_ = 0, streak_ = 0, bestStreak_ = 0;
};
