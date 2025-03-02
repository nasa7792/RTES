/*
 * File: edf_feasibility.c
 * Description: Tests edf feasibility of examples 5-9 using edf feasibility check
 * There are two test which are used one is utilization based and second is scheduling based test
 * Author: [Nalin Saxena]
 * Date: [2025-02-26]
 */
#include <math.h>
#include <stdio.h>
#include <limits.h>

#define TRUE 1
#define FALSE 0
#define U32_T unsigned int

// example 5
U32_T ex5_period[] = {2, 5, 10};
U32_T ex5_wcet[] = {1, 2, 1};

// example 6 ommit example 6 as per assignment instrucitons
/*
U32_T ex6_period[] = {2,5,7,13};
U32_T ex6_wcet[] = {1,1,1,2};
*/
// example 7
U32_T ex7_period[] = {3, 5, 15};
U32_T ex7_wcet[] = {1, 2, 4};

// example 8
U32_T ex8_period[] = {2, 5, 7, 13};
U32_T ex8_wcet[] = {1, 1, 1, 2};

// example 9
U32_T ex9_period[] = {6, 8, 12, 24};
U32_T ex9_wcet[] = {1, 2, 4, 6};

int edf_feasibility_test(U32_T numServices, U32_T period[], U32_T wcet[], U32_T deadline[]);

int main()
{
    int i;
    U32_T numServices;
    printf("******** EDF feasibility test running********\n");

    printf("Ex-5 U=%4.2f%%  (C1=1, C2=2, C3=1; T1=2, T2=5, T3=10; T=D): ",
           ((1.0 / 2.0) * 100.0 + (2.0 / 5.0) * 100.0 + (1.0 / 10.0) * 100.0));
    numServices = 3;
    if (edf_feasibility_test(numServices, ex5_period, ex5_wcet, ex5_period) == TRUE)
        printf("EDF Feasibility test passed ! \n");
    else
        printf("EDF Feasibility test failed ! \n");

    printf("\n \n");

    printf("Ex-7 U=%4.2f%%  (C1=1, C2=2, C3=4; T1=3, T2=5, T3=15; T=D): ",
           ((1.0 / 3.0) * 100.0 + (2.0 / 5.0) * 100.0 + (4.0 / 15.0) * 100.0));
    numServices = 3;
    if (edf_feasibility_test(numServices, ex7_period, ex7_wcet, ex7_period) == TRUE)
        printf("EDF Feasibility test passed ! \n");
    else
        printf("EDF Feasibility test failed ! \n");

    printf("\n \n");

    printf("Ex-8 U=%4.2f%%  (C1=1, C2=1, C3=1; C4=2 T1=2, T2=5, T3=7,T4=13; T=D): ",
           ((1.0 / 2.0) * 100.0 + (1.0 / 5.0) * 100.0 + (1.0 / 7.0) * 100.0 + (2.0 / 13.0) * 100.0));
    numServices = 4;
    if (edf_feasibility_test(numServices, ex8_period, ex8_wcet, ex8_period) == TRUE)
        printf("EDF Feasibility test passed ! \n");
    else
        printf("EDF Feasibility test failed ! \n");

    printf("\n \n");

    printf("Ex-9 U=%4.2f%%  (C1=1, C2=2, C3=4; C4=6 T1=6, T2=8, T3=12,T4=24; T=D): ",
           ((1.0 / 6.0) * 100.0 + (2.0 / 8.0) * 100.0 + (4.0 / 12.0) * 100.0 + (6.0 / 24.0) * 100.0));
    numServices = 4;
    if (edf_feasibility_test(numServices, ex9_period, ex9_wcet, ex9_period) == TRUE)
        printf("EDF Feasibility test passed ! \n");
    else
        printf("EDF Feasibility test failed ! \n");
}
// calculate gcd
int gcd(int a, int b)
{
    while (b != 0)
    {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

int lcm(int a, int b)
{
    return (a * b) / gcd(a, b);
    // common trick to calcualte lcm  https://www.geeksforgeeks.org/program-to-find-lcm-of-two-numbers/
}

//itertaively calculate lcm of all time periods
int calc_sim_period(U32_T numServices, U32_T period[])
{
    int final_period = period[0];
    for (int i = 1; i < numServices; i++)
    {
        final_period = lcm(period[i], final_period);
    }
    return final_period;
}

//compute utilization
float utilization_calc(U32_T numServices, U32_T period[], U32_T wcet[])
{
    float utilization = 0.0;
    for (int i = 0; i < numServices; i++)
    {
        utilization += (float)wcet[i] / period[i];
    }
    return utilization;
}

int edf_feasibility_test(U32_T numServices, U32_T period[], U32_T wcet[], U32_T deadline[])
{
    float utilization = utilization_calc(numServices, period, wcet);
    if (utilization > 1.0)
    {
        printf("schedule is not feasible \n");
        return FALSE;
    }
    int time = 0;
    int rem_wcets[numServices];
    int rem_deadline_times[numServices];
    int lcm_period = calc_sim_period(numServices, period);
    printf("\nsimulation period is %d \n", lcm_period);

    // initalize remaining time and capacity
//below code is not neccessary since utilization_calc test is sufficient to check edf feasibility
    for (int i = 0; i < numServices; i++)
    {
        rem_wcets[i] = wcet[i];
        rem_deadline_times[i] = period[i];
    }
    while (time < lcm_period)
    {
        int current_task_index = -1;
        int curr_earliest_deadline = INT_MAX;

        // find task with earliest deadline
        for (int i = 0; i < numServices; i++)
        {
            if (curr_earliest_deadline > rem_deadline_times[i] && rem_wcets[i] > 0)
            {
                curr_earliest_deadline = rem_deadline_times[i];
                current_task_index = i;
            }
        }
        if (current_task_index == -1)
        {
            time++;
            continue;
        }

        if (rem_wcets[current_task_index] > 0 && time > rem_deadline_times[current_task_index])
        {
            printf("Deadline missed! Task %d missed its deadline at time %d. Deadline was %d.\n",
                   current_task_index, time, rem_deadline_times[current_task_index]);
            return FALSE;
            // infeasible
        }
        //simulate task execution
        rem_wcets[current_task_index] -= 1;
        time++;

        if (rem_wcets[current_task_index] == 0)
        {
            // restore period and wcet
            rem_wcets[current_task_index] = wcet[current_task_index];
            rem_deadline_times[current_task_index] += period[current_task_index];
        }
    }
    return TRUE; //all done no deadlines missed return true
}
