/*
 * File: DM_feasibility.c
 * Description: Tests DM feasibility for example 6 ( as per section 3.6 of text book its a simple utilization
 * utilization would be evaulated as U=∑Ci/Di<=1 (where C is compuation and Di is deadline should be less than 1)
 * The book also suggest to perform scheduling point tests and completition test
 */
#include <math.h>
#include <stdio.h>
#include <limits.h>

#define TRUE 1
#define FALSE 0
#define U32_T unsigned int

U32_T ex6_period[] = {2, 5, 7, 13};
U32_T ex6_wcet[] = {1, 1, 1, 2};
U32_T ex6_deadline[] = {2, 3, 7, 15};

int DM_feasibility_test(U32_T numServices, U32_T period[], U32_T wcet[], U32_T deadline[]);
int completion_time_feasibility(U32_T numServices, U32_T period[], U32_T wcet[], U32_T deadline[]);
int scheduling_point_feasibility(U32_T numServices, U32_T period[], U32_T wcet[], U32_T deadline[]);

int main()
{
    int i;
    U32_T numServices;
    printf("******** DM feasibility test running********\n");

    printf("Ex-6 Scenario (C1=1, C2=1, C3=1 ,C4=2; D1=2, D2=3, D3=7 , D4=15; )");
    numServices = 4;
    if (DM_feasibility_test(numServices, ex6_period, ex6_wcet, ex6_deadline) == TRUE)
        printf("DM Feasibility test passed ! \n");
    else
        printf("DM Feasibility test failed ! \n");

    printf("********Completition  test running********\n");
    if (completion_time_feasibility(numServices, ex6_deadline, ex6_wcet, ex6_deadline) == TRUE)
        printf("CT test FEASIBLE\n");
    else
        printf("CT test INFEASIBLE\n");
    printf("********Scheduling point test running********\n");

    if (scheduling_point_feasibility(numServices, ex6_deadline, ex6_wcet, ex6_deadline) == TRUE)
        printf("FEASIBLE\n");
    else
        printf("INFEASIBLE\n");
}

float compute_interference(int i, U32_T period[], U32_T wcet[], U32_T deadline[])
{
    U32_T interference=0;
    for (U32_T j = 0; j < i; j++) //j<<i
    {
        interference += (U32_T)(ceil((float)deadline[i] / period[j])) * wcet[j];
    }
    return interference;
}

int DM_feasibility_test(U32_T numServices, U32_T period[], U32_T wcet[], U32_T deadline[])
{
    float utilization;
    for (U32_T i = 0; i < numServices; i++)
    {
        U32_T interference = compute_interference(i, period, wcet, deadline);
        utilization = (float)wcet[i] / deadline[i] + (float)interference / deadline[i];
    }
    if (utilization > 1.0)
    {
        printf("test failed task not schedulable \n");
        return FALSE;
    }
    return TRUE;
}

int completion_time_feasibility(U32_T numServices, U32_T period[], U32_T wcet[], U32_T deadline[])
{
    int i, j;
    U32_T an, anext;

    // assume feasible until we find otherwise
    int set_feasible = TRUE;

    // printf("numServices=%d\n", numServices);

    // For all services in the analysis
    for (i = 0; i < numServices; i++)
    {
        an = 0;
        anext = 0;

        for (j = 0; j <= i; j++)
        {
            an += wcet[j];
        }

        // printf("i=%d, an=%d\n", i, an);

        while (1)
        {
            anext = wcet[i];

            for (j = 0; j < i; j++)
                anext += ceil(((double)an) / ((double)period[j])) * wcet[j];

            if (anext == an)
                break;
            else
                an = anext;

            // printf("an=%d, anext=%d\n", an, anext);
        }

        // printf("an=%d, deadline[%d]=%d\n", an, i, deadline[i]);

        if (an > deadline[i])
        {
            set_feasible = FALSE;
        }
    }

    return set_feasible;
}

int scheduling_point_feasibility(U32_T numServices, U32_T period[],
                                 U32_T wcet[], U32_T deadline[])
{
    int rc = TRUE, i, j, k, l, status, temp;

    // For all services in the analysis
    for (i = 0; i < numServices; i++) // iterate from highest to lowest priority
    {
        status = 0;

        // Look for all available CPU minus what has been used by higher priority services
        for (k = 0; k <= i; k++)
        {
            // find available CPU windows and take them
            for (l = 1; l <= (floor((double)period[i] / (double)period[k])); l++)
            {
                temp = 0;

                for (j = 0; j <= i; j++)
                    temp += wcet[j] * ceil((double)l * (double)period[k] / (double)period[j]);

                // Can we get the CPU we need or not?
                if (temp <= (l * period[k]))
                {
                    // insufficient CPU during our period, therefore infeasible
                    status = 1;
                    break;
                }
            }
            if (status)
                break;
        }

        if (!status)
            rc = FALSE;
    }
    return rc;
}
