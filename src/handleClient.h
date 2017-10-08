#ifndef HANDLE_CLIENT
#define HANDLE_CLIENT

/**
 * Main function for clients handling
 */
int handleClient(int clntSocket);

/**
 * Checks the first char in the buffer and delegates the parsing to analyzeWord
 */
int analyzeString(char buffer[]);

/**
 * Compares the string in buffer, starting from position i, to a specified command
 */
int analyzeWord( int i, char buffer[], char* command );

#endif