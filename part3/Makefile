CC := gcc
CFLAGS := -g -Wall -Wno-deprecated-declarations -Werror

philosopher : philosopher.c philosopher.h
	$(CC) $(CFLAGS) philosopher.c -o diningOut -pthread

clean :
	rm diningOut