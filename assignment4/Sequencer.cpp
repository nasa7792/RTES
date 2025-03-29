/*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizo/*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizor 3/16/2025
 */
#include <cstdint>
#include <cstdio>
#include "Sequencer.hpp"

void service2()
{
    puts("this is service 2 implemented as a function\n");
}

int main()
{
    // Example use of the sequencer/service classes:
    Sequencer sequencer{};//default initlization 
    sequencer.addService([]() {
            puts("this is service 1 implemented in a lambda expression\n");
        }, 1, 99, 5);

    sequencer.addService(service2, 1, 98, 10);

    sequencer.startServices();
    // todo: wait for ctrl-c or some other terminating condition
    sequencer.stopServices();
}r 3/16/2025
 */
#include <cstdint>
#include <cstdio>
#include "Sequencer.hpp"
 
void service2()
{
    std::puts("this is service 2 implemented as a function\n");
}

int main()
{
    // Example use of the sequencer/service classes:
    Sequencer sequencer{};
    sequencer.addService([]() {
            std::puts("this is service 1 implemented in a lambda expression\n");
        }, 1, 99, 5);

    sequencer.addService(service2, 1, 98, 10);

    sequencer.startServices();
    // todo: wait for ctrl-c or some other terminating condition
    sequencer.stopServices();
}