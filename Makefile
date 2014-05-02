LIBTCODDIR=libtcod-1.5.1
SDLDIR=SDL-1.2.14
CFLAGS=$(FLAGS) -I$(LIBTCODDIR)/include -I$(SDLDIR)/include -IBrogueCode -IPlatformCode -DBROGUE_TCOD -g

%.o : %.c
	gcc $(CFLAGS) -O2 -s -o $@ -c $< 

OBJS=BrogueCode/Architect.o \
	BrogueCode/Combat.o \
	BrogueCode/Dijkstra.o \
	BrogueCode/Globals.o \
	BrogueCode/IO.o \
	BrogueCode/Buttons.o \
	BrogueCode/MainMenu.o \
	BrogueCode/Items.o \
	BrogueCode/Light.o \
	BrogueCode/Monsters.o \
	BrogueCode/Movement.o \
	BrogueCode/RogueMain.o \
	BrogueCode/Random.o \
	BrogueCode/Recordings.o \
	BrogueCode/Grid.o \
	BrogueCode/Time.o \
	PlatformCode/main.o \
	PlatformCode/platformdependent.o \
	PlatformCode/tcod-platform.o 

all : brogue

brogue : ${OBJS} 
	g++ -o brogue.exe ${OBJS} -L. -L$(LIBTCODDIR)/ -static-libgcc\
	    -static-libstdc++

.PHONY: run
run : brogue
	./brogue

.PHONY: clean
clean:
	$(RM) BrogueCode/*.o
	$(RM) PlatformCode/*.o
