#include "utils.h"

int main(int argc, char *argv[])
{
    const int port = 54321;

    int fd_in = socket(AF_INET, SOCK_DGRAM, 0);

    int broadcast = 1;
    setsockopt(fd_in, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast);

    setup_adapter(fd_in, "eth0");
    setup_receiver(fd_in, port);

    // ------------ support boardcast

    int count = 0;
    while (1)
    {
        // printf("[ ---- Iter-%5d ----------------------------- ]\n", count++);
        recv_single(fd_in);
    }
}