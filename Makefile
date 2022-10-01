# 
# VERSION CHANGES
#

#BV=$(shell (git rev-list HEAD --count))
#BD=$(shell (date))
BV=1234
BD=today
SDLFLAGS=$(shell (sdl2-config --static-libs --cflags))
CFLAGS=  -Wall -O2 -DBUILD_VER="$(BV)" -DBUILD_DATE=\""$(BD)"\" -DFAKE_SERIAL=$(FAKE_SERIAL)
#CFLAGS=  -Wall -O0 -ggdb -g -DBUILD_VER="$(BV)" -DBUILD_DATE=\""$(BD)"\" -DFAKE_SERIAL=$(FAKE_SERIAL)
LIBS=-lSDL2_ttf
CC=gcc
GCC=g++

OBJ=dm3058e-sdl

default: $(OBJ)
	@echo
	@echo

dm3058e-sdl: dm3058e-sdl.cpp
	@echo Build Release $(BV)
	@echo Build Date $(BD)
	${GCC} ${CFLAGS} $(COMPONENTS) dm3058e-sdl.cpp $(SDLFLAGS) $(LIBS) ${OFILES} -o ${OBJ} 

clean:
	rm -v ${OBJ} 
