// Sam Siewert, July 2016
//
// Check to ensure all your CPU cores on in an online state.
//
// Check /sys/devices/system/cpu or do lscpu.
//
// Tegra is normally configured to hot-plug CPU cores, so to make all available,
// as root do:
//
// echo 0 > /sys/devices/system/cpu/cpuquiet/tegra_cpuquiet/enable
// echo 1 > /sys/devices/system/cpu/cpu1/online
// echo 1 > /sys/devices/system/cpu/cpu2/online
// echo 1 > /sys/devices/system/cpu/cpu3/online

#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>

#define NUM_THREADS (4)
#define NUM_CPUS (1)

#define NSEC_PER_SEC (1000000000)
#define NSEC_PER_MSEC (1000000)
#define NSEC_PER_MICROSEC (1000)
#define DELAY_TICKS (1)
#define ERROR (-1)
#define OK (0)

unsigned int idx = 0, jdx = 1;
unsigned int seqIterations = 47;
unsigned int reqIterations = 10000000;
volatile unsigned int fib = 0, fib0 = 0, fib1 = 1;

int numberOfProcessors;

typedef struct
{
    int threadIdx;
} threadParams_t;

// POSIX thread declarations and scheduling attributes
//
pthread_t threads[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];
pthread_attr_t rt_sched_attr[NUM_THREADS];
int rt_max_prio, rt_min_prio;
struct sched_param rt_param[NUM_THREADS];
struct sched_param main_param;
pthread_attr_t main_attr;
pid_t mainpid;

__thread int thread_local_var = 0; // thread local_Variable
int global_unsafe=0;

void *increment_thread(void *threadp)
{
    int cpucore;
    int dummy = 0;
    cpu_set_t cpuset;
    pthread_t thread;
    thread = pthread_self();
    threadParams_t *threadParams = (threadParams_t *)threadp;
    cpucore = sched_getcpu();

    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    printf("\nThread idx=%d",
           threadParams->threadIdx);
    printf("Thread idx=%d ran on core=%d, affinity contained:", threadParams->threadIdx, cpucore);
    for (int i = 0; i < numberOfProcessors; i++)
        if (CPU_ISSET(i, &cpuset))
            printf(" CPU-%d ", i);
    printf("\n");
    for (int i = 0; i < 10000; i++)
    {
        thread_local_var++;
        global_unsafe++;
        usleep(rand() % 1000);
    }
    printf("Thread finished, thread_local_var = %d\n", thread_local_var);

    int *result_ret = (int *)malloc(sizeof(int));
    *result_ret = thread_local_var;
    return result_ret;
}
void print_scheduler(void)
{
    int schedType;

    schedType = sched_getscheduler(getpid());

    switch (schedType)
    {
    case SCHED_FIFO:
        printf("Pthread Policy is SCHED_FIFO\n");
        break;
    case SCHED_OTHER:
        printf("Pthread Policy is SCHED_OTHER\n");
        break;
    case SCHED_RR:
        printf("Pthread Policy is SCHED_OTHER\n");
        break;
    default:
        printf("Pthread Policy is UNKNOWN\n");
    }
}

int main(int argc, char *argv[])
{
    int rc;
    int i, scope, idx;
    cpu_set_t allcpuset;
    cpu_set_t threadcpu;
    int coreid;

    printf("This system has %d processors configured and %d processors available.\n", get_nprocs_conf(), get_nprocs());

    numberOfProcessors = get_nprocs_conf();
    printf("number of CPU cores=%d\n", numberOfProcessors);

    CPU_ZERO(&allcpuset);

    for (i = 0; i < numberOfProcessors; i++)
        CPU_SET(i, &allcpuset);

    if (numberOfProcessors >= NUM_CPUS)
        printf("Using sysconf number of CPUS=%d, count in set=%d\n", numberOfProcessors, CPU_COUNT(&allcpuset));
    else
    {
        numberOfProcessors = NUM_CPUS;
        printf("Using DEFAULT number of CPUS=%d\n", numberOfProcessors);
    }

    mainpid = getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    print_scheduler();
    rc = sched_getparam(mainpid, &main_param);
    main_param.sched_priority = rt_max_prio;
    rc = sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    if (rc < 0)
        perror("main_param");
    print_scheduler();

    pthread_attr_getscope(&main_attr, &scope);

    if (scope == PTHREAD_SCOPE_SYSTEM)
        printf("PTHREAD SCOPE SYSTEM\n");
    else if (scope == PTHREAD_SCOPE_PROCESS)
        printf("PTHREAD SCOPE PROCESS\n");
    else
        printf("PTHREAD SCOPE UNKNOWN\n");

    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);

    for (i = 0; i < NUM_THREADS; i++)
    {
        CPU_ZERO(&threadcpu);
        coreid = i % numberOfProcessors;
        printf("Setting thread %d to core %d\n", i, coreid);
        CPU_SET(coreid, &threadcpu);
        for (idx = 0; idx < numberOfProcessors; idx++)
            if (CPU_ISSET(idx, &threadcpu))
                printf(" CPU-%d ", idx);
        printf("\nLaunching thread %d\n", i);

        rc = pthread_attr_init(&rt_sched_attr[i]);
        rc = pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
        rc = pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
        rc = pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);

        rt_param[i].sched_priority = rt_max_prio - i - 1;
        pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

        threadParams[i].threadIdx = i;

        pthread_create(&threads[i],               // pointer to thread descriptor
                       &rt_sched_attr[i],         // use default attributes
                       increment_thread,          // thread function entry point
                       (void *)&(threadParams[i]) // parameters to pass in
        );
    }
    int total_sum = 0;
    for (int i = 0; i < NUM_THREADS; i++)
    {
        int *result;
        pthread_join(threads[i],(void**)&result);
        total_sum+=*result;
        free(result);
    }

    printf("Total sum of all threads = %d global unsafe=%d\n", total_sum,global_unsafe);
    printf("\nTEST COMPLETE\n");
}
