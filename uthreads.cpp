//
// Created by daniel.hazan on 4/26/18.
//

#include "uthreads.hpp"
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <queue>
#include <cmath>
#include <stdlib.h>
#include <iostream>
#include <map>
#include "thread.hpp"
#include <sys/time.h>
#include <algorithm>

#include <vector>



//this priority queue contains all the threads' id from smallest
//to largest, and thus provides an efficient way to manage the scheduler properly
//we overload first the compare function of the queue-->
struct Thread_Compare{
    bool operator()(const Thread& a , const Thread& b) const{
        return a.getId()<b.getId();
    }
};
template <typename T>
class MyPriorityQueue : public std::priority_queue<T, std::vector<T>>
{
public:

    bool remove(const T& ThreadToRemove)
    {


        typename std::vector<T>::const_iterator ThreadToMoveIter = std::find(this->c.begin(),this->c.end(),ThreadToRemove);
        if(ThreadToMoveIter!= this->c.end())
        {

            this->c.erase(ThreadToMoveIter);
            return  true;
        }
        return false;


    }

};

int countQuantums;
struct sigaction action;
sigset_t signalSet;


std::map<int, Thread*> ThreadSet;
Thread* currentRunnigThread;
std::map<int,Thread*> BlockedThread;

//std::priority_queue<Thread*, std::vector<Thread*>,Thread_Compare> ReadyThreadQueue;
std::priority_queue<int, std::vector<int>,std::greater<int>> ReadyThreadIdQueue;

MyPriorityQueue<Thread*> ReadyThreadQueue2;


struct itimerval ThreadTimer;

/**this method allows the Block signals in signalSet to block and
"mask" the thread from running*/
void blockThread()
{
    if(sigprocmask(SIG_BLOCK,&signalSet,NULL)== -1)
    {
        std::cerr<< "masking error"<<std::endl;
        exit(1);
    }
}
/**this method allows the Un-Block signals in signalSet to resume and
"unmask" the thread to start running*/
void resumThread()
{
    if(sigprocmask(SIG_UNBLOCK,&signalSet,NULL)== -1)
    {
        std::cerr<< "un-masking error"<<std::endl;
        exit(1);
    }
}

/**this method switches between the currently running thread and
pushes it to a new state
 */
void swichRunningThread(Thread_State new_state)
{
    countQuantums++;
    if(ReadyThreadQueue2.empty())
    {
        //if there are no threads waiting in ready state(the main thread is the only thread)
        //check if its time expires
        currentRunnigThread->increaswQuantum();
    } else{

        Thread* nextThread = ReadyThreadQueue2.top();
        ReadyThreadQueue2.pop();
        int returnvalue = 0;//save the return value of sigsetjmp for later jump to siglongjmp
        switch(new_state)
        {
            case READY:
                //push the current Thread to ReadyQueue-->
                ReadyThreadQueue2.push(currentRunnigThread);
                currentRunnigThread->setThreadState(READY);
                returnvalue = sigsetjmp(*(currentRunnigThread->getThreadState()),1);

                break;
            case BLOCKED:
                BlockedThread[currentRunnigThread->getId()] = currentRunnigThread;
                currentRunnigThread->setThreadState(BLOCKED);
                returnvalue = sigsetjmp(*(currentRunnigThread->getThreadState()),1);
                break;
            case TERMINATED:
                /*todo*/
                ThreadSet.erase(currentRunnigThread->getId());
                ReadyThreadIdQueue.push(currentRunnigThread->getId());
                delete currentRunnigThread;
                returnvalue = 0;
                break;
            default:
                break;
        }

        //if returnvalue from sigsetjmp is 0 -->meaning it didnt come from other jump, but it is direct jump
        if(returnvalue==0)
        {
            currentRunnigThread = nextThread;
            nextThread->setThreadState((RUNNING));//set the next thread in ReadyQueue to running
            currentRunnigThread->increaswQuantum();
            siglongjmp(*(currentRunnigThread->getThreadState()),1);

        }

    }
    //in every switch between threads updating the ThreadTimer for
    // the new running Thread (see demo_itimer.c)??/*todo*/
    if(setitimer(ITIMER_VIRTUAL,&ThreadTimer,NULL)==-1)
    {
        std::cerr<<"setitimer Error"<<std::endl;
        exit(1);
    }
}
/**this function converting and initializing the Thread Timer usinig its
 * quantum clock, connecting it to the regular clock (seconds,micro-secs)*/
void restartTimer(int quantum_usec)
{
    /*
    ThreadTimer.it_value.tv_sec = 0;
    ThreadTimer.it_value.tv_usec = 0;
    ThreadTimer.it_interval.tv_sec =(__time_t )floor(quantum_usec/1000000);//converting to seconds
    ThreadTimer.it_interval.tv_usec =quantum_usec;
     */
    /*
    ThreadTimer.it_value.tv_sec = (__time_t )floor(quantum_usec/1000000);//converting to seconds
    ThreadTimer.it_value.tv_usec = quantum_usec - (__time_t )floor(quantum_usec/1000000)*1000000;
    ThreadTimer.it_interval.tv_sec =(__time_t )floor(quantum_usec/1000000);//converting to seconds;
    ThreadTimer.it_interval.tv_usec =quantum_usec - (__time_t )floor(quantum_usec/1000000)*1000000;
    */

    ThreadTimer.it_value.tv_sec = quantum_usec / 1000000;//converting to seconds
    ThreadTimer.it_value.tv_usec = 0;
    ThreadTimer.it_interval.tv_sec = quantum_usec / 1000000;//converting to seconds;
    ThreadTimer.it_interval.tv_usec = quantum_usec % 1000000;

    /*todo*/
}

void timerHandler(int signal)
{
    //when the time for the running thread expires- i.e- sigAlarm is sent--> switch between states!
    swichRunningThread(READY);
}

int uthread_init(int quantum_usecs)
{
    //start running the pid=0 main thread
    if(quantum_usecs<=0)
    {
        std::cerr<<"error in init timer"<<std::endl;
    }
    for(int i=1;i <= MAX_THREAD_NUM;i++)
    {
        ReadyThreadIdQueue.push(i);
    }
    Thread* mainThread = new Thread(0);
    ThreadSet[0] = mainThread;

    action.sa_handler =timerHandler;

    if(sigemptyset(&action.sa_mask)==-1)
    {
        std::cerr<<"error creating mask-set"<<std::endl;
        exit(-1);
    }
    action.sa_flags = 0;
    //when time expires - handle by switching
    if(sigaction(SIGVTALRM,&action,NULL)==-1)
    {
        std::cerr<<"Sigaction error"<<std::endl;
        exit(-1);
    }
    restartTimer(quantum_usecs);
    //set the main thread to running state

    currentRunnigThread = mainThread;
    mainThread->setThreadState(RUNNING);
    swichRunningThread(RUNNING);

    //creating signals set of all threads in library and adding SIGVTALARM
    if(sigemptyset(&signalSet)==-1)
    {
        std::cerr<<"error creating signal set"<<std::endl;
        exit(-1);
    }
    if(sigaddset(&signalSet,SIGVTALRM)==-1)
    {
        std::cerr<<"error adding SIGVTALARM to signalSet"<<std::endl;
        exit(-1);
    }

    return 0;//success

}

int uthread_spawn(void(*f)(void))
{
    blockThread();
    int AvailableThreadId = ReadyThreadIdQueue.top();
    if(AvailableThreadId<MAX_THREAD_NUM)
    {
        Thread* newThread = new Thread(AvailableThreadId,f);
        ReadyThreadIdQueue.pop();
        ReadyThreadQueue2.push(newThread);
        ThreadSet[AvailableThreadId] = newThread;
        resumThread();
        return  AvailableThreadId;


        //continue..../*todo*/
    }
    std::cerr<<"not have enough place for more threads"<<std::endl;
    resumThread();
    return -1;
}

int uthread_terminate(int tid)
{
    //first stopping the current thread from running
    blockThread();
    if(tid==0)
    {
        //terminating the main thread i.e exiting the entire process-->
        std::map<int,Thread*>::iterator iter;

        for(iter=ThreadSet.begin(); iter!= ThreadSet.end();iter++)
        {
            Thread_State allThreadState = iter->second->getThreadState2();
            Thread* ThreadToTerminte = ThreadSet[iter->second->getId()];
            switch (allThreadState)
            {
                case (READY):
                    ReadyThreadQueue2.remove(iter->second);
                    ThreadSet.erase(iter);
                    delete ThreadToTerminte;

                    break;
                case (RUNNING):
                    ThreadSet.erase(iter);
                    break;
                case(BLOCKED):
                    BlockedThread.erase(iter);
                    ThreadSet.erase(iter);
                    delete ThreadToTerminte;
                    break;

                default:
                    break;
            }

        }
        delete currentRunnigThread;
        currentRunnigThread = NULL;
        resumThread();
        exit(0);//success

    }
    if(ThreadSet.count(tid) == 0) // if the thread tid does not exist at all
    {
        std::cerr<<"termanation error"<<std::endl;
        resumThread();
        return -1;
    }
    //check if thread to br terminated is on running state-->
    if(ThreadSet[tid]->getThreadState2() ==RUNNING)
    {
        swichRunningThread(TERMINATED);
    }
    //on other cases remove the thread from relevant data structures and delete the Thread finally
    Thread* ThreadToTerminte = ThreadSet[tid];
    if(ThreadToTerminte->getThreadState2() == BLOCKED)
    {
        BlockedThread.erase(tid);
    }
    if(ThreadToTerminte->getThreadState2() == READY)
    {
        ReadyThreadQueue2.remove(ThreadToTerminte);
    }
    ThreadSet.erase(tid);
    ReadyThreadIdQueue.push(tid);
    delete ThreadToTerminte;
    resumThread();
    return 0;//success



}
int uthread_resume(int tid)
{
    blockThread();//prevent blocking the threads at the middle of function
    if(ThreadSet.count(tid) == 0)
    {
        std::cerr<<"resume error"<<std::endl;
        resumThread();
        return -1;
    }
    if(ThreadSet[tid]->getThreadState2() == READY || ThreadSet[tid]->getThreadState2()==RUNNING)
    {
        resumThread();
        return 0;//success
    }
    //if the thread is on block state - move it to READY queue
    BlockedThread.erase(tid);
    ReadyThreadQueue2.push(ThreadSet[tid]);
    ThreadSet[tid]->setThreadState(READY);
    resumThread();
    return 0;//success
}

int uthread_block(int tid)
{
    blockThread();
    if(tid ==0)
    {
        std::cerr<<"error blocking main thread"<<std::endl;
        resumThread();
        return -1;
    }
    if(ThreadSet.count(tid) == 0)
    {
        std::cerr<<"resume error"<<std::endl;
        resumThread();
        return -1;
    }
    if(ThreadSet[tid]->getThreadState2()==BLOCKED)
    {
        resumThread();
        return 0;//success
    }
    if(ThreadSet[tid]->getThreadState2()==RUNNING)
    {
        swichRunningThread(BLOCKED);
        resumThread();
        return 0;//success
    }
    if(ThreadSet[tid]->getThreadState2()==READY)
    {
        ReadyThreadQueue2.remove(ThreadSet[tid]);
        BlockedThread[tid] = ThreadSet[tid];
        ThreadSet[tid]->setThreadState(BLOCKED);
        resumThread();
        return 0;//success
    }
    resumThread();
    return -1;

}
int uthread_sync(int tid)
{
    blockThread();
    if(tid ==0)
    {
        std::cerr<<"error blocking main thread"<<std::endl;
        resumThread();
        return -1;
    }
    if(ThreadSet.count(tid) == 0 || currentRunnigThread->getId() == tid)
    {
        std::cerr<<"resume error"<<std::endl;
        resumThread();
        return -1;
    }
    Thread* originalThread = currentRunnigThread;
    swichRunningThread(BLOCKED);
    //moving the running  thread to READY state only **after** the thread tid is terminated!
    if(ThreadSet[tid]->getThreadState2() == TERMINATED)
    {
        uthread_resume(originalThread->getId());//moving the running thread to READY only if thread tid is terminated
    }
    resumThread();
    return 0;


}

int uthread_get_tid()
{
    return currentRunnigThread->getId();
}

int uthread_get_total_quantums()
{
    return countQuantums;
}

int uthread_get_quantums(int tid)
{
    blockThread();
    if(ThreadSet.count(tid) ==0)
    {
        std::cerr<<"dont have such  thread"<<std::endl;
        resumThread();
        exit(-1);
    }
    resumThread();
    return ThreadSet[tid]->getQuantums();

}
