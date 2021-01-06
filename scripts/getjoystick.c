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
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* see ~/boards/wyze-v2/kernel/include/linux/mfd/jz_tcu.h */
struct tx2_setup {
	int command;
};

struct tx2_priv {
	char *address;
	char *port;
	struct tx2_setup tx2_cmd;
	int verbose;
	} priv_data =
	{
	.port = "8888",
	.address = "192.168.2.233",
	};

#define TX2_CMD_FILE "/sys/devices/platform/jz-tcu/cmd"

/* A string listing valid short options letters.  */
const char* program_name;  /* The name of this program.  */
const char* const short_options = "a:p:c:d:v";
  /* An array describing valid long options.  */
const struct option long_options[] = {
    { "IP address",      1, NULL, 'a' },
    { "port",      1, NULL, 'p' },
    { "command",      1, NULL, 'c' },
    { "duration",     1, NULL, 'd' },
    { "verbose",   0, NULL, 'v' },
    { NULL,        0, NULL, 0   }   /* Required at end of array.  */
  };

void print_usage (FILE* stream, int exit_code)
{
  fprintf (stream, "Usage:  %s options\n", program_name);
  fprintf (stream,
		"  -a  --address      IP address [192.168.2.233]\n"
		"  -p  --port         Port [8888]\n"
		"  -c  --command      Commands: 22, 16, 58, 40, 52, 10, 28, 64, 46, 34\n"
		"  -d  --duration     Duration in milliseconds to send\n"
		"  -v  --verbose      Print verbose messages.\n");
  exit (exit_code);
}

static char command_lut[] = {
	0,  /* 0x30 */
	58,  /* 0x31 */
	64,  /* 0x32 */
	0,  /* 0x33 */
	10,  /* 0x34 */
	28,  /* 0x35 */
	34,  /* 0x36 */
	0,  /* 0x37 */
	40,  /* 0x38 */
	52,  /* 0x39 */
	46,  /* 0x3a */
	0,  /* 0x3b */
	0,  /* 0x3c */
	0,  /* 0x3d */
	0,  /* 0x3e */
	0,  /* 0x3f */
	};
/*
 * Main program:
 */
int main(int argc,char **argv) {
	int rs, sock, read_size, pingCntr = 0;
	struct sockaddr_in addr;
	struct addrinfo *ai;
	struct addrinfo hints;
	char client_message[2];

  int next_option;
	int fd;
	struct tx2_priv *priv = &priv_data;

  do
    {
    next_option = getopt_long (argc, argv, short_options,
			long_options, NULL);
    switch (next_option) {
		case 'a':   /* -a or --address */
			priv->address = optarg;
			break;
		case 'p':   /* -p or --port */
			priv->port = optarg;
			break;
		case 'c':   /* -c or --command */
			priv->tx2_cmd.command = strtol(optarg, NULL, 0);
			switch (priv->tx2_cmd.command) {  /* time to send = 4 * 1/500 + command * 1/1000 */
			case 4: break;                  /* 0100 - End code */
			case 22: break;                  /* 0100 - Turbo */
			case 16: break;                  /* 0100 - Forward & Turbo */
			case 58: break;                  /* 0001 - Left */
			case 40: break;                  /* 0010 - Backward */
			case 52: break;                  /* 0011 - Backward & Left */
			case 10: break;                  /* 0100 - Forward (18ms) */
			case 28: break;                  /* 0101 - Turbo & Forward & Left */
			case 64: break;                  /* 1000 - Right (72ms) */
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
//			priv->tx2_cmd.duration = strtol(optarg, NULL, 0);
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

	if (priv->tx2_cmd.command) {  /* did we get a command to send? */

		fd = open(TX2_CMD_FILE, O_WRONLY | O_SYNC);  /* might need root access */
		if (fd < 0) {
			perror(TX2_CMD_FILE);
			exit(1);
			}
		
		if (write(fd, &priv->tx2_cmd, sizeof(priv->tx2_cmd)) != sizeof(priv->tx2_cmd)) {
			perror(TX2_CMD_FILE);
			exit(1);
			}
		close(fd);
		}
	else
		{
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;

    /* look up server host */
		while (1) {
			pingCntr = 0;
			if (priv->verbose)
				printf("starting connect loop\n");
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			if ((rs = getaddrinfo(priv->address, priv->port, &hints, &ai)) != 0) {
				printf("getaddrinfo() failed for %s: %s\n", priv->address,
					gai_strerror(rs));
				return 1;
				}
	
		/* create server socket */
			if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
				printf("socket() failed: %s\n", strerror(errno));
				return 1;
				}
//		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

			/* bind server socket */
#if 0
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
				printf("bind() failed: %s\n", strerror(errno));
				close(sock);
				return 1;
				}
#endif
				//Listen
#if 0
			if ((rs = listen(sock, 3)) != 0) {
				printf("listen() failed, returned %d\n", rs);
				perror("errno %d\n");
				return 1;
				}
			if (priv->verbose)
				printf("listen success\n");
#endif
			/* connect */
			if (connect(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
				printf("connect() failed: %s\n", strerror(errno));
				sleep(1);
				}
			else
				{
				if (priv->verbose)
					printf("connect success\n");
				
				fcntl(sock, F_SETFL, O_NONBLOCK);  /* make socket non blocking */
				//Receive a message from client, he sends every 10 ms (when any button is pushed)
				while (1) {
					read_size = recv(sock, client_message, sizeof(client_message), 0);
					if (priv->verbose)
						printf("got recv, errno %d returned %d, got 0x%x\n", errno, read_size, client_message[0]);
					if (read_size > 0) {
						// data has been received
						pingCntr = 0;
						priv->tx2_cmd.command = command_lut[client_message[0] & 0xf];
		
						if (priv->tx2_cmd.command) {
							fd = open(TX2_CMD_FILE, O_WRONLY | O_SYNC);  /* might need root access */
							if (fd < 0) {
								perror(TX2_CMD_FILE);
								exit(1);
								}
							/* only send a command every 250ms */
							if (write(fd, &priv->tx2_cmd, sizeof(priv->tx2_cmd)) != sizeof(priv->tx2_cmd)) {
								perror(TX2_CMD_FILE);
								exit(1);
								}
							close(fd);

							}
						}
					else {
						if (read_size == 0) {
							close(sock);
							break; // socket has been closed or shutdown for send
							}
						else {
							// there is an error, let's see what it is
							switch (errno)
								{
								case EAGAIN:  // also EWOULDBLOCK: 
									// Socket is O_NONBLOCK and there is no data available
									break;
								case EINTR: 
									// an interrupt (signal) has been catched
									// should be ingore in most cases
									break;
								default:
									// socket has an error, no valid anymore
									close(sock);
									break; // socket has been closed or shutdown for send
								}
							}
						}
					usleep(10000);  // debug only
					if (pingCntr++ > 600) {  /* 6 sec's */
						close(sock);
						break;
						}
					}
				}
			}
		}
	// fflush(fd);
		return 0;
	}
