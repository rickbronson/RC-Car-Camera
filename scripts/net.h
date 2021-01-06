/* all of our network needs */

/* * maximum packet size =  1518
 * maximum packet size and multiple of 32 bytes =  1536
 */

static int net(struct sockaddr_in *my_addr, int port);
int StartNetwork(struct sockaddr_in *my_addr, int port);
int StopNetwork(int fdListener, int fdConnection);

union val
	{
	int               i_val;
	long              l_val;
	char              c_cal[10];
	struct timeval    timeval_val;
//	long              timeval_val1;
//	long              timeval_val2;
	} val;


#define OUR_PORT 8888

struct _CONNECTINFO_ { 
   int id; 
   int sockfd; 
   int hCmdThread; 
   int dataActive; 
   char *cmd; 
}; 

static int net(struct sockaddr_in *my_addr, int port)
  {
#define TRUE 1

  int fdListener = 0;
  socklen_t len = sizeof(val);

  printf("Net: Starting network services ...\n");

 // Define our broadcast port
  fdListener = socket(AF_INET, SOCK_DGRAM, 0);

  if ( fdListener == -1 )
    {
    printf("Net: ERR Call to socket\n");
    return(0);
    }

  // set to reuse socket
  getsockopt( fdListener, SOL_SOCKET, SO_REUSEADDR, &val, &len );
  printf("REUSEADDR was %d\n", val.i_val );
  val.i_val = 1;
  len=sizeof(val);
  setsockopt( fdListener, SOL_SOCKET, SO_REUSEADDR, &val, len );
  printf("REUSEADDR is  %d\n", val.i_val );

 // set to reuse socket
  getsockopt( fdListener, SOL_SOCKET, SO_KEEPALIVE, &val, &len );
  printf("SO_KEEPALIVE was %d\n", val.i_val );
  val.i_val = 1;
  len=sizeof(val);
  setsockopt( fdListener, SOL_SOCKET, SO_KEEPALIVE, &val, len );
  printf("SO_KEEPALIVE is  %d\n", val.i_val );

 // set send timeout
  getsockopt( fdListener, SOL_SOCKET, SO_SNDTIMEO, &val, &len );
  printf("SO_SNDTIMEO was timeval.tv_sec=%ld, timeval.tv_usec=%ld\n", 
         val.timeval_val.tv_sec, val.timeval_val.tv_usec );
  val.timeval_val.tv_sec = 1; // two second timeout
  val.timeval_val.tv_usec = 0; // no microsecs
  len=sizeof(val);
  setsockopt( fdListener, SOL_SOCKET, SO_SNDTIMEO, &val, len );
  printf("SO_SNDTIMEO is timeval_val.tv_sec=%ld, timeval_val.tv_usec=%ld\n", 
         val.timeval_val.tv_sec, val.timeval_val.tv_usec );

  memset(my_addr, 0, sizeof (*my_addr)); // zero the struct 
  my_addr->sin_family = AF_INET;
  my_addr->sin_addr.s_addr = htonl(INADDR_ANY);
  my_addr->sin_port = htons(port);
  // and bind to our broadcast port
  if ( bind(fdListener, (struct sockaddr *)my_addr,
            sizeof(*my_addr)) == -1 )
    {
    printf("Net: ERR Call to bind fd = %d, addr = 0x%x\n", fdListener, my_addr->sin_addr.s_addr);
    perror("bind:");
    return(0);
    }

  printf("Net: Server accepting connections on Port %d ...\n", port);

  return(fdListener);
  }
