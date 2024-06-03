#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "util.h"

int client_socket = -1;
char username[MAX_NAME_LEN + 2];
char inbuf[BUFLEN + 1];
char outbuf[MAX_MSG_LEN + 1];
//my port: 11091

int main(int argc, char **argv) {
    int client_socket, bytes_recvd, ip_conversion, retval = EXIT_SUCCESS;
    struct sockaddr_in server_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    if(argc != 3){//check if correct # of args
        fprintf(stderr, "Usage: %s <server IP> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if((ip_conversion = inet_pton(AF_INET, argv[1], 
			&server_addr.sin_addr))==-1){//check if valid IP
        fprintf(stderr, "Invalid IP address '%s'.\n", argv[1]);
        return EXIT_FAILURE;
    }else if(ip_conversion ==0){
       fprintf(stderr, "Couldn't convert IP address '%s'.\n", argv[1]);
        return EXIT_FAILURE;
    }
    if((atoi(argv[2])<1024) || (atoi(argv[2])>65535)){//check if port is in good range
        fprintf(stderr, "Usage: %s <server IP> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    username[0] = '\0';
    while (strlen(username) == 0)
    {
        printf("Enter your username: ");
        if (fgets(username, MAX_NAME_LEN + 2, stdin) != NULL)
        {
            char *p;
            if ((p = strchr(username, '\n')))
            { // check exist newline
                *p = 0;
            }
            else
            {
                scanf("%*[^\n]");
                scanf("%*c"); // clear upto newline
                username[0] = '\0';
                printf("Sorry, limit your username to %d characters.\n", MAX_NAME_LEN);
            }
        } else {
            //just close because the user is doing something wrong
            _exit(EXIT_FAILURE);
        }
    }
    printf("Hello, %s. Let's try to connect to the server.\n", username);
    
    //created TCP scoket
    if((client_socket = socket(AF_INET, SOCK_STREAM, 0))<0){
        fprintf(stderr, "Error: Failed to create socket. %s. \n",strerror(errno));
	retval = EXIT_FAILURE;
	goto EXIT;
    }
     
    //connect to server
    memset(&server_addr, 0, addrlen);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((short) atoi(argv[2]));
    
    if (connect(client_socket, (struct sockaddr *)&server_addr, addrlen) < 0) {
            fprintf(stderr, "Error: Failed to connect to server. %s.\n",
	        strerror(errno));
	    retval = EXIT_FAILURE;
	    goto EXIT;
    }

    //welcome message from server
    printf("\n");
    if (send(client_socket, outbuf, strlen(outbuf), 0) < 0) {
	    fprintf(stderr, "1. Error: Failed to send message to server. %s.\n",
	    strerror(errno));
	    retval = EXIT_FAILURE;
	    goto EXIT;
    }
    if ((bytes_recvd = recv(client_socket, inbuf, BUFLEN - 1, 0)) < 0) {
	    fprintf(stderr, "Error: Failed to receive message from server. %s.\n",
	    strerror(errno));
	    retval = EXIT_FAILURE;
	    goto EXIT;
    }
    inbuf[bytes_recvd] = '\0';
    printf("%s\n\n", inbuf);
    fflush(stdout);

    //sends username to server
    if (send(client_socket, username, strlen(username) + 1, 0) < 0) {
        fprintf(stderr, "2. Error: Failed to send message to server. %s.\n",
        strerror(errno));
        retval = EXIT_FAILURE;
        goto EXIT;
    }
    fd_set sockset;

    while(1){
	    FD_ZERO(&sockset);
        FD_SET(STDIN_FILENO, &sockset);
        FD_SET(client_socket, &sockset);
        if (isatty(STDIN_FILENO)) {
            printf("[%s]: ", username);
        }
        fflush(stdout);
	    //check for highest fd and assign it as such 
	    int maxfd;
	    if (client_socket > STDIN_FILENO) {
            maxfd = client_socket;
	    } else {
            maxfd = STDIN_FILENO;
	    }
	    //check for activity on file descriptors 
	    if (select(maxfd+1, &sockset, NULL, NULL, NULL) < 0) {
            perror("select failed");
            retval = EXIT_FAILURE;
            goto EXIT;
        }
	    if (FD_ISSET(STDIN_FILENO, &sockset)){ //check for activity on STDIN
	        //prompt user for input
            if (fgets(outbuf, MAX_MSG_LEN, stdin) == NULL) {
                //fprintf(stderr, "Error: fgets() failed.\n");
                retval = EXIT_SUCCESS;
                goto EXIT;
	        }

            outbuf[strlen(outbuf)-1] = '\0'; // remove trailing newline
            if (strlen(outbuf) > MAX_MSG_LEN) {
                    printf("Sorry, limit your message to 1 line of at most  %d characters.\n", 
                    MAX_MSG_LEN);
            }else if(strlen(outbuf)==0){
                goto CLIENTSOCKET;

            }
            //sends message to server
            if (send(client_socket, outbuf, strlen(outbuf)+1, 0) < 0) {
                    fprintf(stderr, "2. Error: Failed to send message to server. %s.\n",
                    strerror(errno));
                retval = EXIT_FAILURE;
                goto EXIT;
            }
            if(!strcmp(outbuf,"bye")){//check if user wants to leave
                    printf("Goodbye.\n");
            fflush(stdout);
                    retval = EXIT_SUCCESS;
                    goto EXIT;
            }	
	    }
CLIENTSOCKET:
	
        if(FD_ISSET(client_socket, &sockset)){ //check for activity on client_socket
            if (((bytes_recvd = recv(client_socket, inbuf, BUFLEN - 1, 0)) < 0)
                    && (errno != EINTR)) {
                fprintf(stderr, "Warning: Failed to receive incoming message. %s.\n",
                    strerror(errno));
            } else if(bytes_recvd ==  0) {
                fprintf(stderr, "\nConnection to server has been lost.\n");
                retval = EXIT_FAILURE;
                goto EXIT;
            } else {	    
                inbuf[bytes_recvd] = '\0';
                printf("\n%s\n", inbuf);
                fflush(stdout);
            }
        }
    } //end of while loop
EXIT:
    // If client socket has a file descriptor, close it.
    if (fcntl(client_socket, F_GETFD) != -1) {
        close(client_socket);
    }
    return retval;
}  
