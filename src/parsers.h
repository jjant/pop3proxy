#ifndef PARSERS_H
#define PARSERS_H


/**
 * Analyse command to see if client wrote USER, RETR, or CAPA
 */
int parseClientCommand(char b[]);
int analyzeString(const char b[], const char*);
int commandLength(const char b[]);

/**
 * Search for PIPELINE capability
 */
int parseServerForCAPA(char b[]);

/**
 * Parse command of configuration and apply it
 */
int parseConfigCommand(char b[]);

int stringToInt(const char b[]);

void intToString(int n, char[]);

#endif