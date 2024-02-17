#include "server.h"

int server_init(struct server *this) {
    this->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return -1;
    }
    // Prevent 'Address already in use' errors
    this->reuse = 1;
    if (setsockopt(this->fd, SOL_SOCKET, SO_REUSEPORT, &this->reuse, sizeof(this->reuse)) < 0) {
        printf("SO_REUSEPORT failed: %s \n", strerror(errno));
        return -1;
    }
    client_pool_init(&this->pool, this->fd);
    db_init(&this->db, DB_SIZE);
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(this->port),
        .sin_addr = { htonl(INADDR_ANY) },
    };
    if (bind(this->fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return -1;
    }
    if (listen(this->fd, this->backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return -1;
    }
    return 0;
}

int server_close(struct server *this) {
    close(this->fd);
}

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
