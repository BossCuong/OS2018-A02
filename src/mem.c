
#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct
{
	uint32_t proc; // ID of process currently uses this page
	int index;	 // Index of the page in the list of pages allocated
				   // to the process.
	int next;	  // The next page in the list. -1 if it is the last
				   // page.
} _mem_stat[NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void)
{
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr)
{
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr)
{
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr)
{
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct page_table_t *get_page_table(
	addr_t index, // Segment level index
	struct seg_table_t *seg_table)
{ // first level table

	/*
	 * TODO: Given the Segment index [index], you must go through each
	 * row of the segment table [seg_table] and check if the v_index
	 * field of the row is equal to the index
	 *
	 * */

	if (seg_table->size == 0)
		return NULL;

	int i = 0;
	addr_t seg_index;
	struct page_table_t *r_pages = NULL;

	for (i = 0; i < seg_table->size; i++)
	{
		// Enter your code here
		seg_index = seg_table->table[i].v_index;

		if (seg_index == index)
		{
			r_pages = seg_table->table[i].pages;
			break;
		}
	}

	return r_pages;
}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
	addr_t virtual_addr,   // Given virtual address
	addr_t *physical_addr, // Physical address to be returned
	struct pcb_t *proc)
{ // Process uses given virtual address

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr);
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);

	/* Search in the first level */
	struct page_table_t *page_table = NULL;
	page_table = get_page_table(first_lv, proc->seg_table);
	if (page_table == NULL)
	{
		return 0;
	}

	int i;
	addr_t p_index;
	for (i = 0; i < page_table->size; i++)
	{
		if (page_table->table[i].v_index == second_lv)
		{
			/* TODO: Concatenate the offset of the virtual addess
			 * to [p_index] field of page_table->table[i] to 
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */

			p_index = page_table->table[i].p_index;

			*physical_addr = (p_index << OFFSET_LEN) + offset;

			return 1;
		}
	}
	return 0;
}

addr_t alloc_mem(uint32_t size, struct pcb_t *proc)
{
	pthread_mutex_lock(&mem_lock);
	addr_t ret_mem = 0;
	/* TODO: Allocate [size] byte in the memory for the
	 * process [proc] and save the address of the first
	 * byte in the allocated memory region to [ret_mem].
	 * */

	uint32_t num_pages = ((size % PAGE_SIZE) == 0) ? size / PAGE_SIZE : size / PAGE_SIZE + 1; // Number of pages we will use
	int mem_avail = 0;																		  // We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */

	uint32_t free_p_page = 0;

	//Get physical page available
	for (int i = 0; i < NUM_PAGES; i++)
		if (_mem_stat[i].proc == 0)
			++free_p_page;

	//Get number mem need to use,heap + stack ?
	uint32_t using_v_mem = proc->bp + num_pages * PAGE_SIZE;

	//Check that physical and virtual mem is available to use
	if (num_pages <= free_p_page)
		if (using_v_mem <= (1 << ADDRESS_SIZE))
			mem_avail = 1;

	if (mem_avail)
	{
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */
		int i = 0;
		int pre_page = -1;
		int p_index = -1;
		int cnt_page = 0;

		//Find first page
		while (_mem_stat[i].proc != 0)
			i++;

		pre_page = p_index = i;
		_mem_stat[i].proc = proc->pid;
		_mem_stat[i].index = cnt_page++;
		++i;
		//Update mem status of process
		while (cnt_page != num_pages)
		{
			//If mem isn't used
			if (_mem_stat[i].proc == 0)
			{
				//Update mem status
				_mem_stat[i].proc = proc->pid;
				_mem_stat[i].index = cnt_page++;
				_mem_stat[pre_page].next = i;

				pre_page = i;
			}
			++i;
		}
		//Next index of last page is -1
		_mem_stat[pre_page].next = -1;

		int seg_size;
		int page_size;
		addr_t bp = ret_mem; //break point

		/*----------------------Add entries to segment table and page table----------------*/
		//Init first seg table
		if (!proc->seg_table->size)
		{
			seg_size = ++proc->seg_table->size;
			proc->seg_table->table[seg_size - 1].v_index = seg_size - 1;
		}

		//Init first page table for first segment
		if (!proc->seg_table->table[seg_size - 1].pages)
		{
			proc->seg_table->table[seg_size - 1].pages = malloc(sizeof(struct page_table_t));
			proc->seg_table->table[seg_size - 1].pages->size = 0;
		}

		while (num_pages)
		{
			/*--------------Page process--------------------*/
			page_size = proc->seg_table->table[seg_size - 1].pages->size;

			//If page size < limit page of one segment,use that segment to store page
			if (page_size + 1 <= (1 << PAGE_LEN))
			{
				//Set new page
				page_size = ++proc->seg_table->table[seg_size - 1].pages->size;

				//
				proc->seg_table->table[seg_size - 1].pages->table[page_size - 1].v_index = get_second_lv(bp);

				//Map physic index of page equal first index in _mem_stat of process
				proc->seg_table->table[seg_size - 1].pages->table[page_size - 1].p_index = p_index;

				//Get next index in _mem_stat
				p_index = _mem_stat[p_index].next;

				num_pages--;

				//Increase breakpoint one page
				bp += PAGE_SIZE;
			}
			else //Creat new segment to store page
			{
				/*--------------Segment process--------------------*/
				seg_size = ++proc->seg_table->size;

				//Set index for segment,auto increament from 0
				proc->seg_table->table[seg_size - 1].v_index = seg_size - 1;

				//Init new page table for segment
				if (!proc->seg_table->table[seg_size - 1].pages)
				{
					proc->seg_table->table[seg_size - 1].pages = malloc(sizeof(struct page_table_t));
					proc->seg_table->table[seg_size - 1].pages->size = 0;
				}
			}
		}
	}
	pthread_mutex_unlock(&mem_lock);
	return ret_mem;
}

int free_mem(addr_t address, struct pcb_t *proc)
{
	/*TODO: Release memory region allocated by [proc]. The first byte of
	 * this region is indicated by [address]. Task to do:
	 * 	- Set flag [proc] of physical page use by the memory block
	 * 	  back to zero to indicate that it is free.
	 * 	- Remove unused entries in segment table and page tables of
	 * 	  the process [proc].
	 * 	- Remember to use lock to protect the memory from other
	 * 	  processes.  */
	/**********************************************************************************/
	pthread_mutex_lock(&mem_lock);
	addr_t physical_addr;
	int numPages = 0;
	int i;
	if (translate(address, &physical_addr, proc))
	{
		addr_t physical_page = physical_addr >> OFFSET_LEN;
		//Xac dinh so page can free
		while (physical_page != -1)
		{
			_mem_stat[physical_page].proc = 0;
			physical_page = _mem_stat[physical_page].next;
			numPages++;
		}
		addr_t virtual_addr = address;

		while (numPages != 0)
		{
			addr_t first_lv = get_first_lv(virtual_addr);
			addr_t second_lv = get_second_lv(virtual_addr);

			struct page_table_t *page_table = NULL;
			page_table = get_page_table(first_lv, proc->seg_table);

			//Gan v_index, p_index bang 1
			if (page_table != NULL)
			{
				for (i = 0; i < (1 << PAGE_LEN); i++)
				{
					if (second_lv == page_table->table[i].v_index)
					{
						page_table->table[i].v_index = -1;
						page_table->table[i].p_index = -1;
						virtual_addr = virtual_addr + PAGE_SIZE;
						numPages--;
						break;
					}
				}
			}
			else
			{
				break;
			}
		}
	}
	pthread_mutex_unlock(&mem_lock);
	return 0;
}

int read_mem(addr_t address, struct pcb_t *proc, BYTE *data)
{
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc))
	{
		*data = _ram[physical_addr];
		return 0;
	}
	else
	{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t *proc, BYTE data)
{
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc))
	{
		_ram[physical_addr] = data;
		return 0;
	}
	else
	{
		return 1;
	}
}

void dump(void)
{
	int i;
	for (i = 0; i < NUM_PAGES; i++)
	{
		if (_mem_stat[i].proc != 0)
		{
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				   i << OFFSET_LEN,
				   ((i + 1) << OFFSET_LEN) - 1,
				   _mem_stat[i].proc,
				   _mem_stat[i].index,
				   _mem_stat[i].next);
			int j;
			for (j = i << OFFSET_LEN;
				 j < ((i + 1) << OFFSET_LEN) - 1;
				 j++)
			{

				if (_ram[j] != 0)
				{
					printf("\t%05x: %02x\n", j, _ram[j]);
				}
			}
		}
	}
}
