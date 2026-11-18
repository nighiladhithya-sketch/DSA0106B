// WebApp.h - routes browser requests to the C++ game engine (JSON API + static files)
#pragma once
#include <memory>
#include <string>
#include "Game.h"
#include "HttpServer.h"
#include "Player.h"
#include "Scoreboard.h"
#include "WordBank.h"

class WebApp {
public:
    WebApp(WordBank &bank, Scoreboard &board, const std::string &webRoot);
    HttpResponse handle(const HttpRequest &request);

private:
    HttpResponse api(const HttpRequest &request);
    HttpResponse staticFile(const std::string &path) const;
    std::string stateJson(const std::string &event, const std::string &extraKey = "",
                          const std::string &extraValue = "");
    std::string configJson() const;
    std::string leaderboardJson() const;
    std::string playerJson() const;
    void startRound(const WordEntry &entry, const std::string &difficulty, bool custom);
    void recordResultIfOver();

    WordBank &bank_;                 // references: the app uses, but does not own, these
    Scoreboard &board_;
    std::string webRoot_;
    Player player_;
    std::unique_ptr<Game> game_;     // the current round (has-a relationship)
    bool resultRecorded_ = false;
    int lastAwarded_ = 0;
    int lastRank_ = 0;
};
