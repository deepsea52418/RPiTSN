#include "utils.h"

struct period_info
{
    struct timespec next_period;
    long period_ns;
};

static void inc_period(struct period_info *pinfo)
{
    pinfo->next_period.tv_nsec += pinfo->period_ns;

    while (pinfo->next_period.tv_nsec > ONESEC)
    {
        pinfo->next_period.tv_sec++;
        pinfo->next_period.tv_nsec -= ONESEC;
    }
}

static void periodic_task_init(struct period_info *pinfo, const long period)
{
    pinfo->period_ns = period;
    clock_gettime(CLOCK_TAI, &(pinfo->next_period));
    // pinfo->next_period.tv_sec += 1;
    // pinfo->next_period.tv_nsec = 1e9 - TIME_DELTA;
}

static void wait_rest_of_period(struct period_info *pinfo)
{
    inc_period(pinfo);
    clock_nanosleep(CLOCK_TAI, TIMER_ABSTIME, &pinfo->next_period, NULL);
}

// static int set_realtime(pthread_t thread, int priority, int cpu)
// {
//     cpu_set_t cpuset;
//     struct sched_param sp;
//     int err, policy;

//     int min = sched_get_priority_min(SCHED_FIFO);
//     int max = sched_get_priority_max(SCHED_FIFO);

//     fprintf(stderr, "min %d max %d\n", min, max);

//     if (priority < 0)
//     {
//         return 0;
//     }

//     err = pthread_getschedparam(thread, &policy, &sp);
//     if (err)
//     {
//         fprintf(stderr, "pthread_getschedparam: %s\n", strerror(err));
//         return -1;
//     }

//     sp.sched_priority = priority;

//     err = pthread_setschedparam(thread, SCHED_FIFO, &sp);
//     if (err)
//     {
//         fprintf(stderr, "pthread_setschedparam: %s\n", strerror(err));
//         return -1;
//     }

//     if (cpu < 0)
//     {
//         return 0;
//     }
//     CPU_ZERO(&cpuset);
//     CPU_SET(cpu, &cpuset);
//     err = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
//     if (err)
//     {
//         fprintf(stderr, "pthread_setaffinity_np: %s\n", strerror(err));
//         return -1;
//     }

//     return 0;
// }

int main(int argc, char *argv[])
{

    // set_realtime(pthread_self(), DEFAULT_PRIORITY, -1);

    // setup Conn
    const char *address = "192.168.0.12";
    const int port = 54321;
    const int fd_out = socket(AF_INET, SOCK_DGRAM, 0);
    uint64_t next_time;
    if (HW_FLAG)
    {
        setup_adapter(fd_out, "enp5s0");
    }

    setup_sender_sotime(fd_out, "enp5s0");
    setup_sender(fd_out);

    // start cyclic task
    struct period_info pinfo;
    periodic_task_init(&pinfo, PERIOD);
    int count = 0;
    while (1)
    {
        printf("[ ---- Iter-%5d ----------------------------- ]\n", count++);

        next_time = pinfo.next_period.tv_sec * ONESEC + pinfo.next_period.tv_nsec;
        // // rand_sec = 0;
        // rand_sec = ((float)(rand() % 20) / 100) * ONESEC;
        printf("SW-SCHE    TIMESTAMP %ld.%09ld\n", (next_time + TIME_DELTA) / ONESEC, (next_time + TIME_DELTA) % ONESEC - QDISC_DELTA);
        // sche_single(fd_out, address, port, next_time + TIME_DELTA - 1e5);
        // sche_single(fd_out, address, port, next_time + TIME_DELTA - 5e5);

        // sche_single(fd_out, address, port, next_time + TIME_DELTA - 5e5);
        // sche_single(fd_out, address, port, next_time + TIME_DELTA - 1e5);
        sche_single(fd_out, address, port, next_time + TIME_DELTA);
        // send_single(fd_out, address, port);
        wait_rest_of_period(&pinfo);
    }
}