//
// request.c: Does the bulk of the work for the web server.
// 

#include "segel.h"
#include "request.h"
#include "request_queue.h"

extern pthread_mutex_t queue_mutex;
extern pthread_cond_t queue_not_full;
extern pthread_cond_t queue_not_empty;
extern pthread_cond_t queue_empty;
extern request_queue_t incoming_queue;
extern int total_capacity;


// requestError(      fd,    filename,        "404",    "Not found", "OS-HW3 Server could not find this file");
void requestError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg, struct timeval arrival, struct timeval dispatch, thread_stats_t *t_stats)
{
    char buf[MAXLINE], body[MAXBUF];

    // Create the body of the error message
    sprintf(body, "<html><title>OS-HW3 Error</title>");
    //TODO: check what is correct
//    sprintf(body, "%s<body bgcolor=\"fffff\">\r\n", body);
    sprintf(body, "%s<body bgcolor=""fffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr>OS-HW3 Web Server\r\n", body);

    // Write out the header information for this response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);

    sprintf(buf, "Content-Type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);

    sprintf(buf, "Content-Length: %lu\r\n", strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);

    // Add the required statistics headers
    //TODO: check what is correct
//    sprintf(buf, "Stat-Req-Arrival: %ld.%06ld\r\n", arrival.tv_sec, arrival.tv_usec);
    sprintf(buf, "Stat-Req-Arrival:: %ld.%06ld\r\n", arrival.tv_sec, arrival.tv_usec);
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);

    sprintf(buf, "Stat-Req-Dispatch:: %ld.%06ld\r\n", dispatch.tv_sec, dispatch.tv_usec);
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);

    sprintf(buf, "Stat-Thread-Id:: %d\r\n", t_stats->id);
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);

    sprintf(buf, "Stat-Thread-Count:: %d\r\n", t_stats->total_req);
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);

    sprintf(buf, "Stat-Thread-Static:: %d\r\n", t_stats->stat_req);
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);

    sprintf(buf, "Stat-Thread-Dynamic:: %d\r\n\r\n", t_stats->dynm_req);
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);


    // Write out the content
    Rio_writen(fd, body, strlen(body));
    printf("%s", body);
}


//
// Reads and discards everything up to an empty text line
//
void requestReadhdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
    }
    return;
}

//
// Return 1 if static, 0 if dynamic content
// Calculates filename (and cgiargs, for dynamic) from uri
//
int requestParseURI(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (strstr(uri, "..")) {
        sprintf(filename, "./public/home.html");
        return 1;
    }

    if (!strstr(uri, "cgi")) {
        // static
        strcpy(cgiargs, "");
        sprintf(filename, "./public/%s", uri);
        if (uri[strlen(uri)-1] == '/') {
            strcat(filename, "home.html");
        }
        return 1;
    } else {
        // dynamic
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        } else {
            strcpy(cgiargs, "");
        }
        sprintf(filename, "./public/%s", uri);
        return 0;
    }
}

void increaseStaticReq(thread_stats_t *t_stats) {
    t_stats->stat_req++;
}

void increaseDynamicReq(thread_stats_t *t_stats) {
    t_stats->dynm_req++;
}


//
// Fills in the filetype given the filename
//
void requestGetFiletype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

void requestServeDynamic(int fd, char *filename, char *cgiargs, struct timeval arrival, struct timeval dispatch, thread_stats_t *t_stats) {
    char buf[MAXLINE], *emptylist[] = {NULL};

    // The server does only a little bit of the header.
    // The CGI script has to finish writing out the header.
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: OS-HW3 Web Server\r\n", buf);
    sprintf(buf, "%sStat-Req-Arrival:: %ld.%06ld\r\n", buf, arrival.tv_sec, arrival.tv_usec);
    sprintf(buf, "%sStat-Req-Dispatch:: %ld.%06ld\r\n", buf, dispatch.tv_sec, dispatch.tv_usec);
    sprintf(buf, "%sStat-Thread-Id:: %d\r\n", buf, t_stats->id);
    sprintf(buf, "%sStat-Thread-Count:: %d\r\n", buf, t_stats->total_req);
    sprintf(buf, "%sStat-Thread-Static:: %d\r\n", buf, t_stats->stat_req);
    sprintf(buf, "%sStat-Thread-Dynamic:: %d\r\n", buf, t_stats->dynm_req);


    Rio_writen(fd, buf, strlen(buf));
    int pid = 0;
    if ((pid = Fork()) == 0) {
        /* Child process */
        Setenv("QUERY_STRING", cgiargs, 1);
        /* When the CGI process writes to stdout, it will instead go to the socket */
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
    WaitPid(pid, NULL, WUNTRACED);
}

void requestServeStatic(int fd, char *filename, int filesize, struct timeval arrival, struct timeval dispatch, thread_stats_t *t_stats)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    requestGetFiletype(filename, filetype);

    srcfd = Open(filename, O_RDONLY, 0);

    // Rather than call read() to read the file into memory,
    // which would require that we allocate a buffer, we memory-map the file
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);

    // put together response
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: OS-HW3 Web Server\r\n", buf);
    sprintf(buf, "%sContent-Length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-Type: %s\r\n", buf, filetype);
    sprintf(buf, "%sStat-Req-Arrival:: %ld.%06ld\r\n", buf, arrival.tv_sec, arrival.tv_usec);
    sprintf(buf, "%sStat-Req-Dispatch:: %ld.%06ld\r\n", buf, dispatch.tv_sec, dispatch.tv_usec);
    sprintf(buf, "%sStat-Thread-Id:: %d\r\n", buf, t_stats->id);
    sprintf(buf, "%sStat-Thread-Count:: %d\r\n", buf, t_stats->total_req);
    sprintf(buf, "%sStat-Thread-Static:: %d\r\n", buf, t_stats->stat_req);
    sprintf(buf, "%sStat-Thread-Dynamic:: %d\r\n\r\n", buf, t_stats->dynm_req);

    Rio_writen(fd, buf, strlen(buf));

    //  Writes out to the client socket the memory-mapped file
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}
// handle a request
// request.c

void requestHandle(int fd, struct timeval arrival, struct timeval dispatch, thread_stats_t *t_stats) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    printf("%s %s %s\n", method, uri, version);

    if (strcasecmp(method, "GET")) {
        requestError(fd, method, "501", "Not Implemented", "OS-HW3 Server does not implement this method", arrival, dispatch, t_stats);
        Close(fd);
        return;
    }
    requestReadhdrs(&rio);

    // Check for .skip suffix in the URI
    char *skip_suffix = strstr(uri, ".skip");
    if (skip_suffix && strcmp(skip_suffix, ".skip") == 0) {
        *skip_suffix = '\0'; // Remove the .skip suffix

        pthread_mutex_lock(&queue_mutex);
        // Dequeue the last request in the queue
        struct timeval last_arrival;
        int last_fd = dequeue_last(&incoming_queue, &last_arrival);
        pthread_mutex_unlock(&queue_mutex);


        // Parse the URI after removing the .skip suffix
        is_static = requestParseURI(uri, filename, cgiargs);

        // Handle the current request (with .skip suffix removed)
        if (stat(filename, &sbuf) < 0) {
            requestError(fd, filename, "404", "Not found", "OS-HW3 Server could not find this file", arrival, dispatch, t_stats);
            Close(fd);
            return;
        }
        if (is_static) {
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
                requestError(fd, filename, "403", "Forbidden", "OS-HW3 Server could not read this file", arrival, dispatch, t_stats);
                Close(fd);
                return;
            }
            increaseStaticReq(t_stats);
            requestServeStatic(fd, filename, sbuf.st_size, arrival, dispatch, t_stats);
        } else {
            if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
                requestError(fd, filename, "403", "Forbidden", "OS-HW3 Server could not run this CGI program", arrival, dispatch, t_stats);
                Close(fd);
                return;
            }
            increaseDynamicReq(t_stats);
            requestServeDynamic(fd, filename, cgiargs, arrival, dispatch, t_stats);
        }
        Close(fd);

        // Handle the dequeued last request
        if (last_fd != -1) {
            struct timeval last_dispatch;
            gettimeofday(&last_dispatch, NULL);
            t_stats->total_req++;
            requestHandle(last_fd, last_arrival, last_dispatch, t_stats);
        }
        return;
    }

    // Parse the URI as usual
    is_static = requestParseURI(uri, filename, cgiargs);

    // Handle the request as usual
    if (stat(filename, &sbuf) < 0) {
        requestError(fd, filename, "404", "Not found", "OS-HW3 Server could not find this file", arrival, dispatch, t_stats);
        Close(fd);
        return;
    }
    if (is_static) {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            requestError(fd, filename, "403", "Forbidden", "OS-HW3 Server could not read this file", arrival, dispatch, t_stats);
            Close(fd);
            return;
        }
        increaseStaticReq(t_stats);
        requestServeStatic(fd, filename, sbuf.st_size, arrival, dispatch, t_stats);
    } else {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            requestError(fd, filename, "403", "Forbidden", "OS-HW3 Server could not run this CGI program", arrival, dispatch, t_stats);
            Close(fd);
            return;
        }
        increaseDynamicReq(t_stats);
        requestServeDynamic(fd, filename, cgiargs, arrival, dispatch, t_stats);
    }

    Close(fd);
}
