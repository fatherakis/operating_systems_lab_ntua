/*
 * socket-client.c
 * Simple TCP/IP communication using sockets
 *
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "common.h"

#include <sys/ioctl.h>
#include <crypto/cryptodev.h>
#define DATA_SIZE 512
#define BLOCK_SIZE 16
#define KEY_SIZE 16 /*AES 128*/




#include <sys/select.h>
#include <fcntl.h>
#define PEER_PREFIX "server: "
#define VERIF "Sent.\n"

#define RESETREADS_IN_SD {\
        FD_ZERO(&set_read);\
        FD_SET(sd, &set_read);\
        FD_SET(fileno(stdin), &set_read);\
}

unsigned char key[] = "0123456789abcdef";
unsigned char invect[] = "0123456789abcdef";
struct session_op sess;
int dec_buf_len = 1; // decrypted msg buf len

// HANDLE CTRL+C
/* Insist until all of the data has been written */
ssize_t insist_write(int fd, const void *buf, size_t cnt) {
	ssize_t ret;
	size_t orig_cnt = cnt;
	
	while (cnt > 0) {
	        ret = write(fd, buf, cnt);
	        if (ret < 0)
	                return ret;
	        buf += ret;
	        cnt -= ret;
	}

	return orig_cnt;
}

ssize_t n;
unsigned char buf[512];


volatile int conn_active = 0;
void clear_conn_active() {
	conn_active = 0;	
}

int encrypt(int cfd){
	struct crypt_op cryp;
	struct {
		unsigned char in[DATA_SIZE],		//input for enctyption
			      encrypted[DATA_SIZE],	//output
			      iv[BLOCK_SIZE];		//initializing vector
	} data;
	memset(&cryp,0,sizeof(cryp)); // allocation
	/* data.in to data.encrypted */
	cryp.ses = sess.ses; //(sess initialized in main)
	cryp.len = sizeof(data.in);
	cryp.src = buf; //not data.in for ease of use (buf defined globally)
	cryp.dst = data.encrypted;
	cryp.iv = invect;
	cryp.op = COP_ENCRYPT; //ENCRYPTION

	if (ioctl(cfd,CIOCCRYPT, &cryp))				{ perror("ioctl(CIOCRYPT)"); return 1; }

	int i = 0;
	while(i < DATA_SIZE){
		buf[i] = data.encrypted[i];
		i++;
	}
	return 0;
}

int decrypt(int cfd){ // same as of encrypt (few exceptions)
	struct crypt_op cryp;
	struct {
		unsigned char in[DATA_SIZE],
			      decrypted[DATA_SIZE],
			      iv[BLOCK_SIZE];
	}data;

	memset(&cryp,0,sizeof(cryp));
	/* data.in to data.decrypted */
	cryp.ses = sess.ses;
	cryp.len = sizeof(buf);
	cryp.src = buf;
	cryp.dst = data.decrypted;
	cryp.iv = invect;
	cryp.op = COP_DECRYPT; // DECRYPT
	if(ioctl(cfd, CIOCCRYPT, &cryp))				{ perror("iocl(CIOCCRYPT)"); return 1;}
	memset(buf, '\r',512); //
	int i;
	i = 0;
	while(data.decrypted[i] != '\0'){
		buf[i] = data.decrypted[i];
		i++;
	}
	buf[i]='\r'; //
	dec_buf_len=i;
	return 0;
}

void read_decrypt_write(int rfd, int cfd, int wfd) {
        while(1) { 
                n = read(rfd, buf, sizeof(buf)); //printf("%lu\n", n);
		if (decrypt(cfd))					{ perror("decrypt"); exit(1); }
		if (insist_write(wfd, buf, sizeof(buf)) != sizeof(buf))	{ perror("write"); exit(1); }
		if (n==0) { fprintf(stderr, "\nserver closed the connection"); raise(SIGINT); break; }
		// if connection is closed, select and read are executed as fast as possible
		if (buf[dec_buf_len] == '\r') break; // for safety
	}
}
void read_encrypt_write(int rfd, int cfd, int wfd) {
	while(1) { 
		int flag;
		flag = 0;
                n = read(rfd, buf, sizeof(buf)); //printf("%lu\n", n);
		if (buf[n-1] == '\n')  flag=1;
		if (encrypt(cfd))					{ perror("encrypt"); exit(1); }
                if (insist_write(wfd, buf, sizeof(buf)) != sizeof(buf))	{ perror("write"); exit(1); }
		if (n == 0) {fprintf(stderr, "\nserver closed the connection"); raise(SIGINT); break;}
		if (flag) {  break; }
		// if connection is closed, select and read are executed as fast as possible
	}
}


int main(int argc, char *argv[]) {
	int sd, port, cfd;
	//ssize_t n;
	//char buf[100];
	char *hostname, *crypto_fname;
	struct hostent *hp;
	struct sockaddr_in sa;

	if (argc < 3) { fprintf(stderr, "Usage: %s hostname port\n", argv[0]); exit(1); }
	hostname = argv[1];
	port = atoi(argv[2]); /* Needs better error checking */
	crypto_fname = (argv[3] == NULL) ? "/dev/crypto" : argv[3];

	/* Create TCP/IP socket, used as main chat channel */
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)			{ perror("socket"); exit(1); }
	fprintf(stderr, "Created TCP socket\n");
	
	/* Look up remote hostname on DNS */
	if ( !(hp = gethostbyname(hostname)))				{ printf("DNS lookup failed for host %s\n", hostname); exit(1); }

	/* Connect to remote TCP port */
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
	fprintf(stderr, "Connecting to remote host... ");
	fflush(stderr);
	if (connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0)	{ perror("connect"); exit(1); }
	fprintf(stderr, "Connected.\n");
	
	/* Encryption init */
	cfd = open(crypto_fname, O_RDWR);
	if (cfd < 0)							{ perror("open(crypto_fname)"); exit(1);}
	memset(&sess,0,sizeof(sess));
	sess.cipher = CRYPTO_AES_CBC;
	sess.keylen = KEY_SIZE;
	sess.key = key; // encryption key (globally defined)
	if(ioctl(cfd, CIOCGSESSION, &sess)) 				{ perror("ioctl(CIOCGSESSION)"); exit(1);}

	/* select init */
	fd_set set_read;
	RESETREADS_IN_SD;

	conn_active = 1;
	signal(SIGINT, clear_conn_active);
	while(conn_active) {
		if(select(sd+1, &set_read, NULL, NULL, NULL) < 0)	{ if(errno==EINTR) break; perror("select"); exit(1); }
		//printf("PASSSSSSED SELECT!!\n");
		if(FD_ISSET(sd, &set_read)) {
			insist_write(fileno(stdout), PEER_PREFIX, strlen(PEER_PREFIX));	
			read_decrypt_write(sd, cfd, fileno(stdout));
		}
		if(FD_ISSET(fileno(stdin), &set_read)) { // /!\: Be careful with buffer overruns, ensure NUL-termination
			memset(buf,'\0',512);
			read_encrypt_write(fileno(stdin), cfd, sd);
			insist_write(0,VERIF,strlen(VERIF)); // "Sent." message
		}
		RESETREADS_IN_SD;
	}
	//fprintf(stdout, "I said:\n%s\nRemote says:\n", buf);
	//fflush(stdout);

	/*
	 * Let the remote know we're not going to write anything else.
	 * Try removing the shutdown() call and see what happens.
	 */
	if (ioctl(cfd, CIOCFSESSION, &sess.ses)) 			{ perror("ioctl(CIOCFSESSION)"); exit(1);}
	if (close(cfd) < 0) 						{ perror("close(cfd)"); exit(1);}
	fprintf(stderr, "\nSending shutdown to server.\n");
	if (shutdown(sd, SHUT_WR) < 0)					{ perror("shutdown"); exit(1); }
	//fprintf(stderr, "\nDone.\n");
	return 0;
}
