#include "syscall.h"
int globalVar;
char fileName[20] = "testFile";
void func1(){
    globalVar ++;
    Exit(globalVar);
}

void func2(){
    int globalVar = 0;
    globalVar ++;
    Exit(globalVar);
}
int
main()
{
    int pid = Exec(fileName);
    Join(pid);
    globalVar = 10;
    Fork(func1);
    Yield(); 
    Fork(func2);
    Yield();
    Exit(0);
}

