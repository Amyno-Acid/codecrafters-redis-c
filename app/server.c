#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>

#define SERVER_FD_IDX 0
#define MAX_CLIENT 256
#define MAX_CMD_LEN 256
#define MAX_RAW_CMD_LEN 256

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

struct client_pool {
    struct pollfd fds[MAX_CLIENT];
    struct sockaddr_in client_addrs[MAX_CLIENT];
    int nfds;
    socklen_t client_addr_len;
};
void client_pool_init(struct client_pool *this, int server_fd) {
    this->fds[SERVER_FD_IDX].fd = server_fd;
    this->fds[SERVER_FD_IDX].events = POLLIN;
    this->fds[SERVER_FD_IDX].revents = 0;
    this->nfds = 1;
    this->client_addr_len = sizeof(struct sockaddr_in);
}
int add_client(struct client_pool *this) {
    if (this->nfds == MAX_CLIENT) {
        printf("Max no. clients reached\n");
        return -1;
    }
    int client_fd = accept(
        this->fds[SERVER_FD_IDX].fd,
        (struct sockaddr*) &this->client_addrs[this->nfds],
        &this->client_addr_len
    );
    if (client_fd == -1) {
        printf("Accept failed: %s \n", strerror(errno));
        return -1;
    }
    printf("Client #%d connected\n", this->nfds);
    this->fds[this->nfds].fd = client_fd;
    this->fds[this->nfds].events = POLLIN;
    this->fds[this->nfds].revents = 0;
    return this->nfds++;
}
int split_raw_cmd(char* raw_cmd, int raw_cmd_len, char** cmd) {
    char buff[MAX_RAW_CMD_LEN];
    int cmd_len = 0, buff_len = 0;
    for (int i = 0; i < raw_cmd_len; i++) {
        if (raw_cmd[i] != '\r') {
            buff[buff_len++] = raw_cmd[i];
            continue;
        }
        buff[buff_len++] = '\0';
        cmd[cmd_len] = malloc(buff_len);
        strcpy(cmd[cmd_len], buff);
        if (++cmd_len == MAX_CMD_LEN)
            break;
        buff_len = 0;
        ++i;
    }
    return cmd_len;
}
int handle_cmd(int client_fd) {
    char raw_cmd[MAX_RAW_CMD_LEN];
    int raw_cmd_len;
    if ((raw_cmd_len = read(client_fd, raw_cmd, MAX_RAW_CMD_LEN-1)) == 0) {
        printf("Client exited\n");
        close(client_fd);
        return 1;
    }
    char* cmd[MAX_CMD_LEN];
    printf("Raw size: %d\n", raw_cmd_len);
    int cmd_len = split_raw_cmd(raw_cmd, raw_cmd_len, cmd);
    printf("Received: ");
    for (int i = 0; i < cmd_len; i++) {
        printf("%s ", cmd[i]);
    }
    printf("\n");
    for (int i = 0; cmd[2][i] != '\0'; i++)
        cmd[2][i] = toupper(cmd[2][i]);
    if (!strcmp(cmd[2], "PING")) {
        char pong_buff[] = "+PONG\r\n";
        write(client_fd, pong_buff, sizeof(pong_buff)-1);
    } else if (!strcmp(cmd[2], "ECHO")) {
        char echo_buff[256];
        int echo_buff_len = snprintf(echo_buff, 255, "$%d\r\n%s\r\n", strlen(cmd[4]), cmd[4]);
        write(client_fd, echo_buff, echo_buff_len);
    } else {
        char pong_buff[] = "+PONG\r\n";
        write(client_fd, pong_buff, sizeof(pong_buff)-1);
    }
    return 0;
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);

    int server_fd = server_init(6379, 5);

    struct client_pool pool;
    client_pool_init(&pool, server_fd);
    
    while (poll(pool.fds, pool.nfds, -1)) {
        for (int i = 0; i < pool.nfds; i++) {
            if (!(pool.fds[i].revents & POLLIN)) 
                continue;
            if (i == SERVER_FD_IDX) {
                add_client(&pool);
            } else {
                handle_cmd(pool.fds[i].fd);
            }
        }
    }

	return 0;
}
