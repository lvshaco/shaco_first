
#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

#pragma comment(lib,"ws2_32")

const char* USAGE = "commandc ip port command";

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf(USAGE);
        return 0;
    }

    const char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    WSADATA Ws;
    if (WSAStartup(MAKEWORD(2,2), &Ws) != 0) {
        printf("init window socket env error : %u\n", GetLastError());
        return -1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("create socket error : %u\n", GetLastError());
        return -1;
    }

    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = inet_addr(ip);
    remote_addr.sin_port = htons(port);
    memset(remote_addr.sin_zero, 0x00, 8);

    printf("connect to#%s:%u ... \n", ip, port);
    int res;
    res = connect(sock,(struct sockaddr*)&remote_addr, sizeof(remote_addr));
    if (res == SOCKET_ERROR) {
        printf("connect error : %u\n", GetLastError());
        return -1;
    }
    printf("connect success\n");

    char command[1024] = {0};
    sprintf(command, "%s\n", argv[3]);
    printf("send command#%s", command);
    res = send(sock, command, strlen(command), 0);
    if (res == SOCKET_ERROR) {
        printf("send command error : %s\n", GetLastError());
    }

    char recv_buffer[1024] = {0};
    while (true) {
        int size = recv(sock, recv_buffer, sizeof(recv_buffer)-1, 0);
        if (size == SOCKET_ERROR) {
            printf("recv result error : %u\n", GetLastError());
            break;
        } else if (size == 0) {
            printf("socket closed\n");
            break;
        }
        recv_buffer[size] = '\0';
        printf("%s", recv_buffer);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}

