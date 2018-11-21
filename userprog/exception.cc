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

//with vm on disk
int kickOut(){
    int pageNum = currentThread->space->KickOnePageOut();
    ASSERT(pageNum != -1);
    return pageNum;
}

void Disk2Mem(int vpn){
    int dpn = pageTable[vpn].DiskPage;
    int getOnePP = machine->bmForpm->Find();
    if(getOnePP == -1){
        getOnePP = kickOut();
    }

    OpenFile *vmFile = fileSystem->Open("virMem");
    vmFile->ReadAt(machine->mainMemory + getOnePP * PageSize, 
        PageSize, dpn * PageSize);
    pageTable[vpn].valid = TRUE;
    pageTable[vpn].physicalPage = getOnePP;
    pageTable[vpn].dirty = FALSE;
    delete vmFile;
}

void Vp2Tlb(int pos, int missVpn){
    if(pageTable[missVpn].valid)
        machine->tlb[pos] = machine->pageTable[missVpn];
    else{
        Disk2Mem(missVpn);
        machine->tlb[pos] = machine->pageTable[missVpn];
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

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
        printf("Exception SC_Halt!\n");  
        DEBUG('a', "Shutdown, initiated by user program.\n");
        currentThread->space->delNumPages();
        printTlbMissRatio();   
        currentThread->Finish();
    }
    else if((which == SyscallException) && (type == SC_Exit)){
        printf("Exception SC_Exit!\n");   
        currentThread->space->delNumPages();
        printTlbMissRatio();
 
        currentThread->Finish();
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

