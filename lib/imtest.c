/* imtest.c -- IMAP/IMSP test client
 *
 # Copyright 1998 Carnegie Mellon University
 # 
 # No warranties, either expressed or implied, are made regarding the
 # operation, use, or results of the software.
 #
 # Permission to use, copy, modify and distribute this software and its
 # documentation is hereby granted for non-commercial purposes only
 # provided that this copyright notice appears in all copies and in
 # supporting documentation.
 #
 # Permission is also granted to Internet Service Providers and others
 # entities to use the software for internal purposes.
 #
 # The distribution, modification or sale of a product which uses or is
 # based on the software, in whole or in part, for commercial purposes or
 # benefits requires specific, additional permission from:
 #
 #  Office of Technology Transfer
 #  Carnegie Mellon University
 #  5000 Forbes Avenue
 #  Pittsburgh, PA  15213-3890
 #  (412) 268-4387, fax: (412) 268-7395
 #  tech-transfer@andrew.cmu.edu
 *
 * Author: Chris Newman <chrisn+@cmu.edu>
 * Start Date: 2/16/93
 */

/* kludge: send a non-synchronizing literal with password instead of
   unquoted password; should not do this, as it is not compatible with
   base-line IMAP4rev1 servers.
   */
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>

#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "sasl.h"
#include "saslutil.h"
#include "saslplug.h"
#include "yo_file.h"

#define TEST_USERID "tmartin"
#define AUTH "KERBEROS"
#define TESTSTRING "a001 lfdist . blah\r\n"

/* from OS: */
extern char *getpass();
extern struct hostent *gethostbyname();

/* constant commands */
char logout[] = ". LOGOUT\r\n";

/* authstate which must be cleared before exit */
static void *authstate;



extern struct sasl_client krb_sasl_client;
#define client_start krb_sasl_client.start
#define client_auth  krb_sasl_client.auth
#define client_query krb_sasl_client.query_state
#define client_free  krb_sasl_client.free_state

/* base64 tables
 */
static char basis_64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static char index_64[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};
#define CHAR64(c)  (((c) < 0 || (c) > 127) ? -1 : index_64[(c)])

void to64(out, in, inlen)
    unsigned char *out, *in;
    int inlen;
{
    unsigned char oval;
    
    while (inlen >= 3) {
	*out++ = basis_64[in[0] >> 2];
	*out++ = basis_64[((in[0] << 4) & 0x30) | (in[1] >> 4)];
	*out++ = basis_64[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
	*out++ = basis_64[in[2] & 0x3f];
	in += 3;
	inlen -= 3;
    }
    if (inlen > 0) {
	*out++ = basis_64[in[0] >> 2];
	oval = (in[0] << 4) & 0x30;
	if (inlen > 1) oval |= in[1] >> 4;
	*out++ = basis_64[oval];
	*out++ = (inlen < 2) ? '=' : basis_64[(in[1] << 2) & 0x3c];
	*out++ = '=';
    }
    *out++ = '\r';
    *out++ = '\n';
    *out = '\0';
}

int from64(out, in)
    char *out, *in;
{
    int len = 0;
    int c1, c2, c3, c4;

    if (in[0] == '+' && in[1] == ' ') in += 2;
    if (*in == '\r') return (0);
    do {
	c1 = in[0];
	if (CHAR64(c1) == -1) return (-1);
	c2 = in[1];
	if (CHAR64(c2) == -1) return (-1);
	c3 = in[2];
	if (c3 != '=' && CHAR64(c3) == -1) return (-1); 
	c4 = in[3];
	if (c4 != '=' && CHAR64(c4) == -1) return (-1);
	in += 4;
	*out++ = (CHAR64(c1) << 2) | (CHAR64(c2) >> 4);
	++len;
	if (c3 != '=') {
	    *out++ = ((CHAR64(c2) << 4) & 0xf0) | (CHAR64(c3) >> 2);
	    ++len;
	    if (c4 != '=') {
		*out++ = ((CHAR64(c3) << 6) & 0xc0) | CHAR64(c4);
		++len;
	    }
	}
    } while (*in != '\r' && c4 != '=');

    *out=0;

    return (len);
}


void checkerror(int result)
{
  const char *errstr;

  if ((result==SASL_OK) || (result==SASL_CONTINUE))
    return;
  

  printf("error: %i\n",result);
  exit(1);
}

void fatal(str, level)
    char *str;
    int level;
{
    if (str) fprintf(stderr, "%s\n", str);
    exit(1);
}

  FILE *fp;
char blah2[100];
int sock;
char blah[100];

char *read_string()
{
  char *ret;
  char *out;
  int pos=0;
  char c;
  int len;

  out=(char *) malloc(200);
  ret=(char *) malloc(200);

  while ((c= fgetc(fp)) !=EOF)
    {
      if (c=='\n')
	break;

      ret[pos]=c;
      pos++;

    }
  ret[pos]=0;

  printf("S: %s\n", ret);

  /* should remove ``+ '' and decode64 it */
  if ((ret[0]!='+') || (ret[1]!=' '))
      return ret;

  /* started with "+ " (we should decode64 it */

  ret+=2; /* ignore "+ " */

  len=from64(out ,ret);

  return out;
}

void decode_string(sasl_conn_t *conn)
{
  char *ret;
  char *out;
  int outlen;
  int pos=0;
  int c;
  int len;
  int result;
  int length;
  fd_set rset;
  struct timeval tv;
  
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  FD_ZERO(&rset);
  FD_SET(0,&rset);
  FD_SET( sock, &rset);

  while (select(100, &rset, NULL, NULL, &tv) >0 )
  {
  
  fcntl(sock, F_SETFL, O_NONBLOCK);

  ret=(char *) malloc(5010);
  len=recv(sock, ret, 5000, 0);

  result=sasl_decode(conn,ret, len, &out,(unsigned int *)&outlen);

  free(ret);

  checkerror(result);
  if (out!=NULL)
  {
    out[outlen]=0;
    printf("S: %s", out);
    free(out);
  }

  }


}

void interaction (sasl_interact_t *t)
{
  char result[1024];

  printf("%s:",t->prompt);
  scanf("%s",&result);

  t->len=strlen(result);
  t->result=(char *) malloc(t->len);
  memcpy((char *) t->result, result, t->len);

}

void usage()
{
    fprintf(stderr, "usage: imtest [-k[p/i] / -p] <server> <username> <port>\n");
    exit(1);
}

int sasl_setpeer(void)
{
  int ipaddr;
  struct sockaddr_in *name=malloc(sizeof(struct sockaddr_in));
  int namelen;
  unsigned char ip[4];

  /* set the client */
  if (getpeername(sock,(struct sockaddr *) name,&namelen)!=0)
  {
    printf("ERROR!!! %i\n",sock);
    return 0;
  }

  memcpy(ip,&(name->sin_addr), 4);

    printf("ip = %i %i %i %i\n",
	 ip[0],
	 ip[1],
	 ip[2],
	 ip[3]);

  ipaddr=
    ((unsigned char) ip[0])*256*256*256 +
    ((unsigned char) ip[1])*256*256 +
    ((unsigned char) ip[2])*256 +
    ((unsigned char) ip[3]);

  

  ipaddr=htonl(ipaddr);


  /* if client give server port if server give server */


  /* set the server */
  if (getsockname(sock,(struct sockaddr *) name,&namelen)!=0)
    return 0;

  memcpy(ip,&(name->sin_addr), 4);

  ipaddr=
    ((unsigned char) ip[0])*256*256*256 +
    ((unsigned char) ip[1])*256*256 +
    ((unsigned char) ip[2])*256 +
    ((unsigned char) ip[3]);

  ipaddr=htonl(ipaddr);


  /* set the port */

  /* if client give server port if server give server */


 return SASL_OK;
}

static sasl_security_properties_t *make_secprops(int min,int max)
{
  sasl_security_properties_t *ret=(sasl_security_properties_t *)
    malloc(sizeof(sasl_security_properties_t));

  ret->maxbufsize=1024;
  ret->min_ssf=min;
  ret->max_ssf=max;

  ret->security_flags=0;
  ret->property_names=NULL;
  ret->property_values=NULL;

  return ret;
}

main(argc, argv)
    int argc;
    char **argv;
{

  int ssf=1;
  int result, port,lup;
  char c;
  sasl_conn_t *conn;
  sasl_interact_t *client_interact=NULL;
  sasl_secret_t *secret;
  char *out, *str;
  const char *mechusing;
  int outlen;
  char sen[1024];
  YO_FILE *yofile;
  char hostname[1024];
  int ipaddr;
  char *username;
  char tmp[4];
  
  struct sockaddr_in *saddr_l=malloc(sizeof(struct sockaddr_in));
  struct sockaddr_in *saddr_r=malloc(sizeof(struct sockaddr_in));
  struct sockaddr_in sad;

    int nfds, nfound, count, dologin, dopass;
    int len, done=0, maxplain;
    int prot_req, protlevel;
    
    char *host, *pass, *outbuf, *user, *portstr;
    fd_set read_set, rset;
    struct sockaddr_in addr, laddr;
    struct hostent *hp;
    struct servent *serv;
    struct protstream *pout, *pin;
    char buf[4096];
    char *in;
    sasl_security_properties_t *secprops=NULL;
    char ipstr[4],ip[4];

    malloc(1000);

    if (argc < 3) usage();

    host = argv[1];
    username = argv[2];
    portstr = argv[3];

    if (*argv[1] == '-') 
    {
      if (argv[1][1] == 'p') 
	secprops=make_secprops(0,0);
      else if (argv[1][1] == 'k') {
	    if (argv[1][2] == 'p') {
	      secprops=make_secprops(0,56);
	    } else if (argv[1][2] == 'i') {
	      secprops=make_secprops(0,1);
	    }
	}

	else usage();
	host = argv[2];
	username = argv[3];
	portstr = argv[4];
    }
    if (!portstr) usage();

    port=atoi(portstr);
    malloc(1000);

    result=sasl_client_init(NULL,NULL);

    malloc(1000);
    if ((hp = gethostbyname(host)) == NULL) {
	perror("gethostbyname");
	exit(1);
    }
    malloc(1000);
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }
    malloc(1000);

    printf("sock=%i\n",sock);
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
    addr.sin_port = htons(143);
    malloc(1000);
    printf("%i\n",addr.sin_addr);

    if (connect(sock, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
	perror("connect");
	exit(1);
    }

  

    yofile=YO_fdopen(sock,"r");
    fp=fdopen(sock, "r");




    in=read_string();    
    free(in);

    checkerror(result);


 
    /* client new connection */
    result=sasl_client_new("imap",
			   host,
			   NULL,
			   0,
			   &conn);
  checkerror(result);

   ssf=0;
   sasl_setprop(conn,   SASL_SSF_EXTERNAL, &ssf);  




/* create secret */
  secret=(sasl_secret_t *) malloc(sizeof(sasl_secret_t)+9);
  strcpy(secret->data,"password");



  secret->len=strlen(secret->data);

  printf("username=[%s]\n",username);
  sasl_setprop(conn, SASL_USERNAME, username);

  port=143;

  if (secprops!=NULL)
    sasl_setprop(conn, SASL_SEC_PROPS, secprops);

  /* */



  printf("sock=%i\n",sock);
  memcpy(&sad.sin_addr,"aaaa",4);


  printf("%i\n",sad.sin_addr);
  printf("port=%i\n",sad.sin_port);



    result=sizeof(struct sockaddr_in);
    if (getpeername(sock,(struct sockaddr *)saddr_r,&result)!=0)
      printf("fail!\n");


    memcpy(ip,&(saddr_r->sin_addr), 4);

    printf("remote ip = %i %i %i %i\n",
	 ip[0],
	 ip[1],
	 ip[2],
	 ip[3]);

    result=sizeof(struct sockaddr_in);
    if (getsockname(sock,(struct sockaddr *)saddr_l,&result)!=0)
      printf("fail!\n");

  memcpy(ipstr,&(saddr_l->sin_addr), 4);

  printf("local ip = %i %i %i %i\n",
	 ipstr[0],
	 ipstr[1],
	 ipstr[2],
	 ipstr[3]);


  sasl_setprop(conn,   SASL_IP_REMOTE, saddr_r);  




  printf("%i\n",saddr_l->sin_addr);
  printf("port=%i\n",saddr_l->sin_port);

  sasl_setprop(conn,   SASL_IP_LOCAL, saddr_l);


  /* */
  gethostname(hostname,sizeof(hostname));
  if (! (hp = gethostbyname(hostname))) exit(1); 


  /* call sasl client start */
  result=sasl_client_start(conn, "KERBEROS_V4", /* mechlist */
			   secret, &client_interact,
			   &out, &outlen,
			   &mechusing);

  if (client_interact!=NULL)
  {
    interaction(client_interact); /* fill in prompt */
    result=sasl_client_start(conn, AUTH, /* mechlist */
			     secret, &client_interact,
			     &out, &outlen,
			     &mechusing);
  }  
  checkerror(result);
  free(out);

    send(sock, "a001 authenticate KERBEROS_V4\r\n", strlen("a001 authenticate KERBEROS_V4\r\n"), 0);

    in=read_string();    


    while (done==0)
    {

      result=sasl_client_step(conn,
			    in,
			    strlen(in),
			    &client_interact,
			    &out,
			    &outlen);

      if (client_interact!=NULL)
      {
	interaction(client_interact); /* fill in prompt */
	result=sasl_client_step(conn,
				in,
				strlen(in),
				&client_interact,
				&out,
				&outlen);
      }
      checkerror(result);


      to64(sen, out, outlen);
      send(sock, sen, strlen(sen), 0);
      printf("C: %s", sen);
      free(out);
      
      in=read_string();
      if (strlen(in)>6)
	if ((in[5]=='O') && (in[6]=='K'))
	  done=1;
    }

  if (result!=SASL_OK)
  {
    printf("didn't succeed\n");
    printf("result=%i\n", result);
    exit(1);
  }

  str=(char *) malloc(1024);
YO_add_sasl(yofile, conn);

  count=0;
  while (count<10)
  {

    fgets(str, 1000, stdin);

    /*    for (lup=0;lup<10;lup++)
      printf("%i ",out[lup]);

      printf("[%s]\n",out);*/
  result=strlen(str);
  memcpy(str+result-1,"\r\n\0",3);

  result=sasl_encode(conn, str, 
		     result+1, &out,(unsigned int *) &outlen);
  checkerror(result);

  send(sock, out, outlen, 0);

  out=(char *) malloc(300);

  YO_fgets(out,100, yofile);
  printf("S: %s",out);

  count++;

  }

  free(str);
  exit(0);
}
