/*********************************************************************
 * test-tx2.c - PWM RPI4 test program

To make:
make

To run:
./test-tx2 -r 1 -d 1024 -g 1024 | less
./test-tx2 -r 1 -d 1024 -g 1024 | grep otal
./test-tx2 -r 1 -d 1024 -g 1024 | grep otal
 generate some waveform graphs:
./test-tx22 -d 200 -n 400 -s 1000 -r 1 -m 7 -g -v 
./test-tx22 -d 112 -n 400 -s 1000 -r 10 -m 7 -g -v 

 *********************************************************************/

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

/* see ~/boards/wyze-v2/kernel/include/linux/mfd/jz_tcu.h */
struct tx2_setup {
	int command;
	int duration;  /* in milliseconds */
};

struct tx2_setup setup =
	{
	.command = 58,
	.duration = 500,
	};

struct tx2_priv {
	struct tx2_setup tx2_cmd;
	int verbose;
	} priv_data;

#define TX2_CMD_FILE "/sys/devices/platform/jz-tcu/cmd"

/* A string listing valid short options letters.  */
const char* program_name;  /* The name of this program.  */
const char* const short_options = "c:d:v";
  /* An array describing valid long options.  */
const struct option long_options[] = {
    { "command",      1, NULL, 'c' },
    { "duration",     1, NULL, 'd' },
    { "verbose",   0, NULL, 'v' },
    { NULL,        0, NULL, 0   }   /* Required at end of array.  */
  };

void print_usage (FILE* stream, int exit_code)
{
  fprintf (stream, "Usage:  %s options\n", program_name);
  fprintf (stream,
		"  -c  --command      Commands: 22, 16, 58, 40, 52, 10, 28, 64, 46, 34\n"
		"  -d  --duration     Duration in milliseconds to send\n"
		"  -v  --verbose      Print verbose messages.\n");
  exit (exit_code);
}

/*
 * Main program:
 */
int main(int argc,char **argv) {
  int next_option;
	int fd;
	struct tx2_priv *priv = &priv_data;

	memcpy(&priv->tx2_cmd, &setup, sizeof(struct tx2_setup));
  do
    {
    next_option = getopt_long (argc, argv, short_options,
			long_options, NULL);
    switch (next_option) {
		case 'c':   /* -c or --command */
			priv->tx2_cmd.command = strtol(optarg, NULL, 0);
			switch (priv->tx2_cmd.command) {
			case 4: break;                  /* 0100 - End code */
			case 22: break;                  /* 0100 - Turbo */
			case 16: break;                  /* 0100 - Forward & Turbo */
			case 58: break;                  /* 0001 - Left */
			case 40: break;                  /* 0010 - Backward */
			case 52: break;                  /* 0011 - Backward & Left */
			case 10: break;                  /* 0100 - Forward */
			case 28: break;                  /* 0101 - Turbo & Forward & Left */
			case 64: break;                  /* 1000 - Right */
			case 46: break;                  /* 1010 - Backward & Right */
			case 34: break;                  /* 1100 - Turbo & Forward & Right */
			default:
				priv->tx2_cmd.command = 0;
				printf("Error: Wrong command\n");
				exit(1);
        break;
			}
			break;
		case 'd':   /* -d or --duration */
			priv->tx2_cmd.duration = strtol(optarg, NULL, 0);
			break;
		case 'v':   /* -v or --verbose */
			priv->verbose = 1;
			break;
		case '?':   /* The user specified an invalid option.  */
        /* Print usage information to standard error, and exit with exit
           code one (indicating abonormal termination).  */
			print_usage (stderr, 1);
			break;
		case -1:    /* Done with options.  */
			break;
		default:    /* Something else: unexpected.  */
			break;
			}
		}
  while (next_option != -1);

  if (priv->verbose)
    printf ("Wyze V2 - TX2 driver tester\n");

	fd = open(TX2_CMD_FILE, O_WRONLY | O_SYNC);  /* might need root access */
	if ( fd < 0 ) {
		perror(TX2_CMD_FILE);
		exit(1);
	}

	if (write(fd, &priv->tx2_cmd, sizeof(priv->tx2_cmd)) != sizeof(priv->tx2_cmd)) {
		perror(TX2_CMD_FILE);
		exit(1);
	}
	// fflush(fd);
	close(fd);
	return 0;
	}
