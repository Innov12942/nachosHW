// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    printf("file size:%d\n", size);
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    //ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory
    ASSERT(numPages <= NumDiskPages);

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    fileSystem->Create("virMem", 128 * 64);
    OpenFile *vmFile = fileSystem->Open("virMem");
    ASSERT(vmFile != NULL);

    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
	int dmNo = (pageTable[i].physicalPage = machine->bmForDm->Find());
    printf("AddrSpace apply DiskPage %d\n", dmNo);
    ASSERT(dmNo != -1);
	pageTable[i].valid = FALSE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    pageTable[i].DiskPage = dmNo;
    //bzero(machine->mainMemory + pmNo * PageSize, PageSize);
    char c = 0;
/*    int x;
    scanf("%d", &x);*/
    for(int i = 0; i < PageSize; i++){
        //printf("##");
        vmFile->WriteAt(&c, 1, dmNo * PageSize + i);
    }
    }
    
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
    //bzero(machine->mainMemory, size);

// then, copy in the code and data segments into memory

    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);

//if not using disk vm
/*
        for(int k = 0; k < noffH.code.size; k++){
            int codeVpn = (noffH.code.virtualAddr + k) / PageSize;
            int offset = (noffH.code.virtualAddr + k) % PageSize;
            int physAddr = pageTable[codeVpn].physicalPage * PageSize + offset;
            executable->ReadAt(machine->mainMemory + physAddr,
                1, noffH.code.inFileAddr + k);
        }
  */
        for(int k = 0; k < noffH.code.size; k++){
            int codeVpn = (noffH.code.virtualAddr + k) / PageSize;
            int offset = (noffH.code.virtualAddr + k) % PageSize;
            int diskAddr = pageTable[codeVpn].DiskPage * PageSize + offset;
            char tempByte;
            executable->ReadAt(&tempByte,
                1, noffH.code.inFileAddr + k);
            vmFile->WriteAt(&tempByte, 1, diskAddr);
        }


     /*   executable->ReadAt(&(machine->mainMemory[noffH.code.virtualAddr]),
			noffH.code.size, noffH.code.inFileAddr);*/
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);

//if not use disk vm
/*        for(int k = 0; k < noffH.initData.size; k++){
            int dataVpn = (noffH.initData.virtualAddr + k) / PageSize;
            int offset = (noffH.initData.virtualAddr + k) % PageSize;
            int physAddr = pageTable[dataVpn].physicalPage * PageSize + offset;
            executable->ReadAt(machine->mainMemory + physAddr,
                1, noffH.initData.inFileAddr + k);
        }*/
        for(int k = 0; k < noffH.initData.size; k++){
            int dataVpn = (noffH.initData.virtualAddr + k) / PageSize;
            int offset = (noffH.initData.virtualAddr + k) % PageSize;
            int diskAddr = pageTable[dataVpn].DiskPage * PageSize + offset;
            char tempByte;
            executable->ReadAt(&tempByte,
                1, noffH.initData.inFileAddr + k);
            vmFile->WriteAt(&tempByte, 1, diskAddr);
        }

/*        executable->ReadAt(&(machine->mainMemory[noffH.initData.virtualAddr]),
			noffH.initData.size, noffH.initData.inFileAddr);*/
    }
    delete vmFile;

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
}

void
AddrSpace:: delNumPages(){
    for(int i = 0; i < numPages; i++){
        if(pageTable[i].valid){
            machine->bmForPm->Clear(pageTable[i].physicalPage);
            printf("AddrSpace free physicalPage %d\n", pageTable[i].physicalPage);
        }
        else{
            machine->bmForDm->Clear(pageTable[i].DiskPage);
            printf("AddrSpace free diskPage %d\n", pageTable[i].DiskPage);
        }
    }
}

int 
AddrSpace::KickOnePageOut(){
    for(int i = 0; i < numPages; i++){
        if(pageTable[i].valid){
            bool flag = false;
            for(int j = 0; j < TLBSize; j++){
                if(machine->tlb[j].virtualPage == pageTable[i].virtualPage){
                //    printf("kick vp %d pp %d in tlb %d out while KickOnePageOut\n",
                 //    pageTable[i].virtualPage, pageTable[i].physicalPage ,j);
                    machine->tlb[j].valid = false;
                    flag =true;
                }
            }

            if(flag)
                continue;

            pageTable[i].valid = FALSE;
            machine->bmForPm->Clear(pageTable[i].physicalPage);
            //pageTable[i].DiskPage = machine->bmForDm->Find();
            ASSERT(pageTable[i].DiskPage != -1);

            if(pageTable[i].dirty){
                //printf("AddrSpace kick vp %d to disk\n", i);
                OpenFile *vmFile = fileSystem->Open("virMem");
                ASSERT(vmFile != NULL);
                vmFile->WriteAt(machine->mainMemory + pageTable[i].physicalPage * PageSize,
                    PageSize, pageTable[i].DiskPage * PageSize);
                delete vmFile;
            }
           // printf("return num %d\n", pageTable[i].physicalPage);
            return pageTable[i].physicalPage;
        }
    }

    return -1;
}

bool 
AddrSpace::kickAll(){
    for(int i = 0; i < numPages; i++){
        if(pageTable[i].valid && pageTable[i].dirty){
            OpenFile *vmFile = fileSystem->Open("virMem");
            ASSERT(vmFile != NULL);
            vmFile->WriteAt(machine->mainMemory + pageTable[i].physicalPage * PageSize,
                PageSize, pageTable[i].DiskPage * PageSize);
            delete vmFile;
        }
        pageTable[i].valid = false;
    }

    return true;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{
    for(int i = 0; i < TLBSize; i++)
        machine->tlb[i].valid = FALSE; 
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

