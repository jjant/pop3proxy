all:

	gcc -o proxy proxy.c proxyUtilities.c parsers.c buffer.c -pthread
	gcc -o confClient confClient.c
	gcc -o testT testT.c

parser_run:
	gcc mime_parser.c parser_utils.c -o parser.out
	./parser.out

pop3_run:
	gcc pop3_parser.c parser_utils.c mime_parser.c -o pop3.out
	./pop3.out

clean:
	rm proxy confClient testT
