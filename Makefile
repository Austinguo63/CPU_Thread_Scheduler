CFLAGS = -std=gnu11
LIBS = -lpthread -lm
SOURCES = main.c scheduler.c interface.c
OUT = proj1

default:
	gcc $(CFLAGS) $(SOURCES) $(LIBS) -o $(OUT)
debug:
	gcc -g $(CFLAGS) $(SOURCES) $(LIBS) -o $(OUT)
fdebug:
	gcc -g -fsanitize=thread $(CFLAGS) $(SOURCES) $(LIBS) -o $(OUT)
all:
	gcc $(SOURCES) $(LIBS) -o $(OUT)
clean:
	rm -f $(OUT)
