/*
 * This example was taken from https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpclient.c 
 * and modified to fit the need of the SDDS UDP speed test
 */

/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <math.h> 

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

struct SDDS_Packet {
  unsigned dmode:3;
  unsigned ss:1;
  unsigned of:1;
  unsigned pp:1;
  unsigned sos:1;
  unsigned sf:1;

  unsigned bps:5;
  unsigned vw:1;
  unsigned snp:1;
  unsigned cx:1;  // new complex bit

  // Frame sequence
  uint16_t seq;

  // Time Tag
  uint16_t msptr; // contains [msv, ttv, sscv] bits at the top bits

  uint16_t msdel;

  uint64_t  ttag;
  uint32_t  ttage;

  // Clock info
  int32_t  dFdT;
  uint64_t  freq;
  // Odds & Ends

  uint16_t ssd[2];
  uint8_t aad[20];
  //======================== SDDS data
  uint8_t d[1024];

};

int main(int argc, char **argv) {
    int sockfd, portno, n, run_time;
    size_t tot_sent = 0, start_time;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    float a_rate, d_rate;
    size_t pkts_per_sleep = 1;
    size_t max_pkts_per_sleep = 10000;
    size_t p_sent = 0;
    struct SDDS_Packet p = {0};
    p.sf = 1;
    p.ss = 1; // True for the first 2^16 packets
    p.bps = 8; // SourceNic 
    p.dmode = 1; // XXX: Why is this one?
    p.freq = htonl(60000000);
    uint16_t p_seq = 0;  // Internal sequence counter 
    uint64_t p_ttag = 0;

    /* check command line arguments */
    if (argc != 5) {
       fprintf(stderr,"usage: %s <hostname> <port> <target rate (Bps)> <run time (s)>\n", argv[0]);
       exit(0);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);
    d_rate = atof(argv[3]);
    run_time = atoi(argv[4]);

    start_time = (unsigned)time(NULL);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* send the message to the server */
    serverlen = sizeof(serveraddr);

    /* time start*/
    struct timeval tv;
    gettimeofday(&tv, NULL);

    unsigned long long t_milli_start =  (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;

    /* Start off assuming it takes no time to send the packet, well decrease the time until we are within 90% the expected speed*/
    float t_wait = 1000000*1080.0 / d_rate; 
    while (t_wait < 100 || pkts_per_sleep >= max_pkts_per_sleep) {
      pkts_per_sleep *= 2;
      t_wait *= 2;
    }

    
    while (1) {
      if (time(NULL) - start_time > run_time) {
        printf("%f\n",a_rate);
        break;
      }
      n = sendto(sockfd, &p, sizeof(p), 0, &serveraddr, serverlen);
      p_sent++;
      if (p_sent % pkts_per_sleep == 0) {
        usleep(t_wait);
      }

      if (n < 0) 
        error("ERROR in sendto");

      tot_sent += n;

      p_seq++;
      p_ttag++;
      p.ttag = htonl(p_ttag);

      if (p_seq == 65536)
        p.ss = 0;

      if (p_seq !=0 && ( p_seq % 32 == 31)) {
        p_seq++;
      }

      p.seq = htons(p_seq);

      gettimeofday(&tv, NULL);

      unsigned long long t_milli_end =  (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;

      // Update speeds every 1/10th of a second
      if (t_milli_end - t_milli_start > 500) {

        a_rate = 1000*tot_sent / (t_milli_end - t_milli_start);

        t_milli_start = t_milli_end;

        //printf("Rate = %f MB/s\n", a_rate/1000000.0);
        tot_sent = 0;

        float rate_diff = (a_rate - d_rate) / d_rate;
        //printf("abs Rate diff = %f\n", rate_diff);

        if (fabsf(rate_diff) > 0.05) {
          t_wait += t_wait*rate_diff;
          //printf("twait is = %.20f\n\n", t_wait);
          //printf("pkts_per_sleep is = %i\n\n", pkts_per_sleep);

          // usleep is not very accurate so we will just in crease packets per sleep if we are sleeping for less than 100 us
          // We are not keeping up and should send more packets per sleep
          if ((t_wait < 100) && (pkts_per_sleep < max_pkts_per_sleep)) {
            pkts_per_sleep *= 2;
            t_wait *= 1.5;
          }
        } 
      }
    }
    close(sockfd);
    return 0;
}
