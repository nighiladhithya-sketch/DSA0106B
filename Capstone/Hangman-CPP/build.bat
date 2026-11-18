@echo off
REM Build Hangman with MinGW-w64 g++ (C++17)
echo Building hangman.exe ...
g++ -std=c++17 -O2 -Wall -Iinclude src\main.cpp src\Game.cpp src\WordBank.cpp src\Player.cpp src\Scoreboard.cpp src\HttpServer.cpp src\WebApp.cpp src\ConsoleGame.cpp -o hangman.exe -lws2_32 -static
if errorlevel 1 (
  echo.
  echo Build FAILED. Make sure MinGW-w64 g++ is installed and on PATH.
  pause
  exit /b 1
)
echo Build OK. Run run.bat to play in the browser, or play_console.bat for the terminal version.
pause
