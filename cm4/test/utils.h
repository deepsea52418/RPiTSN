#define _GNU_SOURCE /*for CPU_SET*/
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <linux/net_tstamp.h>
#include <sched.h>
#include <pthread.h>

#ifndef SO_TXTIME
#define SO_TXTIME 61
#define SCM_TXTIME SO_TXTIME
#endif

#ifndef SO_EE_ORIGIN_TXTIME
#define SO_EE_ORIGIN_TXTIME 6
#define SO_EE_CODE_TXTIME_INVALID_PARAM 1
#define SO_EE_CODE_TXTIME_MISSED 2
#endif

#ifndef CLOCK_TAI
#define CLOCK_TAI 11
#endif

#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
#endif

#define BUFFER_LEN 256
#define HW_FLAG 0

#define USE_TXTIME 1
#define DEFAULT_PRIORITY 3
#define TIME_DELTA 2 * ONESEC
#define PERIOD 10000000
#define ONESEC 1000000000ULL

#define QDISC_DELTA 150000

// #define DEADLINE_MODE SOF_TXTIME_DEADLINE_MODE
// #define RECEIVE_ERROR SOF_TXTIME_REPORT_ERRORS

#define DEADLINE_MODE 0
#define RECEIVE_ERROR 0

/* When deadline_mode is set, the qdisc will handle txtime
with a different semantics, changed from a 'strict'
transmission time to a deadline.  In practice, this means
during the dequeue flow etf(8) will set the txtime of the
packet being dequeued to 'now'.  The default is for this
option to be disabled. */

void die(const char *s);

int setup_receiver(int fd, const int port);
int setup_sender(int fd);
int setup_adapter(int fd, const char *dev_name);
void send_single(int fd, const char *address, const int port);
void recv_single(int fd);

// CHUANYU APR 19 MODIFICATION

int setup_sender_sotime(int fd, const char *dev_name);
void sche_single(int fd, const char *address, const int port, __u64 txtime);