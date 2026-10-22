# Tests PC du labyrinthe : compilent directement les sources du croquis.
# Aucune copie du code du jeu. Cibles : make test, make fuzz, make images.
#
# g++ standard avec ASan/UBSan. Sur une machine sans sanitizers, remplacer
# CXXFLAGS/LDFLAGS sur la ligne de commande.

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer
CPPFLAGS ?= -I../maze
LDFLAGS ?= -fsanitize=address,undefined

SIM_SRC = ../maze/maze_logic.cpp \
          ../maze/game.cpp \
          ../maze/renderer.cpp \
          ../maze/display_iface.cpp \
          sim_display.cpp \
          sim_main.cpp

.PHONY: all test fuzz images clean

all: sim

sim: $(SIM_SRC) ../maze/maze_logic.h ../maze/game.h ../maze/renderer.h \
     ../maze/display_iface.h sim_display.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(SIM_SRC) $(LDFLAGS) -o $@

test: sim
	./sim test

fuzz: sim
	./sim fuzz

images: sim
	./sim images

clean:
	rm -f sim
	rm -rf out
