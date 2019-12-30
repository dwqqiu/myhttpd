/* myhttp.c
 * Authod:      Desmond Qiu
 * Version:     1.0, 18-11-19
 *              1.1, 23-11-19, clean up code for submission
 */ 
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// constant buffer sizes
#define L_BUF_SIZE          8000
#define M_BUF_SIZE          2000
#define S_BUF_SIZE          500

// constant declarations
#define DEFAULT_PORT_NO     8000

int main(int argc, char *argv[]) 
{
    // show both header and content
    bool showAll = false;
    char *method, *ip_net, *resource, *host, *httpver = "HTTP/1.1";
    int portno;
    struct hostent *hp;

    if (argc == 1) {
        fprintf(stderr, "[ERROR] Insufficient arguments\n");
        fprintf(stderr, "Please follow these format:\n"
            "myhttp <url>\n"
            "myhttp -m <method> <url>\n"
            "myhttp -m <method> -a <url>\n");
        exit(EXIT_FAILURE);
    }
    else if (argc == 2) {
        // copy the url
        char url[S_BUF_SIZE];
        strcpy(url, argv[1]);

        // check if url has http://
        if (strstr(url, "http://") == NULL) {
            fprintf(stdout, "[ERROR] URL not valid\n");
            fprintf(stdout, "Please include 'http://' in the URL\n");
            exit(EXIT_FAILURE);
        }

        // check if last char is '/'
        int size = strlen(url);
        if ((strcmp(&url[size-1], "/")) != 0) {
            url[size] = '/';
            puts("added /");
            puts(url);
        }

        host = url;
        host+=7;

        if ((strstr(host, ":")) == NULL) {
            portno = DEFAULT_PORT_NO;
            host = strtok(host, "/");

            if ((resource = strtok(NULL, "")) == NULL) {
                resource = "/";
            }
        }
        else {
            host = strtok(host, ":");
            char *p = strtok(NULL, "/");
            portno = atoi(p);

            if ((resource = strtok(NULL, "")) == NULL) {
                resource = "/"; 
            }
        }

        method = "get";
        showAll = false;
    }
    else if (argc > 3) {
        // assign to first element
        int next = 1; 
        // validate -m
        if (strcasecmp(argv[next], "-m") == 0) {
            ++next;
            method = argv[next];
        }

        // validate -a
        ++next;
        if (strcasecmp(argv[next], "-a") == 0) {
            showAll = true;
            ++next;
        }

        // copy the url
        char url[S_BUF_SIZE];
        strcpy(url, argv[next]);

        // check if url has http://
        if (strstr(url, "http://") == NULL) {
            fprintf(stdout, "[ERROR] URL not valid\n");
            fprintf(stdout, "Please include 'http://' in the URL\n");
            exit(EXIT_FAILURE);
        }

        // check if last char is '/'
        int size = strlen(url);
        if ((strcmp(&url[size-1], "/")) != 0) {
            url[size] = '/';
        }

        host = url;
        host+=7;

        if ((strstr(host, ":")) == NULL) {
            portno = DEFAULT_PORT_NO;
            host = strtok(host, "/");

            if ((resource = strtok(NULL, "")) == NULL) {
                resource = "/";
            }
        }
        else {
            host = strtok(host, ":");
            char *p = strtok(NULL, "/");
            portno = atoi(p);

            if ((resource = strtok(NULL, "")) == NULL) {
                resource = "/"; 
            }
        }
    } // end argc check

    // build address socket
    struct sockaddr_in ser_addr, cli_addr;
    bzero((char *) &ser_addr, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(portno);
    if ((hp = gethostbyname(host)) == NULL) {
        fprintf(stderr, "[ERROR] Host %s not found\n", host);
        exit(EXIT_FAILURE);
    }
    ser_addr.sin_addr.s_addr = *(u_long *) hp->h_addr;

    // create socket
    int sd;
    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[ERROR] Create socket fail");
        exit(EXIT_FAILURE);
    }
    
    /* create TCP socket & connect socket to server address */
    sd = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(sd, (struct sockaddr *) &ser_addr, sizeof(ser_addr))<0) { 
        perror("[ERROR] Connect fail");
        exit(EXIT_FAILURE);
    }
    
    // variables to store request and response message
    int nread, nwrite;
    char response[L_BUF_SIZE];
    char request[L_BUF_SIZE];

    // construct request message
    sprintf(request, "%s %s %s\n"
        "Content-Type: text/plain\nContent-Length: 0", 
        method, resource, httpver);
    
    // write to server
    nwrite = write(sd, request, sizeof(request));
    fprintf(stdout, "[REQUEST SENT]\nmethod=%s\nresource=%s\n", 
        method, resource);

    // read from server
    nread = read(sd, response, L_BUF_SIZE); 
    response[nread] = '\0';

    // reconstruct response message for displaying to user
    char *body;
    if (showAll) {
        fprintf(stdout, "\n[RESPONSE RECEIVED]\n%s", response);
    }
    else {
        if ((body = strstr(response, "\n\n")) != NULL) {
            body+=2;
            fprintf(stdout, "\n[RESPONSE RECEIVED]\n%s", body);
        }
        else {
            fprintf(stdout, "\n[RESPONSE RECEIVED]\n%s", response);    
        }
    }
    exit(EXIT_SUCCESS);
}