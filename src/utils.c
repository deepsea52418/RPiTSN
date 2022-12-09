#include "utils.h"

void die(const char *s)
{
    // perror(s);
    // exit(1);
    printf("%s", s);
}

int setup_receiver(int fd, const int port)
{
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        die("bind()");
    }

    int timestamp_flags;
    if (HW_FLAG)
    {
        timestamp_flags = SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    }
    else
    {
        timestamp_flags = SOF_TIMESTAMPING_RX_SOFTWARE;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &timestamp_flags, sizeof(timestamp_flags)) == -1)
    {
        die("setsockopt() HW receiving");
    }

    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPNS, &enable, sizeof(enable)) < 0)
    {
        die("setsockopt() SW receiving");
    }

    return 0;
}

int setup_sender(int fd)
{

    int timestamp_flags;
    if (HW_FLAG)
    {
        timestamp_flags = SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    }
    else
    {
        timestamp_flags = SOF_TIMESTAMPING_TX_SOFTWARE;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &timestamp_flags, sizeof(timestamp_flags)) < 0)
    {
        die("setsockopt() HW sending");
    }

    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPNS, &enable, sizeof(enable)))
    {
        die("setsockopt() SW receiving");
    }
}

int setup_sender_sotime(int fd, const char *dev_name)
{
    int so_priority = DEFAULT_PRIORITY;
    if (setsockopt(fd, SOL_SOCKET, SO_PRIORITY, &so_priority, sizeof(so_priority)))
    {
        die("setsockopt() Priority");
    }

    // Dec5: Chuanyu Modification -> Remove so_bindtodevice, let linux decide to use which interface.
    // Doesn't work. Revert the modification.
    if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, dev_name, strlen(dev_name)))
    {
        die("setsockopt() Bind to dev");
    }
    // set up SO_TXTIME
    static struct sock_txtime sk_txtime;
    sk_txtime.clockid = CLOCK_TAI;
    sk_txtime.flags = (DEADLINE_MODE | RECEIVE_ERROR);
    if (USE_TXTIME && setsockopt(fd, SOL_SOCKET, SO_TXTIME, &sk_txtime, sizeof(sk_txtime)))
    {
        die("setsockopt() So_txtime");
    }
}

int setup_adapter(int fd, const char *dev_name)
{
    struct hwtstamp_config hwts_config;
    struct ifreq ifr;

    memset(&hwts_config, 0, sizeof(hwts_config));
    hwts_config.tx_type = HWTSTAMP_TX_ON;
    hwts_config.rx_filter = HWTSTAMP_FILTER_ALL;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", dev_name);
    ifr.ifr_data = (void *)&hwts_config;
    if (ioctl(fd, SIOCSHWTSTAMP, &ifr) == -1)
    {
        die("ioctl()");
    }

    // unknown operation from udp_tai.c
    struct ifreq ifreq;
    int err;

    memset(&ifreq, 0, sizeof(ifreq));
    strncpy(ifreq.ifr_name, dev_name, sizeof(ifreq.ifr_name) - 1);
    err = ioctl(fd, SIOCGIFINDEX, &ifreq);
    if (err < 0)
    {
        die("ioctl SIOCGIFINDEX failed");
    }
}

void send_single(int fd, const char *address, const int port)
{
    /*
    Send one message
     */
    struct sockaddr_in si_server;
    memset(&si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(port);
    if (inet_aton(address, &si_server.sin_addr) == 0)
    {
        die("inet_aton()");
    }

    char buffer[BUFFER_LEN];

    struct iovec iov = (struct iovec){.iov_base = buffer, .iov_len = BUFFER_LEN};
    struct msghdr msg = (struct msghdr){.msg_name = &si_server,
                                        .msg_namelen = sizeof si_server,
                                        .msg_iov = &iov,
                                        .msg_iovlen = 1};
    ssize_t send_len = sendmsg(fd, &msg, 0);
    if (send_len < 0)
    {
        printf("[!] Error sendmsg()");
    }

    // -------------- obtain the loopback packet

    char data[BUFFER_LEN], control[BUFFER_LEN];
    struct iovec entry;
    struct sockaddr_in from_addr;
    int res;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &entry;
    msg.msg_iovlen = 1;
    entry.iov_base = data;
    entry.iov_len = sizeof(data);
    msg.msg_name = (caddr_t)&from_addr;
    msg.msg_namelen = sizeof(from_addr);
    msg.msg_control = &control;
    msg.msg_controllen = sizeof(control);

    // wait until get the loopback
    while (recvmsg(fd, &msg, MSG_ERRQUEUE) < 0)
    {
    }

    // encode the returned packet
    struct cmsghdr *cmsg;

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
        if (cmsg->cmsg_level == SOL_SOCKET &&
            cmsg->cmsg_type == SCM_TIMESTAMPING)
        {
            struct timespec *ts =
                (struct timespec *)CMSG_DATA(cmsg);
            printf("HW-SEND    TIMESTAMP %ld.%09ld\n", ts[2].tv_sec, ts[2].tv_nsec);
        }

        if (cmsg->cmsg_level == SOL_SOCKET &&
            cmsg->cmsg_type == SO_TIMESTAMPNS)
        {
            struct timespec *ts =
                (struct timespec *)CMSG_DATA(cmsg);
            printf("SW-SEND    TIMESTAMP %ld.%09ld\n", ts->tv_sec, ts->tv_nsec);
        }
    }
}

void sche_single(int fd, const char *address, const int port, uint64_t txtime)
{
    struct sockaddr_in si_server;
    memset(&si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(port);
    if (inet_aton(address, &si_server.sin_addr) == 0)
    {
        die("inet_aton()");
    }

    char buffer[BUFFER_LEN];

    struct cmsghdr *cmsg;
    struct iovec iov = (struct iovec){.iov_base = buffer, .iov_len = BUFFER_LEN};
    struct msghdr msg = (struct msghdr){.msg_name = &si_server,
                                        .msg_namelen = sizeof si_server,
                                        .msg_iov = &iov,
                                        .msg_iovlen = 1};

    char control[CMSG_SPACE(sizeof(txtime))];
    // char control[BUFFER_LEN]; Replece this will crush down program

    if (USE_TXTIME)
    {
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_TXTIME;
        cmsg->cmsg_len = CMSG_LEN(sizeof(__u64));
        *((__u64 *)CMSG_DATA(cmsg)) = txtime; // __64u --> uint64_t CHUANYU MODIFIED
    }

    ssize_t send_len = sendmsg(fd, &msg, 0);
    if (send_len < 1)
    {
        printf("[!] Error sendmsg()");
    }

    char data[BUFFER_LEN], control_back[BUFFER_LEN];
    struct iovec entry;
    struct sockaddr_in from_addr;

    int res;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &entry;
    msg.msg_iovlen = 1;
    entry.iov_base = data;
    entry.iov_len = sizeof(data);
    // msg.msg_name = (caddr_t)&from_addr;
    // msg.msg_namelen = sizeof(from_addr);
    msg.msg_control = &control_back;
    msg.msg_controllen = sizeof(control_back);

    // wait until get the loopback
    int count = 0;
    while (recvmsg(fd, &msg, MSG_ERRQUEUE) < 0 && count++ < 100)
    {
    }

    // encode the returned packet
    // struct cmsghdr *cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
        if (cmsg->cmsg_level == SOL_SOCKET &&
            cmsg->cmsg_type == SCM_TIMESTAMPING)
        {
            struct timespec *ts =
                (struct timespec *)CMSG_DATA(cmsg);
            printf("HW-SEND    TIMESTAMP %ld.%09ld\n", ts[2].tv_sec, ts[2].tv_nsec);
        }

        if (cmsg->cmsg_level == SOL_SOCKET &&
            cmsg->cmsg_type == SO_TIMESTAMPNS)
        {
            struct timespec *ts =
                (struct timespec *)CMSG_DATA(cmsg);
            printf("SW-SEND    TIMESTAMP %ld.%09ld\n", ts->tv_sec, ts->tv_nsec);
        }
    }
}

void recv_single(int fd)
{
    char data[BUFFER_LEN], ctrl[BUFFER_LEN];
    struct msghdr msg;
    struct iovec iov;
    ssize_t len;
    struct cmsghdr *cmsg;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl;
    msg.msg_controllen = sizeof(ctrl);
    iov.iov_base = data;
    iov.iov_len = sizeof(data);

    if (recvmsg(fd, &msg, 0) < 0)
    {
        printf("[!] Error recvmsg()");
    }

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
        if (cmsg->cmsg_level == SOL_SOCKET &&
            cmsg->cmsg_type == SCM_TIMESTAMPING)
        {
            struct timespec *ts =
                (struct timespec *)CMSG_DATA(cmsg);
            printf("HW-RECV    TIMESTAMP %ld.%09ld\n", ts[2].tv_sec, ts[2].tv_nsec);
        }

        if (cmsg->cmsg_level == SOL_SOCKET &&
            cmsg->cmsg_type == SCM_TIMESTAMPNS)
        {

            struct timespec *ts =
                (struct timespec *)CMSG_DATA(cmsg);
            printf("SW-RECV    TIMESTAMP %ld.%09ld\n", ts->tv_sec, ts->tv_nsec);
        }
    }
}
