# Builds the 32-bit d3d9.dll proxy with MinGW-w64 and GNU Make, and installs it
# into Colin McRae: DiRT (2007) or Race Driver: GRID (2008).
#
#   mingw32-make                     build d3d9.dll
#   mingw32-make install-dirt        install into DiRT's Steam folder
#   mingw32-make install-grid        install into GRID's Steam folder
#   mingw32-make install GAME=grid   same as install-grid (GAME is dirt or grid)
#   mingw32-make clean               delete the built d3d9.dll
#
# install copies d3d9.dll, plus the game's default settings file as
# d3d9_sortmodes.ini when the game folder has none yet. It refuses to run
# unless the game's exe is in the target folder.
#
# Override any variable on the command line, for example:
#   mingw32-make install GAME=grid GAME_DIR="D:\Games\GRID"
#   mingw32-make MINGW=D:/tools/mingw32
#
# Commands run through cmd.exe, so this works the same from PowerShell, Command Prompt or Git Bash.

SHELL := cmd.exe
.SHELLFLAGS := /c

MINGW    ?= C:/mingw32
CXX      := $(MINGW)/bin/g++.exe
CXXFLAGS ?= -O2 -Wall
LDFLAGS  ?= -shared -static -s
LDLIBS   := -luuid

GAME         ?= dirt
STEAM_COMMON ?= C:\Program Files (x86)\Steam\steamapps\common

ifeq ($(GAME),dirt)
  GAME_DIR ?= $(STEAM_COMMON)\DiRT
  GAME_EXE := DiRT.exe
  INI_SRC  := d3d9_sortmodes.ini
else ifeq ($(GAME),grid)
  GAME_DIR ?= $(STEAM_COMMON)\Grid
  GAME_EXE := grid.exe
  INI_SRC  := d3d9_sortmodes.grid.ini
else
  $(error Unknown GAME "$(GAME)". Use GAME=dirt or GAME=grid)
endif

TARGET := d3d9.dll
SRC    := d3d9_sortmodes.cpp
DEF    := d3d9.def
INI    := d3d9_sortmodes.ini

.PHONY: all check install install-dirt install-grid clean

all: $(TARGET)

$(TARGET): $(SRC) $(DEF) | check
	"$(CXX)" $(CXXFLAGS) $(LDFLAGS) -o $@ $(SRC) $(DEF) $(LDLIBS)
	@echo Built $@

# Both games are 32-bit, so a 64-bit compiler such as C:\mingw64 must not be used.
check:
	@"$(CXX)" -dumpmachine | findstr /B "i686-" >nul || (echo ERROR: $(CXX) is missing or is not a 32-bit MinGW compiler. Set MINGW to a 32-bit toolchain. & exit /b 1)

install: $(TARGET)
	@if not exist "$(GAME_DIR)\$(GAME_EXE)" echo ERROR: $(GAME_EXE) not found in $(GAME_DIR). Set GAME_DIR to the game folder. & exit /b 1
	@copy /Y $(TARGET) "$(GAME_DIR)\$(TARGET)" >nul
	@echo Copied $(TARGET) to $(GAME_DIR)
	@if not exist "$(GAME_DIR)\$(INI)" copy /Y $(INI_SRC) "$(GAME_DIR)\$(INI)" >nul & echo Copied default settings $(INI_SRC) as $(INI). Set refresh to your in-game refresh rate.

install-dirt:
	@"$(MAKE)" --no-print-directory install GAME=dirt

install-grid:
	@"$(MAKE)" --no-print-directory install GAME=grid

clean:
	@if exist $(TARGET) del /Q $(TARGET)
	@echo Cleaned
