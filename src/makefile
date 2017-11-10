all:

	gcc -o proxy proxy.c proxyUtilities.c parsers.c buffer.c -pthread
	gcc -o confClient confClient.c
	gcc -o testT testT.c

clean:
	rm proxy confClient testT proxy_log.txt