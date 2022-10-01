# 
# VERSION CHANGES
#

#BV=$(shell (git rev-list HEAD --count))
#BD=$(shell (date))
BV=1234
BD=$(shell date '+%Y-%m-%d')
SDLFLAGS=$(shell (sdl2-config --static-libs --cflags))
CFLAGS=  -Wall -O2 -DBUILD_VER="$(BV)" -DBUILD_DATE=\""$(BD)"\" -DFAKE_SERIAL=$(FAKE_SERIAL)
#CFLAGS=  -Wall -O0 -ggdb -g -DBUILD_VER="$(BV)" -DBUILD_DATE=\""$(BD)"\" -DFAKE_SERIAL=$(FAKE_SERIAL)
LIBS=-lSDL2_ttf
CC=gcc
GCC=g++

OBJ1=gdm-8341-sdl
OBJ2=dm3058e-sdl


default: ${OBJ2} ${OBJ2} 
	@echo
	@echo

gdm-8341-sdl: gdm-8341-sdl.cpp
	@echo Build Release $(BV)
	@echo Build Date $(BD)
	${GCC} ${CFLAGS} $(COMPONENTS) gdm-8341-sdl.cpp $(SDLFLAGS) $(LIBS) ${OFILES} -o ${OBJ1} 

dm3058e-sdl: dm3058e-sdl.cpp
	@echo Build Release $(BV)
	@echo Build Date $(BD)
	${GCC} ${CFLAGS} $(COMPONENTS) dm3058e-sdl.cpp $(SDLFLAGS) $(LIBS) ${OFILES} -o ${OBJ2} 



clean:
	rm -v -f ${OBJ1} 
	rm -v -f ${OBJ2} 
