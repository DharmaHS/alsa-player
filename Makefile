all:
	gcc -o music_player.o music_player.c -lasound -lncurses
debug:
	gcc -g -o music_player.o music_player.c -lasound -lncurses
test: all
	./music_player.o
debug-test: debug
	gdb --args music_player.o
clean:
	rm music_player.o
