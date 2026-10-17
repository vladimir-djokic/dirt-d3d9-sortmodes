# Builds the 32-bit d3d9.dll proxy with MinGW-w64 and GNU Make.
#
#   mingw32-make            build d3d9.dll
#   mingw32-make install    copy d3d9.dll into GAME_DIR, plus d3d9_sortmodes.ini if the game has none yet
#   mingw32-make clean      delete the built d3d9.dll
#
# Override any variable on the command line, for example:
#   mingw32-make MINGW=D:/tools/mingw32 GAME_DIR="D:\Games\DiRT" install
#
# Commands run through cmd.exe, so this works the same from PowerShell, Command Prompt or Git Bash.

SHELL := cmd.exe
.SHELLFLAGS := /c

MINGW    ?= C:/mingw32
CXX      := $(MINGW)/bin/g++.exe
CXXFLAGS ?= -O2 -Wall
LDFLAGS  ?= -shared -static -s
LDLIBS   := -luuid
GAME_DIR ?= C:\Program Files (x86)\Steam\steamapps\common\DiRT

TARGET := d3d9.dll
SRC    := d3d9_sortmodes.cpp
DEF    := d3d9.def
INI    := d3d9_sortmodes.ini

.PHONY: all check install clean

all: $(TARGET)

$(TARGET): $(SRC) $(DEF) | check
	"$(CXX)" $(CXXFLAGS) $(LDFLAGS) -o $@ $(SRC) $(DEF) $(LDLIBS)
	@echo Built $@

# The game is 32-bit, so a 64-bit compiler such as C:\mingw64 must not be used.
check:
	@"$(CXX)" -dumpmachine | findstr /B "i686-" >nul || (echo ERROR: $(CXX) is missing or is not a 32-bit MinGW compiler. Set MINGW to a 32-bit toolchain. & exit /b 1)

install: $(TARGET)
	@if not exist "$(GAME_DIR)\" echo ERROR: game folder not found: $(GAME_DIR) & exit /b 1
	@copy /Y $(TARGET) "$(GAME_DIR)\$(TARGET)" >nul
	@echo Copied $(TARGET) to $(GAME_DIR)
	@if not exist "$(GAME_DIR)\$(INI)" copy /Y $(INI) "$(GAME_DIR)\$(INI)" >nul & echo Copied default $(INI)

clean:
	@if exist $(TARGET) del /Q $(TARGET)
	@echo Cleaned
