@echo off
REM Builds and runs the automated engine tests
cd /d "%~dp0"
g++ -std=c++17 -Iinclude tests\test_game.cpp src\Game.cpp src\WordBank.cpp src\Player.cpp -o test_game.exe && test_game.exe
pause
