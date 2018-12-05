//
// Created by daniel.hazan on 4/27/18.
//

#ifndef PROJ2_THREAD_HPP
#define PROJ2_THREAD_HPP

typedef enum Thread_State {READY, RUNNING, BLOCKED, TERMINATED} Thread_State;


#include <setjmp.h>

class Thread
{
public:
    Thread(int id);
    //this constructor creates thread with id and a starting point f in pc
    Thread(int id,void (*f)(void));


    ~Thread();

    int getId() const;

    char* getThreadStack() const;

    sigjmp_buf* getThreadState();

    void increaswQuantum();

    //get the number of quantums that the thread was active
    int getQuantums() const;

    void setThreadState(Thread_State state);

    Thread_State getThreadState2() const;

private:
    int id;
    char* stack;
    sigjmp_buf threadState;
    int quantums;
    Thread_State currentState;
};


#endif //PROJ2_THREAD_HPP
