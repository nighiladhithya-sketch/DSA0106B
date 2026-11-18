// Game.cpp - Hangman rules
#include "Game.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>

int Game::gamesCreated_ = 0;   // file-scope definition of the static data member

Game::Game(const WordEntry &entry, std::unique_ptr<Difficulty> level, bool customWord)
    : word_(entry.word), category_(entry.category), clue_(entry.clue),
      difficulty_(std::move(level)), custom_(customWord), start_(Clock::now()) {
    if (!difficulty_) throw HangmanException("A difficulty level is required");
    if (!WordBank::isValidWord(word_)) throw InvalidGuessException("Word must contain only letters and spaces");
    hintsLeft_ = difficulty_->hints();
    clueShown_ = difficulty_->showsClueAtStart() && !clue_.empty();
    ++gamesCreated_;
}

void Game::ensurePlaying() {
    checkTimer();
    if (isOver()) throw GameOverException();
}

void Game::finish(GameStatus result, const std::string &reason) {
    if (isOver()) return;
    status_ = result;
    endReason_ = reason;
    end_ = Clock::now();
    timeLeftAtEnd_ = std::max(0, secondsLeft());
}

bool Game::isSolved() const {
    return std::all_of(word_.begin(), word_.end(),
                       [this](char c) { return c == ' ' || correct_.count(c) > 0; });
}

GuessResult Game::guess(char letter) {
    ensurePlaying();
    if (!std::isalpha(static_cast<unsigned char>(letter)))
        throw InvalidGuessException(std::string("'") + letter + "' is not a letter");
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    if (correct_.count(c) || wrong_.count(c))
        throw DuplicateGuessException(std::string("the letter ") + c);

    if (word_.find(c) != std::string::npos) {
        correct_.insert(c);
        if (isSolved()) finish(GameStatus::Won, "solved");
        return GuessResult::Correct;
    }
    wrong_.insert(c);
    ++livesLost_;
    if (livesLost_ >= difficulty_->maxLives()) finish(GameStatus::Lost, "hanged");
    return GuessResult::Wrong;
}

GuessResult Game::guess(const std::string &wholeWord) {
    ensurePlaying();
    const std::string attempt = WordBank::normalize(wholeWord);
    if (!WordBank::isValidWord(attempt))
        throw InvalidGuessException("A solve attempt may contain only letters and spaces");
    if (attempt.size() == 1) return guess(attempt[0]);          // delegate to the letter overload
    if (std::find(wrongWords_.begin(), wrongWords_.end(), attempt) != wrongWords_.end())
        throw DuplicateGuessException("\"" + attempt + "\"");

    if (attempt == word_) {
        for (char c : word_) if (c != ' ') correct_.insert(c);
        finish(GameStatus::Won, "solved the word");
        return GuessResult::Correct;
    }
    wrongWords_.push_back(attempt);
    ++livesLost_;                                       // a wrong solve counts as one wrong guess
    if (livesLost_ >= difficulty_->maxLives()) finish(GameStatus::Lost, "hanged");
    return GuessResult::Wrong;
}

char Game::useLetterHint() {
    ensurePlaying();
    if (hintsLeft_ <= 0) throw HintUnavailableException("No hints left for this word");
    std::vector<char> hidden;
    for (char c : word_)
        if (c != ' ' && !correct_.count(c) && std::find(hidden.begin(), hidden.end(), c) == hidden.end())
            hidden.push_back(c);
    std::uniform_int_distribution<std::size_t> dist(0, hidden.size() - 1);
    const char revealed = hidden[dist(WordBank::rng())];
    correct_.insert(revealed);
    --hintsLeft_;
    ++hintsUsed_;
    if (isSolved()) finish(GameStatus::Won, "solved");
    return revealed;
}

const std::string &Game::useClueHint() {
    ensurePlaying();
    if (clue_.empty()) throw HintUnavailableException("This word has no clue");
    if (clueShown_) return clue_;
    if (hintsLeft_ <= 0) throw HintUnavailableException("No hints left for this word");
    clueShown_ = true;
    --hintsLeft_;
    ++hintsUsed_;
    return clue_;
}

void Game::forfeit() {
    finish(GameStatus::Lost, "gave up");
}

void Game::checkTimer() {
    const int limit = difficulty_->timeLimitSeconds();
    if (!isOver() && limit > 0 && elapsedSeconds() >= limit) finish(GameStatus::Lost, "time up");
}

int Game::elapsedSeconds() const {
    const auto stop = isOver() ? end_ : Clock::now();
    return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(stop - start_).count());
}

int Game::secondsLeft() const {
    const int limit = difficulty_->timeLimitSeconds();
    if (limit <= 0) return -1;
    return std::max(0, limit - elapsedSeconds());
}

std::string Game::maskedWord() const {
    std::string out;
    for (char c : word_) out += (c == ' ' || correct_.count(c)) ? c : '_';
    return out;
}

int Game::roundScore() const {
    if (status_ != GameStatus::Won) return 0;
    std::set<char> unique;
    for (char c : word_) if (c != ' ') unique.insert(c);
    double base = 10.0 * unique.size() + 20.0 * livesLeft() - 15.0 * hintsUsed_;
    if (difficulty_->timeLimitSeconds() > 0) base += 2.0 * timeLeftAtEnd_;   // speed bonus
    const int score = static_cast<int>(std::lround(base * difficulty_->scoreMultiplier()));
    return std::max(10, score);
}

std::ostream &operator<<(std::ostream &out, const Game &game) {
    const std::string masked = game.maskedWord();
    for (std::size_t i = 0; i < masked.size(); ++i) out << (i ? " " : "") << masked[i];
    return out;
}
