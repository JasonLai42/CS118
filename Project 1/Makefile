# server.c makefile

CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra

submission = lab4c-204995126.tar.gz
submission_files = lab4c_tcp.c lab4c_tls.c Makefile README

default:
	$(CC) $(CFLAGS) -o webserver webserver.c

clean:
	@rm -f webserver
