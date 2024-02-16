#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>

int server_init(int port, int backlog) {
    int server_fd;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return -1;
    }

    // Prevent 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEPORT failed: %s \n", strerror(errno));
        return -1;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = { htonl(INADDR_ANY) },
    };

    if (bind(server_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return -1;
    }

    if (listen(server_fd, backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return -1;
    }

    return server_fd;
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);

    int server_fd = server_init(6379, 5);

    struct sockaddr_in client_addr;

    struct pollfd fds[256] = {{server_fd, POLLIN, 0}};
    struct sockaddr_in client_addrs[256];
    int client_addr_len = sizeof(struct sockaddr_in);
    int nfds = 1;
    int ready = 0;
    
    while (ready = poll(fds, nfds, -1)) {
        for (int i = 0; i < nfds; i++) {
            if (!(fds[i].revents & POLLIN)) 
                continue;
            if (i == 0) {
                if (nfds == 256) {
                    printf("Max clients reached\n");
                    continue;
                }
                int client_fd = accept(
                    fds[i].fd,
                    (struct sockaddr*) &client_addrs[nfds],
                    &client_addr_len
                );
                if (client_fd == -1) {
                    printf("Accept failed: %s \n", strerror(errno));
                    continue;
                }
                printf("Client #%d connected\n", nfds);
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                fds[nfds].revents = 0;
                ++nfds;
            } else {
                char pong_buff[] = "+PONG\r\n";
                char cmd_buff[256];
                if (read(fds[i].fd, cmd_buff, 255) == 0) {
                    printf("Client #%d exited\n", i);
                    close(fds[i].fd);
                    continue;
                }
                printf("Command received: %s \n", cmd_buff);
                write(fds[i].fd, pong_buff, sizeof(pong_buff)-1);
            }
        }
    }

	return 0;
}
