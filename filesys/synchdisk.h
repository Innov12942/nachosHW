// synchdisk.h 
// 	Data structures to export a synchronous interface to the raw 
//	disk device.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef SYNCHDISK_H
#define SYNCHDISK_H

#include "disk.h"
#include "synch.h"

// The following class defines a "synchronous" disk abstraction.
// As with other I/O devices, the raw physical disk is an asynchronous device --
// requests to read or write portions of the disk return immediately,
// and an interrupt occurs later to signal that the operation completed.
// (Also, the physical characteristics of the disk device assume that
// only one operation can be requested at a time).
//
// This class provides the abstraction that for any individual thread
// making a request, it waits around until the operation finishes before
// returning.
class SynchDisk {
  public:
    SynchDisk(char* name);    		// Initialize a synchronous disk,
					// by initializing the raw Disk.
    ~SynchDisk();			// De-allocate the synch disk data
    
    void ReadSector(int sectorNumber, char* data);
    					// Read/write a disk sector, returning
    					// only once the data is actually read 
					// or written.  These call
    					// Disk::ReadRequest/WriteRequest and
					// then wait until the request is done.
    void WriteSector(int sectorNumber, char* data);
    
    void RequestDone();			// Called by the disk device interrupt
					// handler, to signal that the
					// current disk operation is complete.
    void readerIn(int sc){
        cntLck[sc]->Acquire();
        if(rdCnt[sc]++ == 0)
            wLck[sc]->Acquire();
        printf("reader count:%d\n", rdCnt[sc]);
        cntLck[sc]->Release();
    }
    void readerOut(int sc){
        cntLck[sc]->Acquire();
        if(rdCnt[sc]-- == 1)
            wLck[sc]->Release();

        cntLck[sc]->Release();
    }

    void writerIn(int sc){
        wLck[sc]->Acquire();
    }
    void writerOut(int sc){
        wLck[sc]->Release();
    }
    void openF(int sc){
        visNum[sc]++;
    }
    void closeF(int sc){
        visNum[sc]--;
        ASSERT(visNum[sc] >= 0);
    }
    bool tryRemove(int sc){
        //printf("visNum:%d\n", visNum[sc]);
        return visNum[sc] == 0;
    }

  private:
    Disk *disk;		  		// Raw disk device
    Semaphore *semaphore; 		// To synchronize requesting thread 
					// with the interrupt handler
    Lock *lock;		  		// Only one read/write request
					// can be sent to the disk at a time
    Lock *wLck[NumSectors];
    Lock *cntLck[NumSectors];
    int rdCnt[NumSectors];
    int visNum[NumSectors];
};

#endif // SYNCHDISK_H

