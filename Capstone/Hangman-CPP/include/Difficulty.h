// Difficulty.h - abstract difficulty interface with three concrete levels
// (Unit IV: inheritance, Unit V: pure virtual functions & runtime polymorphism)
#pragma once
#include <memory>
#include <string>
#include "GameExceptions.h"

class Difficulty {
public:
    virtual ~Difficulty() = default;                 // virtual destructor: safe polymorphic delete

    virtual std::string name() const = 0;            // pure virtual: Difficulty is abstract
    virtual std::string description() const = 0;
    virtual int maxLives() const = 0;
    virtual int hints() const = 0;
    virtual std::size_t minLength() const = 0;      // used when the player picks "Any" length
    virtual std::size_t maxLength() const = 0;
    virtual double scoreMultiplier() const = 0;
    virtual int timeLimitSeconds() const { return 0; }   // 0 means untimed
    virtual bool showsClueAtStart() const { return false; }

    // Factory: builds the right subclass from its name.
    static std::unique_ptr<Difficulty> create(const std::string &name);
};

class EasyDifficulty final : public Difficulty {
public:
    std::string name() const override { return "Easy"; }
    std::string description() const override { return "6 wrong guesses, 3 hints, clue shown"; }
    int maxLives() const override { return 6; }
    int hints() const override { return 3; }
    std::size_t minLength() const override { return 3; }
    std::size_t maxLength() const override { return 7; }
    double scoreMultiplier() const override { return 1.0; }
    bool showsClueAtStart() const override { return true; }
};

class MediumDifficulty final : public Difficulty {
public:
    std::string name() const override { return "Medium"; }
    std::string description() const override { return "6 wrong guesses, 2 hints"; }
    int maxLives() const override { return 6; }
    int hints() const override { return 2; }
    std::size_t minLength() const override { return 5; }
    std::size_t maxLength() const override { return 10; }
    double scoreMultiplier() const override { return 1.5; }
};

class HardDifficulty final : public Difficulty {
public:
    std::string name() const override { return "Hard"; }
    std::string description() const override { return "6 wrong guesses, 1 hint, 90 s timer"; }
    int maxLives() const override { return 6; }
    int hints() const override { return 1; }
    std::size_t minLength() const override { return 8; }
    std::size_t maxLength() const override { return 30; }
    double scoreMultiplier() const override { return 2.5; }
    int timeLimitSeconds() const override { return 90; }
};

inline std::unique_ptr<Difficulty> Difficulty::create(const std::string &name) {
    if (name == "Easy" || name == "easy")     return std::make_unique<EasyDifficulty>();
    if (name == "Medium" || name == "medium") return std::make_unique<MediumDifficulty>();
    if (name == "Hard" || name == "hard")     return std::make_unique<HardDifficulty>();
    throw HangmanException("Unknown difficulty: " + name);
}
