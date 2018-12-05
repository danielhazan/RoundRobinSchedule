//
// Created by daniel.hazan on 4/27/18.
//

#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include "thread.hpp"
#include <stdint.h>

#define SECOND 1000000
#define STACK_SIZE 4096

#ifdef __x86_64__
typedef unsigned int address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
            "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
            "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#endif

//constructor
Thread::Thread(int id) :id(id),quantums(0),currentState(READY)
{
    stack = new char[STACK_SIZE];
}
Thread::Thread(int id,void(*f)(void)): id(id),quantums(0),currentState(READY) {

    stack = new char[STACK_SIZE];

    address_t sp, pc;

    sp = (intptr_t)stack + STACK_SIZE - sizeof(address_t);
    pc = (intptr_t)f;
    sigsetjmp(threadState, 1);
    (threadState->__jmpbuf)[JB_SP] = translate_address(sp);
    (threadState->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&threadState->__saved_mask);



}
Thread::~Thread() {
    delete stack;

}

int Thread::getId() const
{
    return id;
}

char* Thread::getThreadStack() const {

    return stack;
}
sigjmp_buf* Thread::getThreadState() {

    return &threadState;
}
int Thread::getQuantums() const {
    return quantums;
}
void Thread::increaswQuantum() {

    quantums++;
}

void Thread::setThreadState(Thread_State state) {
    currentState = state;
}
Thread_State Thread::getThreadState2() const{

    return currentState;
}
