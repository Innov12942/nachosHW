// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "filesys.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
#ifdef UseLRU
    #define tlbScheme tlbLRU
#else 
    #define tlbScheme tlbFIFO
#endif

#define LRUMaxVal 100
#define bufSize 30

int pagefaultNum = 0;

//with vm on disk
void Disk2Mem(int vpn){
    pagefaultNum ++;
    int dpn = machine->pageTable[vpn].DiskPage;
   // printf("num clear %d\n", machine->bmForPm->NumClear());
    int getOnePP = machine->bmForPm->Find();
    if(getOnePP == -1){
        getOnePP = currentThread->space->KickOnePageOut();
        int findOnePP = machine->bmForPm->Find();
        ASSERT(getOnePP == findOnePP)
        //printf("kick one physical page %d out of memory\n", getOnePP);
    }
    ASSERT(getOnePP != -1);

    OpenFile *vmFile = fileSystem->Open("virMem");
    vmFile->ReadAt(machine->mainMemory + getOnePP * PageSize, 
        PageSize, dpn * PageSize);
    machine->pageTable[vpn].valid = TRUE;
    machine->pageTable[vpn].physicalPage = getOnePP;
    machine->pageTable[vpn].dirty = FALSE;
//    printf("diskpage:%d to physicalPage:%d\n", dpn, getOnePP);
 //   int x;
    //scanf("%d", &x);
    delete vmFile;
}

void Vp2Tlb(int pos, int missVpn){
    if(machine->pageTable[missVpn].valid)
        machine->tlb[pos] = machine->pageTable[missVpn];
    else{
        Disk2Mem(missVpn);
    }
}

int LRUVal[TLBSize];
int tlbMiss = 0;
int tlbHit = 0;

void LRUHit(int which){
    for(int i = 0; i < TLBSize; i++)
        if(LRUVal[i] > 0)
            LRUVal[i] --;
    LRUVal[which] = LRUMaxVal;
}

int findLRUEntry(){
    int entryNO = 0;
    int minVal = LRUMaxVal;
    for(int i = 0; i < TLBSize; i++){
        if(LRUVal[i] < minVal){
            minVal = LRUVal[i];
            entryNO = i;
        }
    }
    return entryNO;
}

void HitTlb(int which){
    tlbHit ++;
#ifdef UseLRU
    LRUHit(which);
#endif
}


/*using FIFO rule to exchange entry*/
void tlbFIFO(int missVpn){
    TranslationEntry *lastEntry = &machine->tlb[TLBSize - 1];
    machine->pageTable[lastEntry->virtualPage] = *lastEntry;
    for(int i = TLBSize - 1; i > 0; i--)
        machine->tlb[i] = machine->tlb[i - 1];

    Vp2Tlb(0, missVpn);
}

void tlbLRU(int missVpn){
    int unluck = findLRUEntry();
    TranslationEntry *unluckyEntry = &machine->tlb[unluck];
    machine->pageTable[unluckyEntry->virtualPage] = *unluckyEntry;

    Vp2Tlb(unluck, missVpn);
    LRUHit(unluck);
}

void loadVpToTlb(int missVpn){
    tlbMiss ++;
    int pos = 0;
    bool anyEmpty = FALSE;
    //check if any empty entry in tlb
    for(int i = 0; i < TLBSize; i++){
        if(machine->tlb[i].valid == FALSE){
            pos = i;
            anyEmpty = true;
            break;
        }
    }

    if(anyEmpty)
        Vp2Tlb(pos, missVpn);
    else
        tlbScheme(missVpn);
}

void printTlbMissRatio(){
#ifdef UseLRU
    printf("UseLRU\n");
#else 
    printf("UseFIFO\n");
#endif
    int total = (tlbMiss + tlbHit);
    if(total <= 0)
        total = 1;
    printf("total Hit:%d total Miss:%d\n", tlbHit, tlbMiss);
    printf("tlbMissRatio:%.4f\n", (double)(tlbMiss) / (double)(total));
}

void startPrs(char *fname){
    char filename[20] = "testFile";
    OpenFile *executable = fileSystem->Open(filename);
    AddrSpace *space;

    if (executable == NULL) {
    printf("Unable to open file %s\n", filename);
    return;
    }
    space = new AddrSpace(executable);    
    currentThread->space = space;

    delete executable;          // close file

    space->InitRegisters();     // set the initial register values
    space->RestoreState();      // load page table register

    machine->Run();         // jump to the user progam
    ASSERT(FALSE);          // machine->Run never returns;
                    // the address space exits
                    // by doing the syscall "exit"
}

void startFork(int func){
    currentThread->space->SaveState();
    currentThread->space->RestoreState();
    machine->WriteRegister(PCReg, func);
    machine->WriteRegister(NextPCReg, func + 4);
    machine->Run();
}

#define MaxThreadNum 65536
extern BitMap tidMap;
extern Thread *tidPool[MaxThreadNum];
int exitStatus[MaxThreadNum];

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
        printf("Exception SC_Halt!\n");  
        DEBUG('a', "Shutdown, initiated by user program.\n");
        printTlbMissRatio();   
        interrupt->Halt();
    }
    else if((which == SyscallException) && (type == SC_Exit)){
        if(currentThread->getName()[0] == 'm')
            currentThread->Yield();
        printf("Exception SC_Exit!\n");   
        printTlbMissRatio();
        int curTid = currentThread->getTid();
        exitStatus[curTid] = machine->ReadRegister(4);
        printf("Thread:%d exit with code:%d\n", curTid, exitStatus[curTid]);
        currentThread->Finish();
    }
    else if((which == SyscallException) && (type == SC_Create)){
        printf("syscall Create\n");
        int nameAddr = machine->ReadRegister(4);
        char name[bufSize + 4];
        for(int i = 0; i < bufSize - 1; i++){
            while(!machine->ReadMem(nameAddr + i, 1, (int *)(name + i)));
            if(name[i] == '\0')
                break;
        }
        fileSystem->Create(name, 0);
        machine->advancePC();
    }
    else if((which == SyscallException) && (type == SC_Open)){
        printf("syscall Open\n");
        int nameAddr = machine->ReadRegister(4);
        char name[bufSize + 4];
        for(int i = 0; i < bufSize - 1; i++){
            while(!machine->ReadMem(nameAddr + i, 1, (int *)(name + i)));
            if(name[i] == '\0')
                break;
        }
        int fd = currentThread->open(name);
        if(fd == -1)
            printf("cannot of file:%s\n", name);
        machine->WriteRegister(2, fd);
        machine->advancePC();
    }
    else if((which == SyscallException) && (type == SC_Close)){
        printf("syscall Close\n" );
        int fd = machine->ReadRegister(4);
        currentThread->close(fd);
        machine->advancePC();
    }
    else if((which == SyscallException) && (type == SC_Read)){
        printf("syscall Read\n");
        int bufPosition = machine->ReadRegister(4);
        int length = machine->ReadRegister(5);
        int fd = machine->ReadRegister(6);
        OpenFile *fp = currentThread->ofTable[fd];
        char buf[length + 4];
        int readNum = fp->Read(buf, length);
/*        if(readNum != length)
            printf("syscall Read not enough long!\n");*/
        for(int i = 0; i < readNum; i++)
            while(!machine->WriteMem(bufPosition + i, 1, int(buf[i])));
        machine->WriteRegister(2, readNum);
        machine->advancePC();
    }
    else if((which == SyscallException) && (type == SC_Write)){
        printf("syscall Write\n");
        int bufPosition = machine->ReadRegister(4);
        int length = machine->ReadRegister(5);
        int fd = machine->ReadRegister(6);
        OpenFile *fp = currentThread->ofTable[fd];
        char buf[length + 4];
        for(int i = 0; i < length; i++)
            while(!machine->ReadMem(bufPosition + i, 1, (int *)(buf + i) ));

        fp->Write(buf, length);
        machine->advancePC();
    }
    else if((which == SyscallException) && (type == SC_Exec)){
        printf("syscall execve\n");
        int nameAddr = machine->ReadRegister(4);
        char name[bufSize + 4];
        for(int i = 0; i < bufSize; i++)
            name[i] = '\0';
        for(int i = 0; i < bufSize - 1; i++){
            while(!machine->ReadMem(nameAddr + i, 1, (int *)(name + i)));
            if(name[i] == '\0')
                break;
        }
        printf("execve %s\n", name);
        Thread *t = new Thread("execve thread");
        t->Fork(startPrs, (void *)name);
        
        machine->WriteRegister(2, t->getTid());
        machine->advancePC();
    }
    else if((which == SyscallException) && (type == SC_Fork)){
        char *c = new char[2018];
        printf("syscall Fork\n");
        int funAddr = machine->ReadRegister(4);
        Thread *t = new Thread("syscallfork");
        t->Fork(startFork, (void *)funAddr);
        t->space = currentThread->space;
        machine->advancePC();

    }
    else if((which == SyscallException) && (type == SC_Yield)){
        printf("syscall Yield\n");
        currentThread->Yield();
        machine->advancePC();
    }
    else if((which == SyscallException) && (type == SC_Join)){
        printf("syscall Join\n");
        int tid = machine->ReadRegister(4);
        while(tidPool[tid] != NULL)
            currentThread->Yield();
        machine->WriteRegister(2, exitStatus[tid]);
        machine->advancePC();
    }
    else if(which == PageFaultException) {
        if(machine->tlb != NULL){
                int faultVA = machine->ReadRegister(BadVAddrReg);
                int vpn = faultVA / PageSize;
                loadVpToTlb(vpn);
        }
        else{
                printf("please define USE_TLB\n");
                printf("PageFaultException in ExceptionHandler! Program Halt!\n");
                ASSERT(FALSE)
        }
    }
    else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}

