/* myhttpd.c
 * Authod:      Desmond Qiu
 * Version:     1.0, 17-11-19
 *              1.1, 23-11-19, clean up code for submission
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

// constant buffer sizes
#define L_BUF_SIZE          8000
#define M_BUF_SIZE          2000
#define S_BUF_SIZE          500

// constant declarations
#define DEFAULT_ROOT_DIR    "./"
#define DEFAULT_PORT_NO     8000
#define DEFAULT_LOG_FILE    "myhttpd.log"
#define DEFAULT_PREFORK     5
#define MAX_QUEUE           5

// method prototypes
void daemon_init(void);
void claim_children();
void serve_request(int , char *host);
void datetime(char stime[]);
void request_log(char *method, char *host, char *resource, char *status);
void server_err(char *errorMsg, ...);

// global variables
char *rootdir = DEFAULT_ROOT_DIR;
int portno = DEFAULT_PORT_NO;
char *logfilename = DEFAULT_LOG_FILE;
int prefork = DEFAULT_PREFORK;
FILE *logfile;

int main(int argc, char *argv[])
{   
    // Check option
    int option;
    while ((option = getopt(argc, argv, "p:d:l:m:f:")) != -1) {
        switch (option) {
        case 'p': // port number
            //fprintf(stdout, "P option detected\n");
            if (atoi(optarg) < 1024) {
                server_err("[ERROR] Port number not valid");
            }
            else {
                portno = atoi(optarg);
            }
            break;
        case 'd': // document root directory
            //fprintf(stdout, "D option detected\n");
            //fprintf(stdout, "%s\n", optarg);
            if (chdir(optarg) == -1) {
                server_err("[ERROR] Unable to open directory");
            } 
            else {
                rootdir = optarg;
            } 
            break;
        case 'l': // log file
            //fprintf(stdout, "L option detected\n");
            logfilename = optarg;
            break;
        case 'm': // file for mime types
            //fprintf(stdout, "M option detected\n");
            break;
        case 'f': // number of preforks
            //fprintf(stdout, "F option detected\n");
            if ((atoi(optarg) == 0) && (atoi(optarg) <= 0)) {
                server_err("[ERROR] Prefork amount is not valid");
            }
            else {
                prefork = atoi(optarg);
            }
            break;
        case '?':
            server_err("[ERROR] One of the option is not supported");
            break;
        }
    }

    logfile = fopen(logfilename, "w+");
    if (!logfile) {
        server_err("[ERROR] Opening log file");
    }

    // init as daemon
    daemon_init();

    // cheap solution
    // signals to ignore to prevent zombie processes
    //signal(SIGPIPE, SIG_IGN);
    //signal(SIGCHLD, SIG_IGN);
    // took over by claim_children

    // get time and date
    char now[S_BUF_SIZE];
    datetime(now);

    // show server pid
    fprintf(stdout, "[INFO] SERVER STARTED AT %s\n", now);
    fprintf(stdout, "[REFERENCE] SERVER PID IS %d\n", getpid());
    fprintf(stdout, "[INFO] LISTENING TO PORT %d\n", portno);
    fprintf(stdout, "[INFO] LOGGING TO %s%s\n", rootdir, logfilename);

    // log server start time
    fprintf(logfile, "[INFO] SERVER STARTED AT %s\n", now);
    fprintf(logfile, "[CONFIG] ROOT DIR: %s\n", rootdir);
    fprintf(logfile, "[CONFIG] LISTENING TO PORT: %d\n", portno);
    fprintf(logfile, "[CONFIG] LOG FILENAME: %s\n", logfilename);
    fprintf(logfile, "[CONFIG] TOTAL PREFORK: %d\n", prefork);
    fflush(logfile);

    // create socket
    int sd;
    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        server_err("[ERROR] Create socket fail");
    }

    // build address socket
    struct sockaddr_in ser_addr, cli_addr;
    bzero((char*)&ser_addr, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(portno);
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind socket to address
    if (bind(sd, (struct sockaddr *) &ser_addr, sizeof(ser_addr)) < 0) {
        server_err("[ERROR] Bind address fail");
    }

    int pid;
    for (int i = 0; i < prefork; i++) {
        pid = fork();
        if (pid == 0) { // child process created
            fprintf(stdout, "PF process [%d] of Server [%d] created\n", 
                getpid(), getppid());
            break;
        }
    }

    // parent wait for child processes
    if (pid > 0) { 
        wait(NULL);
        exit(EXIT_SUCCESS);
    }

    // listening for connections
    fprintf(stdout, 
        "PF process [%d] waiting for connection..\n", getpid());
    if (listen(sd, MAX_QUEUE) < 0) {
        server_err("[ERROR] Unable to setup listener");
    }

    int cli_addrlen, nsd, wpid;
    while (1) {
        char *host;
        cli_addrlen = sizeof(cli_addr);
        nsd = accept(sd, (struct sockaddr *) &cli_addr, &cli_addrlen);
        if (nsd < 0) {
            if (errno == EINTR)
                continue;
            perror("[ERROR] Unable to accept connection\n");
            exit(EXIT_FAILURE);
        }
        else { 
            host = inet_ntoa(cli_addr.sin_addr);
            fprintf(stdout, 
                "PF process [%d] accepted Client [%s]\n", getpid(), host);
        }

        wpid = fork();
        if (wpid < 0) {
            perror("[ERROR] Worker fail to fork\n");
            exit(EXIT_FAILURE);
        }
        else if (wpid > 0) {
            close(nsd); // parent close unused socket
            continue;
        }

        // child continues to serve client
        close(sd); // close old socket
        //serve_client(nsd, log);
        serve_request(nsd, host);
        close(nsd); // close new socket
        printf("Worker process [%d] of PF process [%d] exit\n", 
            getpid(), getppid());
        
        exit(EXIT_SUCCESS);
    }
}

void serve_request(int sd, char *host)
{
    int nread, nwrite;
    char buffer[L_BUF_SIZE];
    
    if ((nread = read(sd, buffer, sizeof(buffer))) <= 0) {
        exit(EXIT_SUCCESS);
    }

    // add null termination
    buffer[nread] = '\0';
    
    // duplicate a copy of the request
    char *copy_req_msg = malloc(strlen(buffer) + 1);
    strcpy(copy_req_msg, buffer);
    
    //tokenise the request
    char *method, *resource;
    //extract methods
    method = strtok(buffer, " ");
    
    //extract resource
    resource = strtok(NULL, " ");

    fprintf(stdout, "[REQUEST INFO]\nmethod=%s\nresource=%s\n", 
        method, resource);

    // store the server response
    char response[L_BUF_SIZE], statusline[S_BUF_SIZE], 
        header[L_BUF_SIZE], body[L_BUF_SIZE];
    
    // http version and status message
    char *httpver = "HTTP/1.1", *status, *status_msg;
    
    // booleans for checking if file is valid, dir or reg file
    bool isValid = false, isReg = false, isDir = false;
    
    // check if resource is a file or directory using stat
    struct stat info;
    if (stat (resource, &info) == 0) {
        isValid = true;
        if (info.st_mode & S_IFREG) {
            isReg = true;
        }
        else if (info.st_mode & S_IFDIR) {
            isDir = true;
        }
    }
    else {
        status = "400"; status_msg = "Bad Request";
        sprintf(body, "%s %s\n", status, status_msg);
    }

    if (isValid) {
        if (strcasecmp(method, "GET") == 0) {
            //fprintf(stdout, "[REQUEST -> GET %s]\n", resource);

            // if directory
            // check for index.html, index.htm, default.htm
            // if index missing, display all files in working directory
            if (isDir) {
                // assume index is missing
                char dirListing[M_BUF_SIZE], format[S_BUF_SIZE];
                int fn_len = 0;
                struct dirent *entry;
                DIR *dir;
                if ((dir = opendir(resource)) != NULL) {
                    //fprintf(stdout, "Directory listing:\n");
                    sprintf(format, "Directory Listing:\n");
                    strcpy(dirListing + fn_len, format);
                    fn_len += strlen(format);

                    while((entry = readdir(dir)) != NULL) {
                        if ((strcmp(entry->d_name, ".") == 0) ||
                            (strcmp(entry->d_name, "..") == 0)) { 
                            // do nothing
                        } 
                        else {
                            sprintf(format, "%s  ", entry->d_name);
                            strcpy(dirListing + fn_len, format);
                            fn_len += strlen(format);
                        }
                    }
                    closedir(dir);

                    status = "200"; status_msg = "OK";
                    sprintf(body, "%s\n", dirListing);
                }
                else {
                    perror("ERROR: Unable to open directory");
                    status = "404"; status_msg = "Not Found";
                    sprintf(body, "%s %s\n", status, status_msg);
                } // end opendir
            } // end isDir
        }
        else if (strcasecmp(method, "TRACE") == 0) {
            //fprintf(stdout, "[REQUEST -> TRACE]\n");
            status = "200"; status_msg = "OK";
            sprintf(body, "%s %s\n", status, status_msg);
        }
        else if (strcasecmp(method, "HEAD") == 0) {
            //fprintf(stdout, "[REQUEST -> HEAD]\n");
            status = "200"; status_msg = "OK";
            sprintf(body, "%s %s\n", status, status_msg);
        }
        else {
            fprintf(stderr, "Method not supported\n");
            // send error code 'method not supported'
            status = "405"; status_msg = "Method Not Allowed";
            sprintf(body, "%s %s\n", status, status_msg);
        } // end method handler
    } // end isValid

    // header date time meta
    char now[S_BUF_SIZE];
    datetime(now);

    // build statusline
    sprintf(statusline, "%s %s %s\n", httpver, status, status_msg);
    // add statusline to header
    sprintf(header, "%sContent-Type: text/plain\nDate: %s\nContent-Length: %d\n", 
        statusline, now, (int) strlen(body));
    // combine header and body
    sprintf(response, "%s\n%s", header, body);

    // write to client
    if (strcasecmp(method, "head") == 0)
        nwrite = write(sd, header, strlen(header));
    else if (strcasecmp(method, "trace") == 0)
        nwrite = write(sd, statusline, strlen(statusline));
    else if (strcasecmp(method, "get") == 0)
        nwrite = write(sd, response, strlen(response));

    //nwrite = write(sd, &response_test, strlen(response_test));
    
    // write to log
    request_log(method, host, resource, status);
    //request_log(method, host, response_test, status);

    // write to console
    fprintf(stdout, "\n[RESPONSE SENT]\n");
    fprintf(stdout, "%s", response);
    free(copy_req_msg);
}

void claim_children()
{
    pid_t pid=1;
    while (pid > 0) 
        pid = waitpid(0, (int *)0, WNOHANG);
}

void request_log(char *method, char *host, char *resource, char* status)
{
    char now[S_BUF_SIZE];
    datetime(now);
    fprintf(logfile, "[ACTIVITY] %s %s %s %s %s\n", now, method, host, resource, status);
    fflush(logfile);
}

void datetime(char stime[])
{
    time_t t = time(NULL);
    struct tm *p = localtime(&t);
    strftime(stime, 1000, "%a, %d %b %Y %H:%M:%S %z", p);
}

void daemon_init(void)
{       
    pid_t pid;
    if ((pid = fork()) < 0) {
        perror("ERROR: Fail to fork at daemon\n"); 
        exit(EXIT_FAILURE); 
    } else if (pid > 0) 
        exit(EXIT_SUCCESS); // parent exit
    
    /* child continues */
    setsid();   // become session leader
    chdir(rootdir); // change working directory
    umask(0);   // clear file mode creation mask

    struct sigaction act;
    act.sa_handler = claim_children;
    sigemptyset(&act.sa_mask);
    act.sa_flags   = SA_NOCLDSTOP;
    sigaction(SIGCHLD, (struct sigaction*) &act, (struct sigaction*) 0);
    sigaction(SIGPIPE, (struct sigaction*) &act, (struct sigaction*) 0);
}

void server_err(char *errorMsg, ...)
{
    perror(errorMsg);
    if (logfile)
        fprintf(logfile, "%s\n", errorMsg);
    fprintf(stderr, "[INFO] START SERVER TERMINATED\n");
    exit(EXIT_FAILURE);
}