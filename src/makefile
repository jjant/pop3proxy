CFLAGS=-pedantic -std=c99 -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Wstrict-prototypes -Wcast-align -Wstrict-overflow=5 -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -Werror -pedantic-errors -Wmissing-prototypes
CC=gcc

POP3_FILTER_OUT=pop3filter
POP3_FILTER_SOURCES=proxy.c proxyUtilities.c parsers.c buffer.c
POP3_FILTER_FLAGS=-pthread

POP3_CLIENT_OUT=pop3ctl
POP3_CLIENT_SOURCES=confClient.c

STRIP_MIME_OUT=stripmime
STRIP_MIME_SOURCES=transformer/pop3_parser.c transformer/parser_utils.c transformer/mime_parser.c transformer/filter_list.c transformer/buffer_utils.c transformer/boundary_utils.c transformer/validations.c transformer/mime_state_machine.c transformer/debug.c

all: $(STRIP_MIME_OUT) $(POP3_FILTER_OUT) $(POP3_CLIENT_OUT)

$(STRIP_MIME_OUT):
	$(CC) $(CFLAGS) -o $(STRIP_MIME_OUT) $(STRIP_MIME_SOURCES)

$(POP3_FILTER_OUT):
	$(CC) $(CFLAGS) -o $(POP3_FILTER_OUT) $(POP3_FILTER_SOURCES) $(POP3_FILTER_FLAGS)

$(POP3_CLIENT_OUT):
	$(CC) $(CFLAGS) -o $(POP3_CLIENT_OUT) $(POP3_CLIENT_SOURCES)

a:
		$(CC) -o $(STRIP_MIME_OUT) $(STRIP_MIME_SOURCES) $(CFLAGS) && cp ../mime/mensajes/ii_images.mbox ./retr_mail_0100 && unix2dos ./retr_mail_0100 && FILTER_MEDIAS="image/png" ./stripmime

clean:
	rm pop3filter pop3ctl stripmime proxy_log.txt proxy_errors_log.txt metrics.txt
