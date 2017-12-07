/*
    Alpha Gamma Counter
    Copyright (C) 2017  Mario Vretenar

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <cmath>

#define PI 3.14159265
#define FREQ 125e6	//fpga clock freq

////--------------------------- AGC regs -----------------------------------////

#define AGC_BASE_ADDR		0x40600000
#define AGC_BASE_SIZE		0x10000

struct _par_str{
	uint32_t cntr_thresh_alpha;			//address: h00
	                           			// b14 : alpha trig edge, b13-b0 : alpha threshold (14 bit signed)
	uint32_t cntr_thresh_gamma;			//address: h04
	                           			// b14 : gamma trig edge, b13-b0 : gamma threshold (14 bit signed)
	uint32_t cntr_mintime_alpha;			//address: h08
	                            			// b31-b0 : alpha min. duration to be counted as an event (32 bit unsigned)
	uint32_t cntr_mintime_gamma;			//address: h0C
	                            			// b31-b0 : gamma min. duration to be counted as an event (32 bit unsigned)
	uint32_t reset_fifo;				//address: h10
	                    				// just write anything to this reg to reset the fifo
	uint32_t mes_lost;				//address: h14
	                  				// b31-0 : number of lost samples (32 bit unsigned)
	uint32_t mes_in_queue;				//address: h18
	                      				// b31-16 : maximum number of samples in queue at any time since laste reset (16 bit unsigned) , b15-0 : number of samples currently in queue (16 bit unsigned)											
	uint32_t UNU0;					//address: h1C
	              					// Unused
	uint32_t mes_data;				//address: h20
	                  				// b31 : mes_isd (1=new, 0=empty), b30 : type (0 = alpha, 1 = gamma), b29-16 :  amplitude (14 bit signed), b15-b0 : empty (0s)
	uint32_t mes_timestamp_l;			//address: h24	
	                         			// 32bit unsigned int	lower part [31:0] of 64 bit unsigned timestamp			
	uint32_t mes_timestamp_h;			//address: h28	
	                         			// 32bit unsigned int	higher part [63:32] of 64 bit unsigned timestamp	##READING OF THIS REGISTER CLEARS THIS PEAK FROM FIFO##						
};

_par_str *AGC = NULL;	//parameters

////------------------------------------------------------------------------////

int _mem_fd = -1;

int AGC_exit(void)
{
	if(AGC)
	{
		if(munmap(AGC, AGC_BASE_SIZE) < 0)
			{fprintf(stderr, "munmap() failed: %s\n", strerror(errno)); return -1;}
		AGC = NULL;
	}
	if (_mem_fd>=0)
	{
		close (_mem_fd);
		_mem_fd=0;
	}
	return 0;
}

int AGC_init(void)
{
	_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if(_mem_fd < 0)	{fprintf(stderr, "open(/dev/mem) failed: %s\n", strerror(errno)); return -1;}
	
	void *page_ptr;
	long page_addr, page_off;
	page_addr = AGC_BASE_ADDR & (~(sysconf(_SC_PAGESIZE)-1));
	page_off  = AGC_BASE_ADDR - page_addr;
    
	page_ptr = mmap(NULL, AGC_BASE_SIZE, PROT_READ | PROT_WRITE,MAP_SHARED, _mem_fd, page_addr);
	if((void *)page_ptr == MAP_FAILED)	{fprintf(stderr, "mmap() failed: %s\n", strerror(errno)); AGC_exit(); return -1;}
  	  
	AGC = (_par_str *)page_ptr + page_off;
	
	return 0;
}

void AGC_reset_fifo()
{
	AGC->reset_fifo=0;
}

inline uint32_t AGC_get_num_lost()
{
	return AGC->mes_lost;
}

inline uint16_t AGC_get_in_queue()	
{
	return (AGC->mes_in_queue&0x0000FFFF);
}

inline uint16_t AGC_get_max_in_queue()
{
	return (AGC->mes_in_queue&0xFFFF0000)>>16;
}

int AGC_setup(int cntr_thresh_alpha, int cntr_thresh_gamma, bool cntr_edge_alpha, bool cntr_edge_gamma, uint32_t cntr_mintime_alpha, uint32_t cntr_mintime_gamma)	
{
	if (cntr_thresh_alpha>8191) cntr_thresh_alpha=8191;			//threshold from -8192 to 8191
	else if (cntr_thresh_alpha<-8192) cntr_thresh_alpha=-8192;		//if threshold is negative the peak is assumed to be inverted
	if (cntr_thresh_gamma>8191) cntr_thresh_gamma=8191;
	else if (cntr_thresh_gamma<-8192) cntr_thresh_gamma=-8192;
	AGC->cntr_thresh_alpha = (cntr_thresh_alpha&0x3FFF)|(cntr_edge_alpha?0x4000:0);
	AGC->cntr_thresh_gamma = (cntr_thresh_gamma&0x3FFF)|(cntr_edge_gamma?0x4000:0);
	AGC->cntr_mintime_alpha = cntr_mintime_alpha;				//time is: cntr_mintime_alpha * 8 ns  (32bit unsigned)
	AGC->cntr_mintime_gamma = cntr_mintime_gamma;
	return 0;
}

inline int AGC_get_sample(bool *isalpha, int *amplitude, uint64_t *timestamp)
{
	uint32_t temp;
	temp=AGC->mes_data;
	if (!(temp&0x80000000)) return 1;					//nothing in queue, return 1
	if (temp&0x40000000) *isalpha=false;
	else *isalpha=true;
	*amplitude = (temp&0x3FFF0000)>>16;
	if (*amplitude&0x2000) *amplitude^=0xFFFFC000;
	*timestamp=AGC->mes_timestamp_l;
	*timestamp|=(uint64_t)AGC->mes_timestamp_h<<32;
	return 0;								//new data was returned
}

