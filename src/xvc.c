
// Ref: https://github.com/technion/lol_dht22/blob/master/dht22.c
/* This work, "xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd) 
 * by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/). 
 * "xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/) 
 * by Avnet and is used by Xilinx for XAPP1251.
 *
 *  Description : XAPP1251 Xilinx Virtual Cable Server for Linux
 */

#define BANNER_MESS "Xilnx Virtual Cable for milkV-duo by Vlad Vikulin, Ver 1.0 (C)2024\n"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <wiringx.h>

#include <sys/mman.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h> 
#include <pthread.h>

static int verbose = 0;

static int TCKPIN = 21;
static int TDOPIN = 20;
static int TDIPIN = 19;
static int TMSPIN = 18;

extern int set_pinfunc(char *pin, char *pinfunc);


static uint8_t sizecvt(const int read)
{
    /* digitalRead() and friends from wiringpi are defined as returning a value
    < 256. However, they are returned as int() types. This is a safety function */

    if (read > 255 || read < 0)
    {
        printf("Invalid data from wiringPi library\n");
        exit(EXIT_FAILURE);
    }
    return (uint8_t)read;
}


/*
    digitalWrite(DHTPIN, HIGH);
    delayMicroseconds(500000);
    digitalWrite(DHTPIN, LOW);
    v = sizecvt(digitalRead(DHTPIN));

*/

/* Transition delay coefficients */
static unsigned int jtag_delay = 50;

static int gpio_read(void) { return sizecvt(digitalRead(TDOPIN)); }

static void gpio_write(int tck, int tms, int tdi) {
  digitalWrite(TCKPIN, tck);
  digitalWrite(TMSPIN, tms);
  digitalWrite(TDIPIN, tdi);


  //for (unsigned int i = 0; i < jtag_delay; i++) ;
    //asm volatile("");
}

static uint32_t gpio_xfer(int n, uint32_t tms, uint32_t tdi) {
  uint32_t tdo = 0;

  for (int i = 0; i < n; i++) {
    gpio_write(0, tms & 1, tdi & 1);
    tdo |= gpio_read() << i;
    gpio_write(1, tms & 1, tdi & 1);
    tms >>= 1;
    tdi >>= 1;
  }
  return tdo;
}

static int sread(int fd, void *target, int len) {
   unsigned char *t = target;
   while (len) {
      int r = read(fd, t, len);
      if (r <= 0)
         return r;
      t += r;
      len -= r;
   }
   return 1;
}

//----------------
int handle_data(int fd) {
  const char xvcInfo[] = "xvcServer_v1.0:2048\n";

  do {
    char cmd[16];
    unsigned char buffer[2048], result[1024];
    memset(cmd, 0, 16);

    if (sread(fd, cmd, 2) != 1)
      return 1;

    if (memcmp(cmd, "ge", 2) == 0) {
      if (sread(fd, cmd, 6) != 1)
        return 1;
      memcpy(result, xvcInfo, strlen(xvcInfo));
      if (write(fd, result, strlen(xvcInfo)) != strlen(xvcInfo)) {
        perror("write");
        return 1;
      }
      if (verbose) {
        printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
        printf("\t Replied with %s\n", xvcInfo);
      }
      break;
    } else if (memcmp(cmd, "se", 2) == 0) {
      if (sread(fd, cmd, 9) != 1)
        return 1;
      memcpy(result, cmd + 5, 4);
      if (write(fd, result, 4) != 4) {
        perror("write");
        return 1;
      }
      if (verbose) {
        printf("%u : Received command: 'settck'\n", (int)time(NULL));
        printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);
      }
      break;
    } else if (memcmp(cmd, "sh", 2) == 0) {
      if (sread(fd, cmd, 4) != 1)
        return 1;
      if (verbose) {
        printf("%u : Received command: 'shift'\n", (int)time(NULL));
      }
    } else {
      fprintf(stderr, "invalid cmd '%s'\n", cmd);
      return 1;
    }

    int len;
    if (sread(fd, &len, 4) != 1) {
      fprintf(stderr, "reading length failed\n");
      return 1;
    }

    int nr_bytes = (len + 7) / 8;
    if (nr_bytes * 2 > sizeof(buffer)) {
      fprintf(stderr, "buffer size exceeded\n");
      return 1;
    }

    if (sread(fd, buffer, nr_bytes * 2) != 1) {
      fprintf(stderr, "reading data failed\n");
      return 1;
    }
    memset(result, 0, nr_bytes);

    if (verbose) {
      printf("\tNumber of Bits  : %d\n", len);
      printf("\tNumber of Bytes : %d \n", nr_bytes);
      printf("\n");
    }

    gpio_write(0, 1, 1);

    int bytesLeft = nr_bytes;
    int bitsLeft = len;
    int byteIndex = 0;
    uint32_t tdi, tms, tdo;

    while (bytesLeft > 0) {
      tms = 0;
      tdi = 0;
      tdo = 0;
      if (bytesLeft >= 4) {
        memcpy(&tms, &buffer[byteIndex], 4);
        memcpy(&tdi, &buffer[byteIndex + nr_bytes], 4);
        tdo = gpio_xfer(32, tms, tdi);
        memcpy(&result[byteIndex], &tdo, 4);
        bytesLeft -= 4;
        bitsLeft -= 32;
        byteIndex += 4;
        if (verbose) {
          printf("LEN : 0x%08x\n", 32);
          printf("TMS : 0x%08x\n", tms);
          printf("TDI : 0x%08x\n", tdi);
          printf("TDO : 0x%08x\n", tdo);
        }
      } else {
        memcpy(&tms, &buffer[byteIndex], bytesLeft);
        memcpy(&tdi, &buffer[byteIndex + nr_bytes], bytesLeft);
        tdo = gpio_xfer(bitsLeft, tms, tdi);
        memcpy(&result[byteIndex], &tdo, bytesLeft);
        bytesLeft = 0;
        if (verbose) {
          printf("LEN : 0x%08x\n", bitsLeft);
          printf("TMS : 0x%08x\n", tms);
          printf("TDI : 0x%08x\n", tdi);
          printf("TDO : 0x%08x\n", tdo);
        }
        break;
      }
    }

    gpio_write(0, 1, 0);

    if (write(fd, result, nr_bytes) != nr_bytes) {
      perror("write");
      return 1;
    }

  } while (1);
  /* Note: Need to fix JTAG state updates, until then no exit is allowed */
  return 0;
}

//--------------

int setpins()
{
    if (wiringXSetup("duo", NULL) == -1)
    {
        wiringXGC();
        return -1;
    }

    if ( (set_pinfunc("GP18", "GP18") >=0) &&
	 (set_pinfunc("GP19", "GP19") >=0) &&
	 (set_pinfunc("GP20", "GP20") >=0) &&
	 (set_pinfunc("GP21", "GP21") >=0) ){
	;
    } else {
	return -1;
    }

    if ( (wiringXValidGPIO(TDIPIN) != 0) ||
    	 (wiringXValidGPIO(TDOPIN) != 0) ||
    	 (wiringXValidGPIO(TMSPIN) != 0) ||
    	 (wiringXValidGPIO(TCKPIN) != 0)   )
    {
        return -1;
    }

    pinMode(TDOPIN, PINMODE_INPUT);
    pinMode(TDIPIN, PINMODE_OUTPUT);
    pinMode(TCKPIN, PINMODE_OUTPUT);
    pinMode(TMSPIN, PINMODE_OUTPUT);

    digitalWrite(TDIPIN, LOW);
    digitalWrite(TCKPIN, LOW);
    digitalWrite(TMSPIN, HIGH);

    return 0;
}

//--------------------------------------------------------



int main(int argc, char **argv) {
   int i;
   int s;
   int c;
   int port = 2542;
   
   struct sockaddr_in address;
   
   
   printf( BANNER_MESS );
   
   if (setpins()<0) {
        printf("GPIO initialization failure\n");
   	return -1;
   }

   opterr = 0;

   while ((c = getopt(argc, argv, "vd:p:")) != -1)
      switch (c) {
      case 'v':
         verbose = 1;
         break;
	  case 'p':
		 port = atoi(optarg);
		 break;
      case '?':
//         fprintf(stderr, "usage: %s [-v] [-p <port>]\n example: %s -v -p 2542 \n", *argv ,*argv);
         printf("usage: %s [-v] [-p <port>]\n example: %s -v -p 2542 \n", *argv ,*argv);
         return 1;
      }

   s = socket(AF_INET, SOCK_STREAM, 0);
               
   if (s < 0) {
      perror("socket");
      return 1;
   }


   
   i = 1;
   setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(port);
   address.sin_family = AF_INET;




   if (bind(s, (struct sockaddr*) &address, sizeof(address)) < 0) {
      perror("bind");
      return 1;
   }

   if (listen(s, 1) < 0) {
      perror("listen");
      return 1;
   }

   fd_set conn;
   int maxfd = 0;

   FD_ZERO(&conn);
   FD_SET(s, &conn);
   maxfd = s;

   printf("Listen Port %d\n", port);


   while (1) {
      fd_set read = conn, except = conn;
      int fd;

      if (select(maxfd + 1, &read, 0, &except, 0) < 0) {
         perror("select");
         break;
      }

      for (fd = 0; fd <= maxfd; ++fd) {
         if (FD_ISSET(fd, &read)) {
            if (fd == s) {
               int newfd;
               socklen_t nsize = sizeof(address);

               newfd = accept(s, (struct sockaddr*) &address, &nsize);

//               if (verbose)
                  printf("connection accepted - fd %d\n", newfd);
               if (newfd < 0) {
                  perror("accept");
               } else {
            	   printf("setting TCP_NODELAY to 1\n");
            	  int flag = 1;
            	  int optResult = setsockopt(newfd,
            			  	     IPPROTO_TCP,
            			  	     TCP_NODELAY,
            			  	     (char *)&flag,
            			  	     sizeof(int));
            	  if (optResult < 0)
            		  perror("TCP_NODELAY error");
                  if (newfd > maxfd) {
                     maxfd = newfd;
                  }
                  FD_SET(newfd, &conn);
               }
            }
            else if (handle_data(fd)) {

               if (verbose)
                  printf("connection closed - fd %d\n", fd);
               close(fd);
               FD_CLR(fd, &conn);
            }
         }
         else if (FD_ISSET(fd, &except)) {
            if (verbose)
               printf("connection aborted - fd %d\n", fd);
            close(fd);
            FD_CLR(fd, &conn);
            if (fd == s)
               break;
         }
      }
   }
   return 0;
}


