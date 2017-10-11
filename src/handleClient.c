#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "handleClient.h"

//There are separated defines because perhaps there are necessary in the future
#define ERROR 0
#define USER 1
#define PASS 2
#define QUIT 3
#define STAT 4
#define LIST 5
#define RETR 6
#define DELE 7
#define NOOP 8
#define RSET 9

int handleClient(int clntSocket, char* buffer, int bufsize) {
  int valread, valsend, analyzeResponse;
  char bufferAux[bufsize];
  char* respStr = NULL;

  if ((valread = read( clntSocket , buffer, bufsize)) <= 0){
    return valread;
  }
  else{
    //copy buffer to bufferAux for now...
    //set the string terminating NULL byte on the end of the data read
    memcpy(bufferAux, buffer, bufsize);
    bufferAux[valread] = '\0';
    analyzeResponse = analyzeString( bufferAux );
    //There are separated printf for each case because perhaps there are necessary in the future...
    switch ( analyzeResponse ){
      case ERROR: ;
                  break;
      default:    ;  
                  break;
    }
  }
  return 1;
}

int analyzeString(char buffer[]){
  char c = buffer[0];
  //Check commands list. For those commands with the same first letter, check the second one.
  switch( c ){
      case 'U':
      case 'u': if ( analyzeWord( 1, buffer, "SER" ) == 1 )
                  return USER;
                else 
                  return ERROR;
      case 'P':
      case 'p': if ( analyzeWord( 1, buffer, "ASS" ) == 1 ) //LOL
                  return PASS;
                else 
                  return ERROR;
      case 'Q':
      case 'q': if ( analyzeWord( 1, buffer, "UIT" ) == 1 )
                  return QUIT;
                else 
                  return ERROR;
      case 'S':
      case 's': if ( analyzeWord( 1, buffer, "TAT" ) == 1 )
                  return STAT;
                else 
                  return ERROR;
      case 'L':
      case 'l': if ( analyzeWord( 1, buffer, "IST" ) == 1 )
                  return LIST;
                else 
                  return ERROR;
      case 'D':
      case 'd': if ( analyzeWord( 1, buffer, "ELE" ) == 1 )
                  return DELE;
                else 
                  return ERROR;
      case 'N':
      case 'n': if ( analyzeWord( 1, buffer, "OOP" ) == 1 )
                  return NOOP;
                else 
                  return ERROR;
      case 'R':
      case 'r': c = buffer[1];
                if ( (c == 'S' || c == 's') && analyzeWord( 2, buffer, "ET" ) == 1 )
                  return RSET;
                else if ( (c == 'E' || c == 'e') && analyzeWord( 2, buffer, "TR" ) == 1 )
                  return RETR;
                else 
                  return ERROR;
      default:  return ERROR;
  }
}

//Based on POP3 behavior. Some things are permitted, like spaces after commands,
//ending a command with no args even if they are necessary, etc. Check with POP3 server responses and compare...
int analyzeWord(int i, char buffer[], char* command){
  int bufIndex = i, comIndex = 0;
  while ( 1 ){
    char uppercaseLetter = toupper( buffer[bufIndex] );
    if ( command[comIndex] == '\0' && ( uppercaseLetter == '\n' || uppercaseLetter == ' ' ) )
      return 1;
    else if ( uppercaseLetter == '\0' || ( uppercaseLetter != command[comIndex] ) )
      return 0;
    comIndex++;
    bufIndex++;
  }
}