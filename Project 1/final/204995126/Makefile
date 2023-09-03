# server.c makefile

CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra

submission = 204995126.tar.gz
submission_files = webserver.c Makefile README

default:
	$(CC) $(CFLAGS) -o webserver webserver.c

dist:
	@tar -czf $(submission) $(submission_files)

clean:
	@rm -f webserver 204995126.tar.gz
