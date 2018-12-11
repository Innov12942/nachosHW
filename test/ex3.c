#include "syscall.h"

#define fname "testfile"

int
main(){
    char wBuf[20] = "deadbeaf";
    char rBuf[20];
    int bufSize= 8;
    Create(fname);
    int fd = Open(fname);
    Write(wBuf, bufSize, fd);
    Read(rBuf, bufSize, fd);
    Close(fd); 
}
