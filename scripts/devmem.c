/*
 * devmem2.c: Simple program to read/write from/to any location in memory.
 *
 *  Copyright (C) 2000, Jan-Derk Bakker (J.D.Bakker@its.tudelft.nl)
 *
 *
 * This software has been developed for the LART computing board
 * (http://www.lart.tudelft.nl/). The development has been sponsored by
 * the Mobile MultiMedia Communications (http://www.mmc.tudelft.nl/)
 * and Ubiquitous Communications (http://www.ubicom.tudelft.nl/)
 * projects.
 *
 * The author can be reached at:
 *
 *  Jan-Derk Bakker
 *  Information and Communication Theory Group
 *  Faculty of Information Technology and Systems
 *  Delft University of Technology
 *  P.O. Box 5031
 *  2600 GA Delft
 *  The Netherlands
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>
  
#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
  __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)
 
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

  /* A string listing valid short options letters.  */
const char* program_name;  /* The name of this program.  */
const char* const short_options = "a:t:c:w:v";
  /* An array describing valid long options.  */
const struct option long_options[] = {
    { "address",     1, NULL, 'a' },
    { "type",     1, NULL, 't' },
    { "count",     1, NULL, 'c' },
    { "write",     1, NULL, 'w' },
    { "verbose",   0, NULL, 'v' },
    { NULL,        0, NULL, 0   }   /* Required at end of array.  */
  };

void print_usage (FILE* stream, int exit_code)
{
  fprintf (stream, "Usage:  %s options\n", program_name);
  fprintf (stream,
           "  NOTE: see /opt/emerald/acq/rd/usr/src/linux/include/asm/arch/AT91RM9200.h\n"
           "  for AT91RM9200 chip addresses\n"
           "  -a  --address addr        Starting address to read[AT91C_ST_CRTR]\n"
           "  -t  --type type           Access width, b (byte), h (half), or w (word. [w]\n"
           "  -c  --count count         The number of consecutive valuse to read. [1]\n"
           "  -w  --write value         Write this value. [0]\n"
           "  -v  --verbose             Print verbose messages.\n");
  exit (exit_code);
}

/* map and then read memory add addr */
int map_read_mem (off_t addr, int access_type, unsigned long *read_result)
  {
  void *map_base, *virt_addr; 
  int fd;

  if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    return (fd);
  /* Map one page */
  map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
  if (map_base == (void *) -1)
    return ((int) map_base);
    
  virt_addr = map_base + (addr & MAP_MASK);
  switch(access_type)
    {
    case 'b':
      *read_result = *((unsigned char *) virt_addr);
      break;
    case 'h':
      *read_result = *((unsigned short *) virt_addr);
      break;
    case 'w':
      *read_result = *((unsigned long *) virt_addr);
      break;
    }
  printf("Value at address 0x%x (%p): 0x%x\n", addr, virt_addr, *read_result); 
  if (munmap(map_base, MAP_SIZE) == -1)
    return -1;
  return 0;
  }

int main(int argc, char **argv) 
  {
  int fd;
  void *map_base, *virt_addr; 
  unsigned long read_result, writeval;
  off_t initial_addr, addr;
  int access_type = 'w';
  int verbose = 0;  /* be verbose? */
  int count = 1, next_option;
  int write_flag = 0;
	
  do
    {
    next_option = getopt_long (argc, argv, short_options,
                               long_options, NULL);
    switch (next_option)
      {
      case 'a':   /* -a or --address */
        /* This option takes an argument, the initial baud rate */
        initial_addr = addr = strtoul (optarg, NULL, 0);
        break;
      case 't':   /* -t or --type */
        /* This option takes an argument, the initial baud rate */
        access_type = *optarg;
        if (access_type != 'b' && access_type != 'h' && access_type != 'w')
          {
          fprintf(stderr, "Illegal data type '%c'.\n", access_type);
          exit(2);
          }
        break;
      case 'c':   /* -c or --count */
        /* This option takes an argument, the initial baud rate */
        count = strtol (optarg, NULL, 0);
        break;
      case 'w':   /* -w or --write */
        /* This option takes an argument, the initial baud rate */
        write_flag = 1;
        writeval = strtol (optarg, NULL, 0);
        break;
      case 'v':   /* -v or --verbose */
        verbose = 1;
        break;
      case '?':   /* The user specified an invalid option.  */
        /* Print usage information to standard error, and exit with exit
           code one (indicating abonormal termination).  */
        print_usage (stderr, 1);
        break;
      case -1:    /* Done with options.  */
        break;
      default:    /* Something else: unexpected.  */
        abort ();
      }
    }
  while (next_option != -1);

  if (verbose)
    printf ("Kernel memory read program\n");

  if (verbose)
    printf("/dev/mem opened addr = 0x%x\n", addr); 
    
  for (; count > 0; count--)
    {
    if (map_read_mem (addr, access_type, &read_result) < 0)
      FATAL;
    switch(access_type)
      {
      case 'b':
        addr += sizeof (char);
        break;
      case 'h':
        addr += sizeof (short);
        break;
      case 'w':
        addr += sizeof (long);
        break;
      }
    }
	addr = initial_addr;
  if (write_flag) 
    {
    int fd;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
      return (fd);
    /* Map one page */
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
    if (map_base == (void *) -1)
      FATAL;
    printf("Memory mapped at address %p.\n", map_base); 
    virt_addr = map_base + (addr & MAP_MASK);
    switch(access_type) 
      {
      case 'b':
        *((unsigned char *) virt_addr) = writeval;
        read_result = *((unsigned char *) virt_addr);
        break;
      case 'h':
        *((unsigned short *) virt_addr) = writeval;
        read_result = *((unsigned short *) virt_addr);
        break;
      case 'w':
        *((unsigned long *) virt_addr) = writeval;
        read_result = *((unsigned long *) virt_addr);
        break;
      }
    printf("Written 0x%X; readback 0x%X\n", writeval, read_result); 
    if (munmap(map_base, MAP_SIZE) == -1)
      FATAL;
    }
  close(fd);
  return 0;
  }

