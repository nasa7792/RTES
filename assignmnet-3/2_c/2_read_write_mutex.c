/* 
authour- Nalin Saxena
Initlization code is Leveraged from Few examples provided by Dr Sam Siewert.

Code for Excercise 2c
Now, using a MUTEX, provide an example using RT-Linux Pthreads that does a thread 
safe update of a complex state (3 or more numbers � e.g., Latitude, Longitude and 
Altitude of a location) with a timestamp (pthread_mutex_lock).  Your code should 
include two threads and one should update a timespec structure contained in a structure 
that includes a double precision attitude state of {Lat, Long, Altitude and Roll, Pitch, 
Yaw at Sample_Time} and the other should read it and never disagree on the values as 
function of time. You can just make up values for the navigational state using math 
library function generators (e.g., use simple periodic functions for Roll, Pitch, Yaw 
sin(x), cos(x2), and cos(x), where x=time and linear functions for Lat, Long, Alt) and see 
http://linux.die.net/man/3/clock_gettime for how to get a precision timestamp).  The 
second thread should read the times-stamped state without the possibility of data 
corruption (partial update of one of the 6 floating point values).  There should be no 
disagreement between the functions and the state reader for any point in time. Run this 
for 180 seconds with a 1 Hz update rate and a 0.1 Hz read rate. Make sure the 18 values 
read are correct.
*/
#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include<stdlib.h>
#include <sched.h>
#include <time.h>
#include<math.h>

#define NUM_THREADS (2)
#define NUM_CPUS (1)


int numberOfProcessors;
volatile int terminate_test=0; // a flag to terminate the test

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

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //mutex
//a struct containig our dummy data and timespec
typedef struct{
    float lat;
    float longi;
    float alti;
    float pitch;
    float yaw;
    float roll;
    struct timespec sample_time;
    float checksum_val;
}NAV_DATA;

NAV_DATA nav_data;


void *data_generator(void *threadp)
{
    int cpucore;
    cpu_set_t cpuset;
    pthread_t thread;
    thread = pthread_self();
    threadParams_t *threadParams = (threadParams_t *)threadp;
    cpucore = sched_getcpu();
    srand(time(NULL));
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    printf("\nThread idx=%d",
           threadParams->threadIdx);
    printf("Thread idx=%d ran on core=%d, affinity contained:", threadParams->threadIdx, cpucore);
    for (int i = 0; i < numberOfProcessors; i++)
        if (CPU_ISSET(i, &cpuset))
            printf(" CPU-%d ", i);
    //operate till test is not terminated
    while(!terminate_test){
        //lock critical section
        pthread_mutex_lock(&mutex);
        double time=rand()%100;
        nav_data.lat=123*time;
        nav_data.longi=456*time;
        nav_data.alti=789*time;
        nav_data.roll=sin(time);
        nav_data.yaw=cos(2*time);
        nav_data.pitch=sin(2*time);
        clock_gettime(CLOCK_REALTIME,&nav_data.sample_time);
        nav_data.checksum_val=nav_data.lat+nav_data.longi+nav_data.alti+nav_data.roll+nav_data.yaw+nav_data.pitch; // a simple checksum
        pthread_mutex_unlock(&mutex); //unlock 
        sleep(1); //sleep for 1 seconds 1Hz
    }
    return NULL;
}

void *data_consumer(void *threadp)
{
    int cpucore;
    cpu_set_t cpuset;
    pthread_t thread;
    thread = pthread_self();
    threadParams_t *threadParams = (threadParams_t *)threadp;
    cpucore = sched_getcpu();

    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    printf("\nThread idx=%d",
           threadParams->threadIdx);
    printf("Thread idx=%d ran on core=%d, affinity contained:\n", threadParams->threadIdx, cpucore);
    for (int i = 0; i < numberOfProcessors; i++)
        if (CPU_ISSET(i, &cpuset))
            printf(" CPU-%d ", i);
    //needs to run for 18 iterations
    for(int i=0;i<18;i++){
        sleep(10);
        float local_checksum;
        pthread_mutex_lock(&mutex);
        NAV_DATA local_nav_data=nav_data;
        //validate checksum
        local_checksum=local_nav_data.lat+local_nav_data.longi+local_nav_data.alti+local_nav_data.yaw+local_nav_data.pitch+local_nav_data.roll;
        if(local_checksum==nav_data.checksum_val){
            printf("VALID CHECKSUM-Read Iteration %d: Latitiude=%.6f, Longitude=%.6f, Altitude=%.6f, Roll=%.6f, Pitch=%.6f, Yaw=%.6f, Time=%ld.%09ld,\n", 
                i + 1, local_nav_data.lat,local_nav_data.longi,local_nav_data.alti,local_nav_data.roll,local_nav_data.pitch,local_nav_data.yaw,local_nav_data.sample_time.tv_sec,local_nav_data.sample_time.tv_nsec);
        }
        pthread_mutex_unlock(&mutex);
    }
    terminate_test=1; //terminate test after 18 iterations
    return NULL;
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
    cpu_set_t cpuset;

    int coreid;
    CPU_ZERO(&cpuset);
    for (i = 0; i < NUM_CPUS; i++)
    CPU_SET(i, &cpuset);

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

        rc=pthread_attr_init(&rt_sched_attr[i]);
        //threads have explict scheduling seperate from processes
        rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
        rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
        rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &cpuset); //threads run on set cpu cors
        rt_param[i].sched_priority=rt_max_prio-i-1;
        pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);
        threadParams[i].threadIdx=i;
    }
    
   pthread_create(&threads[0], &rt_sched_attr[0], data_generator, (void *)&(threadParams[0])); //0 index gets the highest prioirty the data generator thread
   pthread_create(&threads[1], &rt_sched_attr[1], data_consumer, (void *)&(threadParams[1]));

   for(i=0; i<2; i++)
   pthread_join(threads[i], NULL); //terminate and join 

    printf("\nTEST COMPLETE\n");
}
