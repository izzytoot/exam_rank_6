#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct s_client{
    int id;
    char msg[100000]; // Buffer to store partial messages
} t_client;

t_client clients[2048]; // Max fds

// To be used by select() - modifies the passed to it. The sets should be reset every loop.
fd_set readfds; // Temporary copy used by select(). 
fd_set writefds; // Temporary copy used by select(). 
fd_set activefds; // Master set - it keeps track of every socket currently open (the listener + all connected clients).

char buffread[100000];
char buffwrite[100000];

int max = 0;
int nextid = 0;

void err(char *str){
    write(2, str, strlen(str));
    exit(1);
}

void boradcast(int sendfd){
    for (int fd = 0; fd <= max; fd++){
		// only send if the fd is in the write_set and not in the send_set
        if (FD_ISSET(fd, &writefds) && (fd != sendfd))
            write(fd, buffwrite, strlen(buffwrite));
    }
}

// The code sets up the socket to listen on 127.0.0.1 (2130706433)

int main(int ac, char **av){
    if (ac != 2)
        err("Wrong numebr of arguments\n");
	
	// socket() creates the listener
    int sockfd = max = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        err("Fatal error\n");
    FD_ZERO(&activefds);

	// Add the listener to the master set so select() knows to watch for new connections.
    FD_SET(sockfd, &activefds);

    struct sockaddr_in servadrr;
    servadrr.sin_family = AF_INET;
    servadrr.sin_addr.s_addr = htonl(2130706433);
    servadrr.sin_port = htons(atoi(av[1]));

    if ((bind(sockfd, (const struct sockaddr *)&servadrr, sizeof(servadrr))) != 0) // bind() attaches the socket to the port.
        err("Fatal error\n");
    if (listen(sockfd, 10) != 0) // listen() puts the socket in a state where it can accept incoming connections.
        err("Fatal error\n");

    while (1){
        readfds = writefds = activefds;

		// select() blocks the program until something happens - a new connection, a message received, or a socket becoming ready to write.
        // max + 1 - the range of FDs to check
		if (select(max + 1, &readfds, &writefds, 0, 0) < 0)
            continue;
        
        for (int fd = 0; fd <= max; fd++){

            if (!FD_ISSET(fd, &readfds))
                continue;

			// Scenario A - new connection (fd == sockfd):
            if (fd == sockfd){
                int clientsock = accept(fd, 0, 0); // accept() the connection.

				if (clientsock > max)
					max = clientsock; // max is now clientsock
                clients[clientsock].id = nextid++; // assign a nextid++.
                
				// reset msg to 0
                bzero(clients[clientsock].msg, strlen(clients[clientsock].msg));

				// add the new FD to activefds
                FD_SET(clientsock, &activefds);

				// notify everyone
                sprintf(buffwrite, "server: client %d just arrived\n", clients[clientsock].id);
                boradcast(clientsock);
            } 
			// Scenario B: client disconnecting (recv <= 0)
			else {
                int read = recv(fd, buffread, sizeof(buffread), 0);
                if (read <= 0){
					// notify everyone
                    sprintf(buffwrite, "server: client %d just left\n", clients[fd].id);
                    boradcast(fd);

					// stop watching this FD and close it
                    FD_CLR(fd, &activefds);
                    close(fd);
                } 
				// Scenario C: client sending a message ()´´
				else{
                    for (int i = 0, j = strlen(clients[fd].msg); i < read; i++, j++){
						// append the received buffread into the clients[fd].msg buffer
                        clients[fd].msg[j] = buffread[i];

						// look for \n
                        if (clients[fd].msg[j] == '\n'){

							// format the string and call broadcast it
                            clients[fd].msg[j] = 0;
                            sprintf(buffwrite, "client %d: %s\n", clients[fd].id, clients[fd].msg);
                            boradcast(fd);

							// clear buffer
                            bzero(clients[fd].msg, strlen(clients[fd].msg));
                            j = -1;
                        }
                    }
                }
            }
        }
    }
	return 0;
}