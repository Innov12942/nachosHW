// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	return FALSE;		// not enough space
    
    int remainSec = numSectors;
    int secEachtime = remainSec;
    int nxtSector = 0;
    int loopCnt = 0;
    int tmpSectors[SectorSize / sizeof(int)];

    while(remainSec > 0){
        if(remainSec > NumDirect - 1)
            secEachtime = NumDirect - 1;
        else
            secEachtime = remainSec;

        if(loopCnt == 0)
            for (int i = 0; i < secEachtime; i++)
                dataSectors[i] = freeMap->Find();
        else{
            for (int i = 0; i < secEachtime; i++)
                tmpSectors[i] = freeMap->Find();
        }

        if(remainSec > NumDirect - 1){
            if(loopCnt == 0){
                dataSectors[NumDirect - 1] = freeMap->Find();
                nxtSector = dataSectors[NumDirect - 1];
            }
            else{
                tmpSectors[NumDirect - 1] = freeMap->Find();
                ASSERT(nxtSector > 0);
                synchDisk->WriteSector(nxtSector, (char *)tmpSectors);
                nxtSector = tmpSectors[NumDirect - 1];               
            }
        }
        else{
            if(loopCnt != 0){
                ASSERT(nxtSector > 0);
                synchDisk->WriteSector(nxtSector, (char *)tmpSectors);
            }
        }

        remainSec -=  (NumDirect - 1);
        loopCnt ++;
    }
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    for (int i = 0; i < numSectors; i++) {
	   ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	   freeMap->Clear((int) dataSectors[i]);
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    int numBytes = offset;
    int numSectors  = offset / SectorSize + 1;

    int remainSec = numSectors;
    int nxtSector = 0;
    int loopCnt = 0; 
    int curSecAry[SectorSize / sizeof(int)];
    while(remainSec > 0){
        if(loopCnt != 0)
            synchDisk->ReadSector(nxtSector, (char *)curSecAry);
        if(remainSec > NumDirect - 1){
            if(loopCnt != 0)
                nxtSector = curSecAry[NumDirect - 1];
            else
                nxtSector = dataSectors[NumDirect - 1];
        }
        else{
            if(loopCnt == 0)
                return dataSectors[remainSec - 1];
            else
                return curSecAry[remainSec- 1];
        }     
        remainSec -=  NumDirect - 1;
        loopCnt ++;
    }

    printf("ByteToSector error!\n");
    ASSERT(false);
    return dataSectors[offset / SectorSize];
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < numSectors; i++)
	printf("%d ", dataSectors[i]);
    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors; i++) {
	synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
    delete [] data;
}

void
FileHeader::setType(const char *name){
    char buf[30];
    strcpy(buf, name);
    int len = strlen(name);
    int i = 0;
    for(i = 0; i < len; i++)
        if(buf[i] == '.')
            break;
    if(i + 4 < len)
        buf[i + 4] = '\0';
    if(i < len - 1){
        const char *tmp = buf + i + 1;
        strcpy(type, tmp);
    }
    else{
        const char *tmp = "??";
        strcpy(type, tmp);
    }
}

bool
FileHeader:: Extend(BitMap *freeMap, int size){
    numBytes += size;
    int originSecs = numSectors;
    numSectors = divRoundUp(numBytes, SectorSize);
    int additionalSecs = numSectors - originSecs;
    if(additionalSecs == 0)
        return true;
    else if(freeMap->NumClear() < additionalSecs){
        printf("Extend faild NumClear:%d\n",freeMap->NumClear() );
        freeMap->Print();
        return false;
    }
    int remainSec = numSectors;
    int secEachtime = remainSec;
    int nxtSector = 0;
    int loopCnt = 0;
    int tmpSectors[SectorSize / sizeof(int)];
    while(remainSec > 0){
        if(remainSec > NumDirect - 1)
            secEachtime = NumDirect - 1;
        else
            secEachtime = remainSec;

        if(originSecs - secEachtime >= 0){
            if(originSecs == NumDirect - 1 && remainSec > NumDirect - 1)
                int x;
            else{
                if(remainSec > NumDirect - 1){
                    if(loopCnt == 0){
                        nxtSector = dataSectors[NumDirect - 1];
                        synchDisk->ReadSector(nxtSector, (char *)tmpSectors);
                    }
                    else{
                        nxtSector = tmpSectors[NumDirect - 1];
                        synchDisk->ReadSector(nxtSector, (char *)tmpSectors);
                    } 
                }
                remainSec -=  secEachtime;-
                loopCnt ++;
                originSecs -= secEachtime;
                continue;
            }
        }

        if(loopCnt == 0)
            for (int i = originSecs; i < secEachtime; i++)
                dataSectors[i] = freeMap->Find();
        else{
            for (int i = originSecs; i < secEachtime; i++)
                tmpSectors[i] = freeMap->Find();
        }

        if(remainSec > NumDirect - 1){
            if(loopCnt == 0){
                dataSectors[NumDirect - 1] = freeMap->Find();
                nxtSector = dataSectors[NumDirect - 1];
            }
            else{
                tmpSectors[NumDirect - 1] = freeMap->Find();
                ASSERT(nxtSector > 0);
                synchDisk->WriteSector(nxtSector, (char *)tmpSectors);
                nxtSector = tmpSectors[NumDirect - 1];               
            }
        }
        else{
            if(loopCnt != 0){
                ASSERT(nxtSector > 0);
                synchDisk->WriteSector(nxtSector, (char *)tmpSectors);
            }
        }

        remainSec -=  secEachtime;
        loopCnt ++;
        originSecs -= secEachtime;
    }
    return true;

};

void 
FileHeader::setCreate_t(){
    time(&create_t);
}

void 
FileHeader::setVisit_t(){
    time(&visit_t);
}

void 
FileHeader::setModify_t(){
    time(&modify_t);
}

