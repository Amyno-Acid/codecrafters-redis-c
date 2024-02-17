#include "server.h"
#include "command.h"
#include "database.h"

int main(int argc, char** argv) {
	// Disable output buffering
	setbuf(stdout, NULL);

    struct server svr;
    svr.port = 6379;
    svr.backlog = 5;
    svr.role = master;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--port")) {
            svr.port = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--replicaof")) {
            svr.role = slave;
            svr.master_port = atoi(argv[i+=2]);
        }
    }

    printf("Options:\n  role:%d\n  port:%d\n  master:%d\n", svr.role, svr.port, svr.master_port);

    if (server_init(&svr) == -1) {
        printf("server_init failed\n");
    };
    
    while (poll(svr.pool.fds, svr.pool.nfds, -1)) {
        for (int i = 0; i < svr.pool.nfds; i++) {
            if (!(svr.pool.fds[i].revents & POLLIN)) 
                continue;
            if (i == SERVER_FD_IDX) {
                add_client(&svr.pool);
            } else {
                handle_cmd(svr.pool.fds[i].fd, &svr);
            }
        }
    }

    server_close(&svr);

	return 0;
}
