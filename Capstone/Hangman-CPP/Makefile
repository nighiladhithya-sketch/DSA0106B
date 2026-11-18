# Hangman - C++ Edition  (Linux / macOS)
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic -Iinclude
SRCS      = src/main.cpp src/Game.cpp src/WordBank.cpp src/Player.cpp src/Scoreboard.cpp src/HttpServer.cpp src/WebApp.cpp src/ConsoleGame.cpp

hangman: $(SRCS) include/*.h
	$(CXX) $(CXXFLAGS) $(SRCS) -o hangman

test: tests/test_game.cpp src/Game.cpp src/WordBank.cpp src/Player.cpp
	$(CXX) $(CXXFLAGS) tests/test_game.cpp src/Game.cpp src/WordBank.cpp src/Player.cpp -o test_game && ./test_game

run: hangman
	./hangman

console: hangman
	./hangman --console

clean:
	rm -f hangman test_game

.PHONY: test run console clean
