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
							// b13-b0 signed int		alpha threshold
	uint32_t cntr_thresh_gamma;			//address: h04
							// b13-b0 signed int		gamma threshold
	uint32_t cntr_mintime_alpha;			//address: h08
							// 32bit unsigned int		alpha min. duration to be counted as an event
	uint32_t cntr_mintime_gamma;			//address: h0C
							// 32bit unsigned int		gamma min. duration to be counted as an event
	uint32_t reset_fifo;				//address: h10
							// just write anything to this reg to reset the fifo
	uint32_t mes_lost;				//address: h14
							// b31-0 : number of lost samples (32 bit unsigned)
	uint32_t mes_in_queue;				//address: h18
							// b31-16 : maximum number of samples in queue at any time since laste reset (16 bit unsigned) , b15-0 : number of samples currently in queue (16 bit unsigned)											
	uint32_t out_a_time_shift;			//address: h1C
							// b7-b0 : used to add a time delay on channel A (8 bit unsigned, 1==1e-8 s)
	uint32_t mes_data;				//address: h20
							// b31 : mes_isd (1=new, 0=empty), b30 : type (0 = alpha, 1 = gamma), b29-16 :  amplitude (14 bit signed), b15-b0 : empty (0s)
	uint32_t mes_cntr_t0;				//address: h21	
							// 32bit unsigned int		time difference from falling threshold of last peak to rising threshold of current peak			
	uint32_t mes_cntr_t1;				//address: h21	
							// 32bit unsigned int		time from rising threshold crossing to falling threshold crossing (peak width)	##READING OF THIS REGISTER CLEARS THIS PEAK FROM FIFO##	
//	uint32_t mes_cntr_t2;				//address: h21
//							// 32bit unsigned int		time from rising threshold crossing to maximum amplitude (peak max timestamp)	##READING OF THIS REGISTER CLEARS THIS PEAK FROM FIFO##		
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

inline int AGC_reset_fifo()
{
	AGC->reset_fifo=0;
	return 0;
}

inline unsigned int AGC_get_num_lost()
{
	return AGC->mes_lost;
}

inline unsigned int AGC_get_in_queue()	
{
	return (AGC->mes_in_queue&0x0000FFFF);
}

inline unsigned int AGC_get_max_in_queue()
{
	return (AGC->mes_in_queue&0xFFFF0000)>>16;
}

int AGC_setup(int cntr_thresh_alpha, int cntr_thresh_gamma, unsigned int cntr_mintime_alpha, unsigned int cntr_mintime_gamma, unsigned char out_a_time_shift)	
{
	if (cntr_thresh_alpha>8191) cntr_thresh_alpha=8191;			//threshold from -8192 to 8191
	else if (cntr_thresh_alpha<-8192) cntr_thresh_alpha=-8192;		//if threshold is negative the peak is assumed to be inverted
	if (cntr_thresh_gamma>8191) cntr_thresh_gamma=8191;
	else if (cntr_thresh_gamma<-8192) cntr_thresh_gamma=-8192;
	AGC->cntr_thresh_alpha = (cntr_thresh_alpha&0x3FFF);
	AGC->cntr_thresh_gamma = (cntr_thresh_gamma&0x3FFF);
	AGC->cntr_mintime_alpha = cntr_mintime_alpha;				//time is: cntr_mintime_alpha * 8 ns
	AGC->cntr_mintime_gamma = cntr_mintime_gamma;
	AGC->out_a_time_shift = out_a_time_shift;
	return 0;
}

inline int AGC_get_sample(bool *isalpha, int *amplitude, unsigned int *cntr_t0, unsigned int *cntr_t1)
{
	uint32_t temp;
	temp=AGC->mes_data;
	if (!(temp&0x80000000)) return 1;					//nothing in queue, return 1
	if (temp&0x40000000) *isalpha=false;
	else *isalpha=true;
	*amplitude = (temp&0x3FFF0000)>>16;
	if (*amplitude&0x2000) {*amplitude^=0x2000; *amplitude^=0xFFFFE000;}
	*cntr_t0=AGC->mes_cntr_t0;
	*cntr_t1=AGC->mes_cntr_t1;
//	*cntr_t2=AGC->mes_cntr_t2;
	return 0;								//new data was returned
}

