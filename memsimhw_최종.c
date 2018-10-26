//
// Virual Memory Simulator Homework
// Two-level page table system
// Inverted page table with a hashing system 
// Student Name:김세훈
// Student Number:B311030
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes

struct pageTableEntry {
	int level;				// page table level (1 or 2)
	char valid;
	struct pageTableEntry *secondLevelPageTable;	// valid if this entry is for the first level page table (level = 1)
	int frameNumber;								// valid if this entry is for the second level page table (level = 2)
};

struct framePage {
	int number;			// frame number
	int pid;			// Process id that owns the frame
	int virtualPageNumber;			// virtual page number using the frame
	struct framePage *lruLeft;	// for LRU circular doubly linked list
	struct framePage *lruRight; // for LRU circular doubly linked list
};

struct invertedPageTableEntry {
	int pid;				// process id
	int virtualPageNumber;		// virtual page number
	int frameNumber;			// frame number allocated
	struct invertedPageTableEntry *next;
};

struct procEntry {
	char *traceName;			// the memory trace name
	int pid;					// process (trace) id
	int ntraces;				// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAccess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;			// The number of page faults
	int numPageHit;				// The number of page hits
	struct pageTableEntry *firstLevelPageTable;
	FILE *tracefp;
};

struct framePage *oldestFrame; // the oldest frame pointer

int firstLevelBits;//Level 1의 주소비트 크기 20-firstLevelBits=secondLevelBits;
int phyMemSizeBits;//12에서 3비트 Frame 8개
int numProcess;

void initPhyMem(struct framePage *phyMem, int nFrame) {
	int i;
	for (i = 0; i < nFrame; i++) {
		phyMem[i].number = i;
		phyMem[i].pid = -1;
		phyMem[i].virtualPageNumber = -1;
		phyMem[i].lruLeft = &phyMem[(i - 1 + nFrame) % nFrame];
		phyMem[i].lruRight = &phyMem[(i + 1 + nFrame) % nFrame];
	}

	oldestFrame = &phyMem[0];

}

void secondLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames) {
	int i, k;
	int isEnd = 1;
	int nFrame;
	unsigned firstAddr, secondAddr;
	unsigned ofirstAddr, osecondAddr, oAddr;

	int secondLevelBits = 20 - firstLevelBits;
	nFrame = 1 << (phyMemSizeBits - PAGESIZEBITS);
	//fscanf 로 한줄 한줄읽어서 몇개의 2nd page 가 필요한지, 몇개가 page fault인지 계산해서 저장
	unsigned addr, physicalAddress, tempAddr;
	char rw;

	int sizeFirst = 1;
	int sizeSecond = 1;

	sizeFirst = 1 << firstLevelBits;
	sizeSecond = 1 << secondLevelBits;

	for (i = 0; i<numProcess; i++)
	{
		procTable[i].ntraces = 0;
		procTable[i].num2ndLevelPageTable = 0;
		procTable[i].numPageFault = 0;
		procTable[i].numPageHit = 0;
		procTable[i].firstLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*sizeFirst);
	}
	//기본값 초기화

	while (isEnd == 1)
	{
		for (i = 0; i < numProcess; i++)
		{
			if (fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) != EOF)
				//읽을 내용이 있을 때만 읽는다 
			{
				firstAddr = (addr >> (32 - firstLevelBits));
				secondAddr = (addr << firstLevelBits);
				secondAddr = secondAddr >> (32 - secondLevelBits);
				procTable[i].ntraces++;
				//받은 주소로 1-lelvel 주소비트와 2-level 주소비트를 구한다.

				if (procTable[i].firstLevelPageTable[firstAddr].valid != 'o')
				{//1-level 페이지가 처음 접근이라면
					procTable[i].firstLevelPageTable[firstAddr].valid = 'o';
					procTable[i].firstLevelPageTable[firstAddr].level = 1;
					procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*sizeSecond);
					//1-level 페이지에 대한 2-level 페이지를 생성한다.
					procTable[i].num2ndLevelPageTable++;
				}
				if (procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].valid != 'o')
				{	//secondpage가 처음 접근 이거나 valid ==x 인 상태라면
					procTable[i].numPageFault++;
					procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].level = 2;
					procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].valid = 'o';

					if (oldestFrame->pid != -1)
					{//할당할 프레임을 다른 페이지에서 사용하고 있었다면!
						oAddr = oldestFrame->virtualPageNumber;
						ofirstAddr = (oAddr >> (32 - firstLevelBits));
						osecondAddr = (oAddr << firstLevelBits);
						osecondAddr = osecondAddr >> (32 - secondLevelBits);
						procTable[oldestFrame->pid].firstLevelPageTable[ofirstAddr].secondLevelPageTable[osecondAddr].valid = 'x';
						//해당 페이지 엔트리로 찾아가서 invalid로 바꿔준다.
					}
					oldestFrame->pid = procTable[i].pid;
					oldestFrame->virtualPageNumber = addr;
					//할당할 프레임에 정보를 넣어준다.
					procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber = oldestFrame->number;
					//어떤 프레임에 넣었는지 페이지엔트리에 저장한다.
					oldestFrame = oldestFrame->lruRight;
					//할당한 프레임이 최신화 되도록 oldestFrame의 위치를 옮겨준다.
				}
				else if (procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].valid == 'o')
				{//valid==0 즉 hit라면!
					if (oldestFrame->pid == phyMemFrames[procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber].pid &&
						oldestFrame->virtualPageNumber == phyMemFrames[procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber].virtualPageNumber)
					{//만약 Hit한 프레임이 현재 oldest라면 
						procTable[i].numPageHit++;
						oldestFrame = oldestFrame->lruRight;
						//oldest만 한칸 옮겨주면된다.
					}
					else
					{//그것이 아니라면 oldest에서 가장 먼쪽으로 노드의 위치를 변경해줘야한다. 
						phyMemFrames[procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber].lruLeft->lruRight =
							phyMemFrames[procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber].lruRight;
						//해당 프레임의 왼쪽 프레임이 오른쪽 프레임을 가리키게한다.
						phyMemFrames[procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber].lruRight->lruLeft =
							phyMemFrames[procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber].lruLeft;
						//해당 프레임의 오른쪽 프레임이 왼쪽 프레임을 가리키게한다.


						oldestFrame->lruLeft->lruRight = &phyMemFrames[procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber];
						phyMemFrames[procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber].lruLeft = oldestFrame->lruLeft;
						//oldest 프레임의 왼쪽 프레임이 오른쪽으로 해당 프레임을 가리키게 한다, 해당프레임이 왼쪽으로 oldest 프레임의 왼쪽 프레임을 가리키게한다.

						phyMemFrames[procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber].lruRight = oldestFrame;
						oldestFrame->lruLeft = &phyMemFrames[procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber];
						//oldest 프레임이 왼쪽으로 해당 프레임을 가리키게 한다. 해당프레임이 오른쪽으로 oldest프레임을 가리키게한다.
						procTable[i].numPageHit++;
					}
				}
				//물리주소 계산
				physicalAddress = procTable[i].firstLevelPageTable[firstAddr].secondLevelPageTable[secondAddr].frameNumber;
				physicalAddress = physicalAddress << PAGESIZEBITS;
				tempAddr = addr << (32 - PAGESIZEBITS);
				tempAddr = tempAddr >> (32 - PAGESIZEBITS);
				physicalAddress += tempAddr;
				printf("2Level procID %d traceNumber %d virtual addr %x pysical addr %x\n", i, procTable[i].ntraces, addr, physicalAddress);
			}
			else
			{
				isEnd = 0;
			}
		}
	}
	for (i = 0; i < numProcess; i++) {
		printf("**** %s *****\n", procTable[i].traceName);
		printf("Proc %d Num of traces %d\n", i, procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n", i, procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void invertedPageVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, int nFrame) {
	int i, index, temp, k, index2, count;
	int isEnd = 1;
	int hit = 0;

	struct invertedPageTableEntry * ptr;
	struct invertedPageTableEntry * tempPtr;

	unsigned addr, vAddr, temp2, physicalAddress, tempAddr;
	char rw;

	struct invertedPageTableEntry ** hashTable = (struct invertedPageTableEntry **)malloc(sizeof(struct invertedPageTableEntry *)*nFrame);
	for (i = 0; i < nFrame; i++)
	{
		hashTable[i] = (struct invertedPageTableEntry *)malloc(sizeof(struct invertedPageTableEntry));
		hashTable[i] = NULL;
	}//해쉬 테이블 동적할당

	while (isEnd == 1)
	{
		for (i = 0; i < numProcess; i++)
		{

			if (fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) != EOF)
			{//읽을 게 있다면 
				procTable[i].ntraces++;
				vAddr = addr >> PAGESIZEBITS;//오프셋을 제외한 가상주소 20bit
				index = ((vAddr + procTable[i].pid) % nFrame);//해쉬테이블을 접근할 인덱스 계산 

				ptr = hashTable[index];//ptr은 현재 가리키는 hashTable의 시작노드를 가리키는 포인터 

				hit = 0;//hit여부를 판단하는 변수

				if (ptr == NULL){ procTable[i].numIHTNULLAccess++; } //시작노드가 없다면 해당 인덱스는 비어있다
				else{ procTable[i].numIHTNonNULLAccess++; }//시작노드가 있다면 해당 인덱스는 비어있지 않다

				while (ptr != NULL)
				{//내가 찾는 노드가 해당 인덱스의 링크드 리스트에서 있는지 찾는 while문, 링크를 하나씩 따라간다.
					procTable[i].numIHTConflictAccess++;
					if (ptr->pid == procTable[i].pid&&ptr->virtualPageNumber == vAddr)
					{//HIT라면
						hit = 1;
						procTable[i].numPageHit++;

						if (ptr->frameNumber == oldestFrame->number)
						{//히트한 프레임이 현재 oldest프레임이라면 
							oldestFrame = oldestFrame->lruRight;//oldest만 한칸 옮겨준다
						}
						else
						{//아니라면 히트한 프레임이 oldset프레임에서 가장 먼쪽으로 위치하도록 이동한다.
							phyMemFrames[ptr->frameNumber].lruLeft->lruRight = phyMemFrames[ptr->frameNumber].lruRight;
							//해당 프레임의 왼쪽 프레임이 오른쪽 프레임을 가리키게한다.
							phyMemFrames[ptr->frameNumber].lruRight->lruLeft = phyMemFrames[ptr->frameNumber].lruLeft;
							//해당 프레임의 오른쪽 프레임이 왼쪽 프레임을 가리키게한다.


							oldestFrame->lruLeft->lruRight = &phyMemFrames[ptr->frameNumber];
							phyMemFrames[ptr->frameNumber].lruLeft = oldestFrame->lruLeft;
							//oldest 프레임의 왼쪽 프레임이 오른쪽으로 해당 프레임을 가리키게 한다, 해당프레임이 왼쪽으로 oldest 프레임의 왼쪽 프레임을 가리키게한다.

							phyMemFrames[ptr->frameNumber].lruRight = oldestFrame;
							oldestFrame->lruLeft = &phyMemFrames[ptr->frameNumber];
							//oldest 프레임이 왼쪽으로 해당 프레임을 가리키게 한다. 해당프레임이 오른쪽으로 oldest프레임을 가리키게한다.
						}
						break;//찾았다면 break
					}
					ptr = ptr->next;//아직 못찾았다면 다음 링크로 가본다.
				}

				if (hit == 0)
					//while문 다 돌아도 일치하는 노드가 없을 경우, 즉 Fault일 경우
				{
					procTable[i].numPageFault++;
					struct invertedPageTableEntry * node = (struct invertedPageTableEntry *)malloc(sizeof(struct invertedPageTableEntry));
					//새로운 노드를 만들고 링크의 가장 앞에 붙여줘야 한다.

					if (oldestFrame->virtualPageNumber != -1)
					{//해당프레임을 다른 해쉬테이블 엔트리 가 사용하고 있었다면 
						temp = oldestFrame->virtualPageNumber;//템프에 해당엔트리의 주소를 저장한다
						index2 = ((temp + oldestFrame->pid) % nFrame);
						ptr = hashTable[index2];//해당 엔트리를 찾아간다 

						count = 0;
						while (ptr != NULL)
						{//지울 노드를 찾기위해 링크를 따라간다 
							if (ptr->pid == oldestFrame->pid&&ptr->virtualPageNumber == temp)
							{
								if (count == 0)
								{//count가 0이라면 즉 첫번째 노드가 바로 찾는 그 노드라면
									hashTable[index2] = ptr->next;//링크를 뛰어넘음으로 해당 노드를 리스트에서 없애버린다.
								}

								else
								{
									if (ptr->next == NULL)
									{
										tempPtr->next = NULL;
									}
									else
									{
										tempPtr->next = ptr->next;
									}
								}
								free(ptr);
								break;
							}
							count++;
							tempPtr = ptr;//tempPtr은 ptr보다 하나 전의 노드를 가리킴으로 리스트에서 삭제를 하게한다.
							ptr = ptr->next;
						}//여기까지 기존내용 삭제 
					}
					//hashTable에 노드 추가 

					node->pid = procTable[i].pid;
					node->frameNumber = oldestFrame->number;
					node->virtualPageNumber = vAddr;
					//추가할 노드에 정보를 담고
					node->next = hashTable[(vAddr + procTable[i].pid) % nFrame];
					hashTable[(vAddr + procTable[i].pid) % nFrame] = node;
					//노드를 리스트의 가장 앞에연결한다 
					oldestFrame->pid = node->pid;
					oldestFrame->virtualPageNumber = vAddr;
					oldestFrame = oldestFrame->lruRight;
					//프레임정보를 갱신하고 oldest를 한칸 옮겨준다.
				}
				physicalAddress = hashTable[index]->frameNumber;
				physicalAddress = physicalAddress << PAGESIZEBITS;
				tempAddr = addr << (32 - PAGESIZEBITS);
				tempAddr = tempAddr >> (32 - PAGESIZEBITS);
				physicalAddress += tempAddr;
				//물리주소 계산
				printf("IHT procID %d traceNumber %d virtual addr %x pysical addr %x\n", i, procTable[i].ntraces, addr, physicalAddress);
			}
			else
			{
				isEnd = 0;
			}
		}
	}
	for (i = 0; i < numProcess; i++) {
		printf("**** %s *****\n", procTable[i].traceName);
		printf("Proc %d Num of traces %d\n", i, procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n", i, procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n", i, procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n", i, procTable[i].numIHTNonNULLAccess);
		printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAccess == procTable[i].ntraces);
	}
}

int main(int argc, char *argv[]) {
	int i;


	firstLevelBits = atoi(argv[1]);
	phyMemSizeBits = atoi(argv[2]);
	numProcess = argc - 3;
	if (argc < 4) {
		printf("Usage : %s firstLevelBits PhysicalMemorySizeBits TraceFileNames\n", argv[0]); exit(1);
	}

	if (phyMemSizeBits < PAGESIZEBITS) {
		printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits %d\n", phyMemSizeBits, PAGESIZEBITS); exit(1);
	}
	if (VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits <= 0) {
		printf("firstLevelBits %d is too Big\n", firstLevelBits); exit(1);
	}


	int nFrame = (1 << (phyMemSizeBits - PAGESIZEBITS)); assert(nFrame>0);
	//RAM의 Frame 개수가 몇개인지!

	struct procEntry *procTable = (struct procEntry*)malloc(sizeof(struct procEntry)*numProcess);
	struct framePage *phyMem = (struct framePage*)malloc(sizeof(struct framePage)*nFrame);
	initPhyMem(phyMem, nFrame);
	// initialize procTable for two-level page table
	for (i = 0; i < numProcess; i++) {
		procTable[i].tracefp = fopen(argv[i + 3], "r");
		procTable[i].traceName = argv[i + 3];
		procTable[i].pid = i;

		printf("process %d opening %s\n", i, argv[i + 3]);
	}
	printf("\nNum of Frames %d Physical Memory Size %ld bytes\n", nFrame, (1L << phyMemSizeBits));

	printf("=============================================================\n");
	printf("The 2nd Level Page Table Memory Simulation Starts .....\n");
	printf("=============================================================\n");
	secondLevelVMSim(procTable, phyMem);


	// initialize procTable for the inverted Page Table


	for (i = 0; i < numProcess; i++) {
		// rewind tracefiles
		free(procTable[i].firstLevelPageTable);
		procTable[i].ntraces = 0;
		procTable[i].num2ndLevelPageTable = 0;
		procTable[i].numIHTConflictAccess = 0;
		procTable[i].numIHTNULLAccess = 0;
		procTable[i].numIHTNonNULLAccess = 0;
		procTable[i].numPageFault = 0;
		procTable[i].numPageHit = 0;
		procTable[i].firstLevelPageTable = NULL;
		rewind(procTable[i].tracefp);
	}
	initPhyMem(phyMem, nFrame);

	printf("=============================================================\n");
	printf("The Inverted Page Table Memory Simulation Starts .....\n");
	printf("=============================================================\n");

	invertedPageVMSim(procTable, phyMem, nFrame);

	return(0);
}