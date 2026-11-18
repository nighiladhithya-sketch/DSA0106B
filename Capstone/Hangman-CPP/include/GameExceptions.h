// GameExceptions.h - custom exception hierarchy (Unit V: exception handling)
#pragma once
#include <stdexcept>
#include <string>

// Base class for every error the game can report to the player.
class HangmanException : public std::runtime_error {
public:
    explicit HangmanException(const std::string &message) : std::runtime_error(message) {}
    virtual ~HangmanException() = default;
    virtual std::string type() const { return "HangmanException"; }
};

// The guess is not a letter / not a valid word.
class InvalidGuessException : public HangmanException {
public:
    explicit InvalidGuessException(const std::string &message) : HangmanException(message) {}
    std::string type() const override { return "InvalidGuess"; }
};

// The same letter (or word) was already tried.
class DuplicateGuessException : public HangmanException {
public:
    explicit DuplicateGuessException(const std::string &guess)
        : HangmanException("You already tried " + guess) {}
    std::string type() const override { return "DuplicateGuess"; }
};

// A move was attempted after the round finished.
class GameOverException : public HangmanException {
public:
    GameOverException() : HangmanException("This round is over - start a new word") {}
    std::string type() const override { return "GameOver"; }
};

// No hints remain, or the word has no clue.
class HintUnavailableException : public HangmanException {
public:
    explicit HintUnavailableException(const std::string &message) : HangmanException(message) {}
    std::string type() const override { return "HintUnavailable"; }
};

// The word list could not be loaded or has no matching words.
class WordBankException : public HangmanException {
public:
    explicit WordBankException(const std::string &message) : HangmanException(message) {}
    std::string type() const override { return "WordBank"; }
};
