// WebApp.cpp
#include "WebApp.h"
#include <cctype>
#include <fstream>
#include <sstream>
#include "Json.h"

WebApp::WebApp(WordBank &bank, Scoreboard &board, const std::string &webRoot)
    : bank_(bank), board_(board), webRoot_(webRoot) {}

HttpResponse WebApp::handle(const HttpRequest &request) {
    if (request.method != "GET") return {405, "text/plain", "Only GET is supported"};
    if (request.path.rfind("/api/", 0) == 0) return api(request);
    return staticFile(request.path);
}

// ---------------------------------------------------------------- static files
HttpResponse WebApp::staticFile(const std::string &requested) const {
    const std::string path = (requested == "/" || requested.empty()) ? "/index.html" : requested;
    if (path.find("..") != std::string::npos) return {403, "text/plain", "Forbidden"};

    std::ifstream file(webRoot_ + path, std::ios::binary);
    if (!file) return {404, "text/plain", "Not found: " + path};
    std::ostringstream content;
    content << file.rdbuf();

    const std::string ext = path.substr(path.find_last_of('.') + 1);
    std::string type = "application/octet-stream";
    if (ext == "html") type = "text/html; charset=utf-8";
    else if (ext == "css") type = "text/css; charset=utf-8";
    else if (ext == "js") type = "application/javascript; charset=utf-8";
    else if (ext == "svg") type = "image/svg+xml";
    else if (ext == "png") type = "image/png";
    else if (ext == "ico") type = "image/x-icon";
    return {200, type, content.str()};
}

// ---------------------------------------------------------------- JSON helpers
std::string WebApp::playerJson() const {
    return Json()
        .add("name", player_.name())
        .add("totalScore", player_.totalScore())
        .add("played", player_.gamesPlayed())
        .add("wins", player_.wins())
        .add("losses", player_.losses())
        .add("streak", player_.streak())
        .add("bestStreak", player_.bestStreak())
        .add("streakBonus", player_.streakBonusPercent())
        .str();
}

std::string WebApp::leaderboardJson() const {
    std::string arr = "[";
    bool first = true;
    for (const ScoreEntry &e : board_.entries()) {
        arr += (first ? "" : ",") + Json().add("name", e.name).add("score", e.score)
                   .add("difficulty", e.difficulty).add("word", e.word).add("date", e.date).str();
        first = false;
    }
    return arr + "]";
}

std::string WebApp::configJson() const {
    std::string levels = "[";
    const char *names[] = {"Easy", "Medium", "Hard"};
    for (int i = 0; i < 3; ++i) {
        std::unique_ptr<Difficulty> d = Difficulty::create(names[i]);   // polymorphic objects
        levels += (i ? "," : "") + Json().add("name", d->name()).add("description", d->description())
                      .add("lives", d->maxLives()).add("hints", d->hints())
                      .add("timeLimit", d->timeLimitSeconds()).add("multiplier", d->scoreMultiplier()).str();
    }
    levels += "]";
    std::string lengths = "{";                         // {"Animals":{"3":4,...},...}
    bool firstCat = true;
    for (const auto &cat : bank_.lengthCounts()) {
        Json perLength;
        for (const auto &len : cat.second) perLength.add(std::to_string(len.first), len.second);
        lengths += (firstCat ? "" : ",") + Json::escape(cat.first) + ":" + perLength.str();
        firstCat = false;
    }
    lengths += "}";
    return Json()
        .add("categories", bank_.categories())
        .raw("lengthCounts", lengths)
        .add("wordCount", static_cast<int>(bank_.size()))
        .raw("difficulties", levels)
        .raw("player", playerJson())
        .raw("leaderboard", leaderboardJson())
        .str();
}

static std::vector<std::string> toStrings(const std::set<char> &letters) {
    std::vector<std::string> out;
    for (char c : letters) out.emplace_back(1, c);
    return out;
}

std::string WebApp::stateJson(const std::string &event, const std::string &extraKey,
                              const std::string &extraValue) {
    Json j;
    j.add("event", event);
    if (!extraKey.empty()) j.add(extraKey, extraValue);
    j.add("hasGame", game_ != nullptr);
    if (game_) {
        game_->checkTimer();
        recordResultIfOver();
        const Game &g = *game_;
        const char *status = g.status() == GameStatus::Playing ? "playing"
                           : g.status() == GameStatus::Won ? "won" : "lost";
        std::vector<std::string> masked;
        for (char c : g.maskedWord()) masked.emplace_back(1, c);
        j.add("masked", masked)
         .add("category", g.isCustomWord() ? std::string("Two Player") : g.category())
         .add("difficulty", g.difficulty().name())
         .add("maxLives", g.difficulty().maxLives())
         .add("livesLost", g.livesLost())
         .add("livesLeft", g.livesLeft())
         .add("wrongLetters", toStrings(g.wrongLetters()))
         .add("correctLetters", toStrings(g.correctLetters()))
         .add("wrongWords", g.wrongWords())
         .add("wordLength", static_cast<int>(WordBank::letterCount(g.maskedWord())))
         .add("hintsLeft", g.hintsLeft())
         .add("hintsUsed", g.hintsUsed())
         .add("hasClue", g.hasClue())
         .add("clue", (g.clueVisible() || g.isOver()) ? g.clue() : std::string())   // clue revealed at the end
         .add("status", status)
         .add("endReason", g.endReason())
         .add("word", g.isOver() ? g.word() : std::string())
         .add("timeLimit", g.difficulty().timeLimitSeconds())
         .add("secondsLeft", g.secondsLeft())
         .add("elapsed", g.elapsedSeconds())
         .add("roundScore", g.roundScore())
         .add("awarded", g.isOver() ? lastAwarded_ : 0)
         .add("rank", g.isOver() ? lastRank_ : 0)
         .add("custom", g.isCustomWord())
         .add("gamesCreated", Game::gamesCreated());
    }
    j.raw("player", playerJson());
    return j.str();
}

// ---------------------------------------------------------------- game flow
void WebApp::startRound(const WordEntry &entry, const std::string &difficulty, bool custom) {
    game_ = std::make_unique<Game>(entry, Difficulty::create(difficulty), custom);  // old round freed automatically
    resultRecorded_ = false;
    lastAwarded_ = lastRank_ = 0;
}

void WebApp::recordResultIfOver() {
    if (!game_ || !game_->isOver() || resultRecorded_) return;
    resultRecorded_ = true;
    if (game_->status() == GameStatus::Won) {
        lastAwarded_ = player_.recordWin(game_->roundScore());
        if (!game_->isCustomWord())                    // two-player words are not ranked
            lastRank_ = board_.add({player_.name(), lastAwarded_, game_->difficulty().name(),
                                    game_->word(), Scoreboard::today()});
    } else {
        player_.recordLoss();
    }
}

HttpResponse WebApp::api(const HttpRequest &req) {
    const std::string &p = req.path;
    try {
        if (p == "/api/config")      return {200, "application/json; charset=utf-8", configJson()};
        if (p == "/api/leaderboard") return {200, "application/json; charset=utf-8", leaderboardJson()};
        if (p == "/api/state")       return {200, "application/json; charset=utf-8", stateJson("state")};

        if (p == "/api/new") {
            if (!req.param("name").empty()) player_.setName(req.param("name"));
            const std::string difficulty = req.param("difficulty", "Medium");
            std::unique_ptr<Difficulty> level = Difficulty::create(difficulty);
            const std::string lengthParam = req.param("length", "0");
            int wordLength = 0;
            try { wordLength = lengthParam == "any" ? 0 : std::stoi(lengthParam); }
            catch (const std::exception &) { throw InvalidGuessException("Invalid word length: " + lengthParam); }
            startRound(bank_.pick(req.param("category", "Random"), *level, wordLength), difficulty, false);
            return {200, "application/json; charset=utf-8", stateJson("new")};
        }
        if (p == "/api/custom") {
            if (!req.param("name").empty()) player_.setName(req.param("name"));
            const std::string word = WordBank::normalize(req.param("word"));
            if (!WordBank::isValidWord(word) || WordBank::letterCount(word) < 2 || word.size() > 30)
                throw InvalidGuessException("Secret word must be 2-30 letters (spaces allowed)");
            startRound({word, "Two Player", req.param("clue")}, req.param("difficulty", "Medium"), true);
            return {200, "application/json; charset=utf-8", stateJson("new")};
        }

        if (!game_) throw HangmanException("Start a new game first");

        if (p == "/api/guess") {
            const std::string letter = req.param("letter");
            if (letter.size() != 1) throw InvalidGuessException("Send exactly one letter");
            const GuessResult r = game_->guess(letter[0]);
            return {200, "application/json; charset=utf-8",
                    stateJson(r == GuessResult::Correct ? "correct" : "wrong", "letter",
                              std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(letter[0])))))};
        }
        if (p == "/api/solve") {
            const GuessResult r = game_->guess(req.param("word"));
            return {200, "application/json; charset=utf-8",
                    stateJson(r == GuessResult::Correct ? "solved" : "wrongWord")};
        }
        if (p == "/api/hint") {
            if (req.param("type") == "clue") {
                game_->useClueHint();
                return {200, "application/json; charset=utf-8", stateJson("clue")};
            }
            const char c = game_->useLetterHint();
            return {200, "application/json; charset=utf-8", stateJson("hint", "letter", std::string(1, c))};
        }
        if (p == "/api/forfeit") {
            game_->forfeit();
            return {200, "application/json; charset=utf-8", stateJson("forfeit")};
        }
        return {404, "application/json; charset=utf-8", "{\"error\":\"Unknown endpoint\"}"};
    }
    // Handlers ordered from most derived to most general (Unit V).
    catch (const DuplicateGuessException &e) {
        return {200, "application/json; charset=utf-8", stateJson("error", "error", e.what())};
    } catch (const HangmanException &e) {        // Invalid guess, game over, no hints, word bank...
        return {200, "application/json; charset=utf-8", stateJson("error", "error", e.what())};
    } catch (const std::exception &e) {
        return {500, "application/json; charset=utf-8", Json().add("error", e.what()).str()};
    } catch (...) {
        return {500, "application/json; charset=utf-8", "{\"error\":\"Unknown server error\"}"};
    }
}
