all:

	gcc -o proxy proxy.c proxyUtilities.c buffer.c -pthread
	gcc -o confClient confClient.c

clean:
	rm proxy confClient