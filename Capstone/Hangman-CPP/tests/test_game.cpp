// test_game.cpp - automated checks for the game engine (no framework needed)
// Build & run:  g++ -std=c++17 -Iinclude tests/test_game.cpp src/Game.cpp src/WordBank.cpp src/Player.cpp -o test_game && ./test_game
#include <cassert>
#include <iostream>
#include <thread>
#include "Game.h"
#include "Player.h"

// A test-only difficulty with a 1-second timer (shows how new levels plug in via inheritance).
class BlitzDifficulty : public Difficulty {
public:
    std::string name() const override { return "Blitz"; }
    std::string description() const override { return "test level"; }
    int maxLives() const override { return 3; }
    int hints() const override { return 1; }
    std::size_t minLength() const override { return 1; }
    std::size_t maxLength() const override { return 50; }
    double scoreMultiplier() const override { return 1.0; }
    int timeLimitSeconds() const override { return 1; }
};

template <typename E, typename F> static void expectThrow(F f) {
    try { f(); } catch (const E &) { return; }
    assert(false && "expected exception was not thrown");
}

int main() {
    // Correct, wrong, duplicate and invalid guesses
    Game g({"HELLO WORLD", "Test", "first program"}, Difficulty::create("Medium"));
    assert(g.maskedWord() == "_____ _____");
    assert(g.guess('l') == GuessResult::Correct);
    assert(g.maskedWord() == "__LL_ ___L_");
    assert(g.guess('z') == GuessResult::Wrong && g.livesLeft() == 5);
    expectThrow<DuplicateGuessException>([&] { g.guess('L'); });
    expectThrow<InvalidGuessException>([&] { g.guess('7'); });

    // Wrong whole-word guess counts as one wrong guess; correct one wins
    assert(g.guess(std::string("HELLO THERE")) == GuessResult::Wrong && g.livesLeft() == 4);
    assert(g.guess(std::string("hello   world")) == GuessResult::Correct);
    assert(g.status() == GameStatus::Won && g.roundScore() > 0);
    expectThrow<GameOverException>([&] { g.guess('Q'); });

    // Every level allows exactly 6 wrong guesses
    for (const char *lv : {"Easy", "Medium", "Hard"}) assert(Difficulty::create(lv)->maxLives() == 6);

    // Word length selection
    WordBank bank;
    for (int i = 0; i < 20; ++i) assert(WordBank::letterCount(bank.pick("Random", MediumDifficulty(), 5).word) == 5);
    assert(WordBank::letterCount(bank.pick("Random", EasyDifficulty(), 9).word) >= 9);
    expectThrow<WordBankException>([&] { bank.pick("Animals", EasyDifficulty(), 3); });   // none in built-in list
    expectThrow<WordBankException>([&] { bank.pick("Random", EasyDifficulty(), 2); });

    // Losing all lives
    Game lose({"CAT", "Test", ""}, Difficulty::create("Hard"));
    for (char c : std::string("BDEFGH")) lose.guess(c);
    assert(lose.status() == GameStatus::Lost && lose.roundScore() == 0);

    // Hints: letter hint reveals a letter, clue is free on Easy, hints run out
    Game hint({"MANGO", "Food", "King of fruits"}, Difficulty::create("Easy"));
    assert(hint.clueVisible());
    const char c = hint.useLetterHint();
    assert(hint.word().find(c) != std::string::npos && hint.hintsLeft() == 2);
    hint.useClueHint();                                   // already visible on Easy: no cost
    assert(hint.hintsLeft() == 2);
    hint.useLetterHint(); hint.useLetterHint();
    expectThrow<HintUnavailableException>([&] { hint.useLetterHint(); });

    // Timer runs out
    Game timed({"PYTHON", "Test", ""}, std::make_unique<BlitzDifficulty>());
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    expectThrow<GameOverException>([&] { timed.guess('P'); });
    assert(timed.status() == GameStatus::Lost && timed.endReason() == "time up");

    // Player streak bonus
    Player p("Deebak");
    assert(p.recordWin(100) == 100);                      // first win: no bonus
    assert(p.recordWin(100) == 110);                      // second consecutive win: +10%
    p.recordLoss();
    assert(p.streak() == 0 && p.bestStreak() == 2 && p.totalScore() == 210);

    std::cout << "All " << Game::gamesCreated() << " test games passed.\n";
    return 0;
}
