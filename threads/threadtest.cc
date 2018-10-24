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

// testnum is set in main.cc
int testnum = 5;

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

/*My thread Test function*/
void
mySimpleThread(int which){
    int num;
    
    for (num = 0; num < 5; num++) {
        //printf("*** thread tid:%d uid:%d looped %d times\n",
        // which, currentThread->getUid(), num);
        //currentThread->Yield();
    }
}

void ThreadTest2(){
    DEBUG("t", "Entering test2");
    for(int i = 0; i < 125; i++){
        Thread *t = new Thread("forked thread");
        t->Fork(mySimpleThread, (void*)(t->getTid()));
    }
    currentThread->Yield();
    printf("tidPool: ");
    for(int i = 0; i < maxThreadNum; i++){
        printf("%d ", tidPool[i]);
    }
    scheduler->Print();
}
//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine to test TS.
//----------------------------------------------------------------------
void ThreadTest3(){
    DEBUG("t", "Entering test3");
    for(int i = 0; i < 5; i++){
        Thread *t = new Thread("forked thread");
        t->Fork(SimpleThread, (void*)(t->getTid()));
    }
    currentThread->TS();
    SimpleThread(0);
}

//----------------------------------------------------------------------
// ThreadTest
//  Invoke a test routine to test priority.
//----------------------------------------------------------------------
void testPr(Thread* t){
    for(int i = 0; i < 2; i++){
        t->Print();
        printf(" looped %d times\n", i);
    }
}

void ThreadTest4(){
    DEBUG("t", "Entering test4");
    for(int i = 0; i < 4; i++){
        Thread *t = new Thread("forked", i);
        t->Fork(testPr, (void*)t);
    }
    testPr(currentThread);
}

void testTimer(Thread *t){
    for(int i = 0; i < 10; i++){
        t->Print();
        printf(" looped %d times\n", i);
        t->threadTick();
        //printf("Total Ticks:%d\n", stats->totalTicks);
    }
}

void ThreadTest5(){
    DEBUG("t", "Entering test4");
    for(int i = 0; i < 3; i++){
        Thread *t = new Thread("forked");
        t->Fork(testTimer, (void*)t);
    }
    testTimer(currentThread);
}

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
    case 5:
            ThreadTest5();
            break;
    default:
	       printf("No test specified.\n");
	break;
    }
}


