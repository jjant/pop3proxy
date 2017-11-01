all:

	gcc -o proxy proxy.c proxyUtilities.c parsers.c buffer.c -pthread
	gcc -o confClient confClient.c

clean:
	rm proxy confClient retrieved_mail* transformed_mail*