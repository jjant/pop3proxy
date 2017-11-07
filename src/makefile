all:

	gcc -o proxy proxy.c proxyUtilities.c parsers.c buffer.c -pthread
	gcc -o confClient confClient.c
	gcc -o testT testT.c

parser_run:
	gcc mime_parser.c parser_utils.c -o parser.out 
	./parser.out

clean:
	rm proxy confClient testT
