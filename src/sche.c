#include "utils.h"

struct period_info
{
    struct timespec next_period;
    long period_ns;
};

static void inc_period(struct period_info *pinfo)
{
    pinfo->next_period.tv_nsec += pinfo->period_ns;

    while (pinfo->next_period.tv_nsec > 1e9)
    {
        pinfo->next_period.tv_sec++;
        pinfo->next_period.tv_nsec -= 1e9;
    }
}

static void periodic_task_init(struct period_info *pinfo, const long period)
{
    pinfo->period_ns = period;
    clock_gettime(CLOCK_TAI, &(pinfo->next_period));
}

static void wait_rest_of_period(struct period_info *pinfo)
{
    inc_period(pinfo);
    clock_nanosleep(CLOCK_TAI, TIMER_ABSTIME, &pinfo->next_period, NULL);
}

int main(int argc, char *argv[])
{
    // setup Conn
    const char *address = "192.168.0.13";
    const int port = 54321;
    const int fd_out = socket(AF_INET, SOCK_DGRAM, 0);
    uint64_t next_time;
    if (HW_FLAG)
    {
        setup_adapter(fd_out, "eth0");
    }
    // Make
    setup_sender_sotime(fd_out, "eth0");

    setup_sender(fd_out);

    // start cyclic task
    struct period_info pinfo;
    periodic_task_init(&pinfo, 1e6);
    int count = 0;
    while (1)
    {
        printf("[ ---- Iter-%5d ----------------------------- ]\n", count++);
        next_time = pinfo.next_period.tv_sec * 1e9 + pinfo.next_period.tv_nsec;

        sche_single(fd_out, address, port, next_time + TIME_DELTA);
        // send_single(fd_out, address, port);
        wait_rest_of_period(&pinfo);
    }
}