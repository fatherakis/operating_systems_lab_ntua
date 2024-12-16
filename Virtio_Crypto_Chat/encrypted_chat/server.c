/*
 * socket-server.c	CRYPTODEV Implementation
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


#include <sys/ioctl.h>
#include <crypto/cryptodev.h>
#define DATA_SIZE 512
#define BLOCK_SIZE 16
#define KEY_SIZE 16 /* AES 128 */

#include "common.h"

#include <fcntl.h>

#define VERIF "Sent.\n"
#define PEER_PREFIX "client: "

#define RESETREADS_IN_NEWSD {\
        FD_ZERO(&set_read);\
        FD_SET(newsd, &set_read);\
        FD_SET(fileno(stdin), &set_read);\
}

unsigned char key[] = "0123456789abcdef";
unsigned char invect[] = "0123456789abcdef";
struct session_op sess;
int dec_buf_len = 1;

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

int sd = -1;
volatile int conn_active = 0;
void clear_conn_active() {
	if (!conn_active) { if (close(sd) < 0) perror("close"); fprintf(stderr, "\n"); exit(0); }
	else { conn_active = 0; }
}


int encrypt(int cfd){
	struct crypt_op cryp;
	struct {
		unsigned char in[DATA_SIZE],
			      encrypted[DATA_SIZE],
			      iv[BLOCK_SIZE];
	}data;
	memset(&cryp,0,sizeof(cryp));
	/* data.in to data.encrypted */
	cryp.ses = sess.ses;
	cryp.len = sizeof(data.in);
	cryp.src = buf;
	cryp.dst = data.encrypted;
	cryp.iv = invect;
	cryp.op = COP_ENCRYPT;

	if (ioctl(cfd,CIOCCRYPT, &cryp))				{ perror("ioctl(CIOCRYPT)"); return 1; }

	int i = 0;
	while(i<DATA_SIZE){
		buf[i] = data.encrypted[i];
		i++;
	}
	return 0;
}

int decrypt(int cfd){
	struct crypt_op cryp;
	struct {
		unsigned char in[DATA_SIZE],
			      decrypted[DATA_SIZE],
			      iv[BLOCK_SIZE];
	}data;
	memset(&cryp,0,sizeof(cryp));
	/* data.in to data.decrypted */
	cryp.ses = sess.ses;
	cryp.len = sizeof(data.in);
	cryp.src = buf;
	cryp.dst = data.decrypted;
	cryp.iv = invect;
	cryp.op = COP_DECRYPT;
	if(ioctl(cfd, CIOCCRYPT, &cryp))				{ perror("iocl(CIOCCRYPT)"); return 1;}
	memset(buf, '\r',512);
	int i;
	i = 0;
	while(data.decrypted[i] != '\0'){
		buf[i] = data.decrypted[i];
		i++;
	}
	buf[i]='\r';
	dec_buf_len = i;
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
	//char buf[100];
	char* crypto_fname;
	char addrstr[INET_ADDRSTRLEN];
	int /*sd,*/ newsd, cfd;
	//ssize_t n;
	socklen_t len;
	struct sockaddr_in sa;

	crypto_fname = (argv[1] == NULL) ? "/dev/crypto" : argv[1];

	/* Make sure a broken connection doesn't kill us */
	//signal(SIGPIPE, SIG_IGN);

	/* Create TCP/IP socket, used as main chat channel */
	if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)		{ perror("socket"); exit(1); }
	fprintf(stderr, "Created TCP socket\n");

	/* Bind to a well-known port */
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0)	{ perror("bind"); exit(1); }
	fprintf(stderr, "Bound TCP socket to port %d\n", TCP_PORT);

	/* Listen for incoming connections */
	if (listen(sd, TCP_BACKLOG) < 0)			{ perror("listen"); exit(1); }

	/* Loop forever, accept()ing connections */
	signal(SIGINT, clear_conn_active);
	
	
	while(1) {
		fprintf(stderr, "Waiting for an incoming connection...\n");

		/* Accept an incoming connection */
		len = sizeof(struct sockaddr_in);
		if ((newsd = accept(sd, (struct sockaddr *)&sa, &len)) < 0)		{ perror("accept"); exit(1); }
		if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr)))	{ perror("could not format IP address"); exit(1); }
		fprintf(stderr, "Incoming connection from %s:%d\n", addrstr, ntohs(sa.sin_port));

		cfd = open(crypto_fname, O_RDWR);
		if (cfd < 0)							{ perror("open(crypto_fname)"); exit(1);}	
		memset(&sess,0,sizeof(sess));
		sess.cipher = CRYPTO_AES_CBC;
		sess.keylen = KEY_SIZE;
		sess.key = key;
		if(ioctl(cfd, CIOCGSESSION, &sess)) 				{ perror("ioctl(CIOCGSESSION)"); exit(1);}


		/* We break out of the loop when the remote peer goes away */
        	fd_set set_read;
		RESETREADS_IN_NEWSD;
		conn_active = 1;
		while(conn_active) {
	                if(select(newsd+1, &set_read, NULL, NULL, NULL) < 0)	{ if(errno==EINTR) break; perror("select"); exit(1); }
			//printf("PAASSSSED SELECT!!!\n");
	                if(FD_ISSET(newsd, &set_read)) {
				insist_write(fileno(stdout), PEER_PREFIX, strlen(PEER_PREFIX));
				read_decrypt_write(newsd, cfd, fileno(stdout));
	                }
		        if(FD_ISSET(fileno(stdin), &set_read)) { //!\: Be careful with buffer overruns, ensure NUL-termination
				memset(buf,'\0',512);
				read_encrypt_write(fileno(stdin), cfd, newsd);
				insist_write(0,VERIF,strlen(VERIF));
	                }
			RESETREADS_IN_NEWSD;
		}
		/* Make sure we don't leak open files */
		if(ioctl(cfd, CIOCFSESSION, &sess.ses)) 			{ perror("ioctl(CIOCFSESSION)"); exit(1);}
		if(close(cfd) < 0)						{ perror("close(cfd)"); exit(1);}
		fprintf(stderr, "Closing file descriptor for socket.\n");
		if (close(newsd) < 0)				perror("close");
	}

	/* This will never happen */
	return 1;
}

