
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct s_client{
	int id;
	char msg[100000];
} t_client;

t_client clients[2048];
fd_set writefds, readfds, activefds;
int max_fd = 0, next_id = 0;
char buff_write[120000], buff_read[120000];

void err(char *str){
	write(2, str, strlen(str));
	exit(1);
}

void broadcast(int sender_fd, char *str){
	for (int fd = 0; fd <= max_fd; fd++){
		if (FD_ISSET(fd, &writefds) && sender_fd != fd)
			send(fd, str, strlen(str), 0);
	}
}

int main(int ac, char **av){
	if (ac != 2)
		err("Wrong number of arguments\n");
	
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		err("Fatal error\n");
	max_fd = sockfd;

	FD_ZERO(&activefds);
	FD_SET(sockfd, &activefds);

	struct sockaddr_in servaddr;
	socklen_t addr_len = sizeof(servaddr);
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 

	if((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		err("Fatal error\n");
	
	if(listen(sockfd, 128) != 0)
		err("Fatal error \n");

	while(1){
		readfds = writefds = activefds;

		if(select(max_fd + 1, &readfds, &writefds, NULL, NULL) < 0)
			continue;
		
		for(int fd = 0; fd <= max_fd; fd++){
			if (!FD_ISSET(fd, &readfds))
				continue;
			
			if (fd == sockfd) {
				int connectfd = accept(sockfd, (struct sockaddr *)&servaddr, &addr_len);
				if (connectfd < 0)
					continue;
				if(connectfd > max_fd)
					max_fd = connectfd;
				clients[connectfd].id = next_id++;
				memset(clients[connectfd].msg, 0, sizeof(clients[connectfd].msg));
				FD_SET(connectfd, &activefds);
				sprintf(buff_write, "server: client %d just arrived\n", clients[connectfd].id);
				broadcast(connectfd, buff_write);
			} else {
				int n = recv(fd, buff_read, 100000, 0);
				if (n <= 0){
					sprintf(buff_write, "server: client %d just left\n", clients[fd].id);
					broadcast(fd, buff_write);
					FD_CLR(fd, &activefds);
					close(fd);
				} else {
					for (int i = 0, j = strlen(clients[fd].msg); i < n; i++, j++) {
					clients[fd].msg[j] = buff_read[i];
					if (clients[fd].msg[j] == '\n'){
						clients[fd].msg[j] = '\0';
						sprintf(buff_write, "client %d: %s\n", clients[fd].id, clients[fd].msg);
						broadcast(fd, buff_write);
						memset(clients[fd].msg, 0, sizeof(clients[fd].msg));
						j = -1;
						}
					}
				}
			}
		}
	}
	return 0;
}

