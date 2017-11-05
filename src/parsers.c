#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "proxyUtilities.h"
#include "parsers.h"
#include "buffer.h"

extern struct Metrics   metrics;
extern struct Settings  settings;


int parseClientCommand(char b[]){
    int     idx  = 0;
    char    c;

    c = b[idx];
    switch( toupper(c) ){
        case 'U':   if ( analyzeString( b+1, "SER" ) == 1 ) {
                        return 0;
                    }
                    else 
                        return 3;
        case 'R':   if ( analyzeString( b+1, "ETR" ) == 1 )
                        return 1;
                    else 
                        return 3;
        case 'C':   if ( analyzeString( b+1, "APA" ) == 1 )
                        return 2;
                    else 
                        return 3;
        default:    return 3;
    }
}

int analyzeString( const char buffer[], const char *command) {
    enum StringState { READ = 0, RIGHT, WRONG };
    
    int  bufIndex  = 0; 
    int  comIndex  = 0;
    int  state     = READ;
    while ( state == READ ){
        char uppercaseLetter = toupper( buffer[bufIndex] );
        if ( command[comIndex] == '\0' && ( uppercaseLetter == '\n' || uppercaseLetter == ' ' ) ) {
            state = RIGHT;
        }
        else if ( uppercaseLetter == '\0' || ( uppercaseLetter != command[comIndex] ) ) {
            state = WRONG;
        }
        comIndex++;
        bufIndex++;
    }
    return state;
}

int commandLength( const char b[] ){
    int len = 0;
    for( int i = 0; b[i] != '\n' && b[i] != '\0'; i++)
        len++;
    return len;
}

int parseServerForCAPA(char b[]){
    for (int i = 0; b[i] != '.'; i++) {
        if ( b[i] == 'P' ) {
            if ( strncmp( b+i+1, "IPELINING", 9) == 0 ) {
                return 1;
            }
        }
    }
    return 0;
}

//Almost nothing is implemented
int parseConfigCommand(char b[]){
    enum Response { ERROR = 0, OK, GCC, GHA, GTB, VN, PP, MP };
    
    //It only compares the first 3 or 2 chars of the command. Check later.
    if ( strncmp( b, "OS ", 3) == 0 ){                                         //OK
        //Set origin server
        settings.origin_server = malloc (strlen(b+3));
        memcpy(settings.origin_server, b+3, strlen(b+3)+1);
        printf("---New origin server: %s---\n", settings.origin_server);
        return OK;
    }
    else if ( strncmp( b, "EF ", 3) == 0 ){                                    //OK, but not used yet
        //Set error file
        settings.error_file = malloc (strlen(b+3));
        memcpy(settings.error_file, b+3, strlen(b+3)+1);
        printf("---New error file: %s---\n", settings.error_file);
        return OK;
    }
    else if ( strncmp( b, "PA ", 3) == 0 ){                                    //WTF?
        //Set POP3 address
        return OK;
    }
    else if ( strncmp( b, "MA ", 3) == 0 ){                                    //WTF? 
        //Set management address
        return OK;
    }
    else if ( strncmp( b, "RM ", 3) == 0 ){                                    //OK, but not used yet
        //Set replacement message
        settings.replacement_message = malloc (strlen(b+3));
        memcpy(settings.replacement_message, b+3, strlen(b+3)+1);
        return OK;
    }
    else if ( strncmp( b, "CT ", 3) == 0 ){                                    //OK, but not used yet
        //Set censurable types
        settings.censurable = malloc (strlen(b+3));
        memcpy(settings.censurable, b+3, strlen(b+3)+1);
        return OK;
    }
    else if ( strncmp( b, "MP ", 3) == 0 ){                                    //OK
        //Set management port
        if( settings.management_port == stringToInt(b+3))
            return OK;
        settings.management_port = stringToInt(b+3);
        printf("---New management port: %d---\n", settings.management_port);
        return MP;
    }
    else if ( strncmp( b, "PP ", 3) == 0 ){                                    //OK. Note: TCP socket has TIME_WAIT
        //Set POP3 port
        if ( settings.pop3_port == stringToInt(b+3) )
            return OK;
        settings.pop3_port = stringToInt(b+3);
        printf("---New POP3 port: %d---\n", settings.pop3_port);
        return PP;
    }
    else if ( strncmp( b, "OP ", 3) == 0 ){                                    //OK
        //Set origin port
        settings.origin_port = stringToInt(b+3);
        printf("---New origin port: %d---\n", settings.origin_port);
        return OK;
    }
    else if ( strncmp( b, "CD ", 3) == 0 ){                                    //OK, but not used yet
        //Set command
        settings.cmd = malloc (strlen(b+3));
        memcpy(settings.cmd, b+3, strlen(b+3)+1);
        return OK;
    }
    else if ( strncmp( b, "SCC", 3) == 0 ){   
        //Set concurrent connections metrics on
        metrics.cc_on = 1;
        return OK;
    }
    else if ( strncmp( b, "RCC", 3) == 0 ){
        //Set concurrent connections metrics on (reset)
        metrics.cc_on = 0;
        return OK;
    }
    else if ( strncmp( b, "GCC", 3) == 0 ){
        //Get concurrent connections metrics
        return GCC;
    }
    else if ( strncmp( b, "SHA", 3) == 0 ){
        //Set historical accesses metrics on
        metrics.ha_on = 1;
        return OK;
    }
    else if ( strncmp( b, "RHA", 3) == 0 ){
        //Set historical accesses metrics on (reset)
        metrics.ha_on = 0;
        return OK;
    }
    else if ( strncmp( b, "GHA", 3) == 0 ){
        //Get historical accesses metrics
        return GHA;
    }
    else if ( strncmp( b, "STB", 3) == 0 ){
        //Set transfered bytes metrics on
        metrics.tb_on = 1;
        return OK;
    }
    else if ( strncmp( b, "RTB", 3) == 0 ){
        //Set transfered bytes metrics on (reset)
        metrics.tb_on = 0;
        return OK;
    }
    else if ( strncmp( b, "GTB", 3) == 0 ){                                     //TB not implemented
        //Get transfered bytes metrics
        return GTB;
    }
    else if ( strncmp( b, "VN", 2) == 0 ){
        //Get version
        return VN;
    }
    else
        return ERROR;
}

int stringToInt(char b[]){
    int dec = 0;
    for(int i=0; b[i] >= 48 && b[i] <= 57; i++){
        dec = dec * 10 + ( b[i] - '0' );
    }
    return dec;    
}

void intToString(int n, char b[]){
    char aux;
    int i = 0;
    //Max int enters in array size 10 + \0
    for( int j = 0; j < 11; j++)
        b[j] = '\0';
    if(n == 0) {
        b[i] = 48;
        return;
    }
    while(n > 0){
        aux = (n % 10) + 48;
        for( int j = i; j > 0; j--)
            b[j] = b[j-1];
        b[0] = aux;
        n = n/10;
        i++;
    }
}