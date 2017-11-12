all:

	gcc -o pop3filter proxy.c proxyUtilities.c parsers.c buffer.c -pthread
	gcc -o pop3ctl confClient.c
	gcc -o testT testT.c
	gcc -o stripmime pop3_parser.c parser_utils.c mime_parser.c

clean:
	rm pop3filter pop3ctl stripmime proxy_log.txt proxy_errors_log.txt testT
