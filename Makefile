CFLAGS = -I/usr/include/dbus-1.0 -I/usr/lib64/dbus-1.0/include -Wall -Wextra
DEBUG_FLAGS = -ggdb3 -gdwarf -fsanitize=address -fsanitize=undefined -fsanitize=leak

all:
	gcc -o portal $(CFLAGS) portal.c -ldbus-1

debug:
	gcc -o portal $(CFLAGS) $(DEBUG_FLAGS) portal.c -ldbus-1

run: all
	@echo
	@./portal

run_debug: debug
	@echo
	@./portal
