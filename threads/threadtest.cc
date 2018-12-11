// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"
#include "synch.h"

// testnum is set in main.cc
int testnum = 4;

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	printf("*** thread %d looped %d times\n", which, num);
        currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, (void*)1);
    SimpleThread(0);
}

/*Test synchronization*/
#define slotVolume 5 
volatile int slot[slotVolume];
volatile int breadCnt = 0;
Condition slotCond("slotCond");
Lock condLck("condlck");

void consumer(int which){
    for (int i = 0; i < 5; ++i)
    {
        condLck.Acquire();
        slotCond.Wait(&condLck);
        breadCnt --;
        int breadVal = slot[breadCnt];
        printf("consumer:%d eat bread:%d\n", which, breadCnt);
        condLck.Release();
    }
}

void producer(int which){
    for (int i = 0; i < 5; ++i)
    {
        condLck.Acquire();
        slot[breadCnt] = 1;
        printf("producer:%d produce bread:%d\n", which, breadCnt ++);
        slotCond.Signal(&condLck);
        condLck.Release();
        currentThread->Yield();
    }
}

/*barrier*/
class Barrier{
    /*number of threads that will come to the barrier*/
public:
    Barrier(int num){
        tnum = num;
        selfLck = new Lock("barrier");
        selfCond = new Condition("barrier");
    }
    ~Barrier(){
        delete selfLck;
    }
    void setNum(int num){
        selfLck->Acquire();
        tnum = num;
        selfLck->Release();
    }
    void stuck(){
        selfLck->Acquire();
        curNum ++;
        if(curNum >= tnum)
            selfCond->Broadcast(selfLck);
        else
            selfCond->Wait(selfLck);
        selfLck->Release();
    }
private:
     int tnum;
     int curNum;
     Lock *selfLck;
     Condition *selfCond;
};

Barrier bar(3);

void barrierTest(int which){
    printf("thread:%d enter barrier\n", which);
    bar.stuck();
    printf("thread:%d continued after barrier\n", which);
}

void ThreadTest3(){
    DEBUG('t', "Entering ThreadTest3");

    for(int i = 0; i < 3; i++){
        Thread *t = new Thread("forked thread");
        t->Fork(barrierTest, (void *)i);
    }
}

/*read write lock problem*/

class rwLock{
public:
    rwLock(){
        rdLck = new Lock("rdl");
        wtLck = new Lock("wtl");
    }
    ~rwLock(){
        delete rdLck;
        delete wtLck;
    }
    void readerIn(){
        rdLck->Acquire();
        rdNum ++;
        if(rdNum == 1)
            wtLck->Acquire();
        rdLck->Release();

    }
    void readerOut(){
        rdLck->Acquire();
        rdNum --;
        if(rdNum == 0)
            wtLck->Release();
        rdLck->Release();
    }
    void writerIn(){
        wtLck->Acquire();
    }
    void writerOut(){
        wtLck->Release();
    }
    int getRdNum(){
        return rdNum;
    }

private:
    int rdNum;
    Lock *rdLck;
    Lock *wtLck;

};

rwLock rwl;
volatile int val = 0;
void writerFun(int which){
    rwl.writerIn();
    val = which;
    printf("writer:%d writes value:%d\n", which, which);
    rwl.writerOut();
}

void readerFun(int which){
    for(int i = 0; i < 3; i++){
        if(i != 0)
            currentThread->Yield();
        rwl.readerIn();
        printf("reader:%d reads value:%d with readerNum:%d\n", which, val, rwl.getRdNum());
        currentThread->Yield();
        rwl.readerOut();
        currentThread->Yield();
    }
}

void ThreadTest4(){
     DEBUG('t', "Entering ThreadTest4");

    for(int i = 0; i < 3; i++){
        Thread *t = new Thread("rd thread");
        t->Fork(readerFun, (void *)i);
    }

    Thread *t = new Thread("wt thread");
    t->Fork(writerFun, (void *)2);


}

void ThreadTest2(){
    DEBUG('t', "Entering ThreadTest2");

    for(int i = 0; i < 3; i++){
        Thread *t = new Thread("consumer thread");
        t->Fork(consumer, (void *)i);
    }
    for(int i = 0; i < 3; i++){
        Thread *t = new Thread("producer thread");
        t->Fork(producer, (void *)i);
    }
}

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
	   ThreadTest1();
	   break;
    case 2:
        ThreadTest2();
        break;
    case 3:
        ThreadTest3();
        break;
    case 4:
        ThreadTest4();
        break;
    default:
	printf("No test specified.\n");
	break;
    }
}

