#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024

volatile sig_atomic_t wasSigHup = 0;
volatile sig_atomic_t running = 1;
int active_socket = -1;

void sigHupHandler(int r) {
    wasSigHup = 1;
}

void sigIntHandler(int sig) {
    running = 0;
}

int main() {
    signal(SIGINT, sigIntHandler);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) == -1) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    if (sigaction(SIGHUP, NULL, &sa) == -1) {
        perror("sigaction get failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction set failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) == -1) {
        perror("sigprocmask failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d. PID: %d\n", PORT, getpid());
    printf("Send SIGHUP with: kill -HUP %d\n", getpid());
    printf("Connect with: telnet localhost %d\n", PORT);

    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;
        
        if (active_socket != -1) {
            FD_SET(active_socket, &read_fds);
            if (active_socket > max_fd) {
                max_fd = active_socket;
            }
        }
        
        int ready = pselect(max_fd + 1, &read_fds, NULL, NULL, NULL, &origMask);
        
        if (ready == -1) {
            if (errno == EINTR) {
                if (wasSigHup) {
                    printf("Received SIGHUP signal\n");
                    wasSigHup = 0;
                }
                continue;
            }
            perror("pselect failed");
            break;
        }
        
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (new_socket == -1) {
                perror("accept failed");
                continue;
            }
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);
            
            printf("New connection from %s:%d\n", client_ip, client_port);
            
            if (active_socket != -1) {
                printf("Closing connection: only one active connection allowed\n");
                close(new_socket);
            } else {
                active_socket = new_socket;
                printf("Active connection established\n");
            }
        }
        
        if (active_socket != -1 && FD_ISSET(active_socket, &read_fds)) {
            char buffer[BUFFER_SIZE];
            int bytes_read = recv(active_socket, buffer, BUFFER_SIZE, 0);
            
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    printf("Connection closed by client\n");
                } else {
                    perror("recv failed");
                }
                close(active_socket);
                active_socket = -1;
            } else {
                printf("Received %d bytes of data\n", bytes_read);
            }
        }
        
        if (wasSigHup) {
            printf("Received SIGHUP signal\n");
            wasSigHup = 0;
        }
    }

    if (active_socket != -1) {
        close(active_socket);
    }
    close(server_fd);
    
    printf("Server stopped gracefully\n");
    return 0;
}
