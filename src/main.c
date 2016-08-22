/*
 *  IRC server project
 *  James Katz, 2016
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "log.h"
#include "list.h"
#include "connection.h"
#define MAX_NICK 20
#define MAX_USER 50
#define MAX_NICKS 100

int set_up_socket(void);
void bind_to_port(int sockfd, char *port);
void listen_to_port(int sockfd);
void handle_new_connection (void *newsockfd);
void process_user_message(struct new_connection conn, char message[256], int *nick_rec, int *user_rec, char nick[], char user[]);
int check_nick(char nick[]);
void add_nick(char nick[]);
int send_response(struct sockaddr_in sender, struct sockaddr_in dest, int msg_code,  char nick[], char user[], int sockpd);

char *nicks[MAX_NICKS];
int current_users = 0;
struct sockaddr_in server_addr;

int main(int argc, char *argv[])
{
    int opt;
    char *port = "6667", *passwd = NULL;
    int verbosity = 0;

    while ((opt = getopt(argc, argv, "p:o:vqh")) != -1)
        switch (opt)
        {
        case 'p':
            port = strdup(optarg);
            break;
        case 'o':
            passwd = strdup(optarg);
            break;
        case 'v':
            verbosity++;
            break;
        case 'q':
            verbosity = -1;
            break;
        case 'h':
            fprintf(stderr, "Usage: chirc -o PASSWD [-p PORT] [(-q|-v|-vv)]\n");
            exit(0);
            break;
        default:
            fprintf(stderr, "ERROR: Unknown option -%c\n", opt);
            exit(-1);
        }

    if (!passwd)
    {
        fprintf(stderr, "ERROR: You must specify an operator password\n");
        exit(-1);
    }

    /* Set logging level based on verbosity */
    switch(verbosity)
    {
    case -1:
        chirc_setloglevel(QUIET);
        break;
    case 0:
        chirc_setloglevel(INFO);
        break;
    case 1:
        chirc_setloglevel(DEBUG);
        break;
    case 2:
        chirc_setloglevel(TRACE);
        break;
    default:
        chirc_setloglevel(INFO);
        break;
    }

  int sockfd = set_up_socket();
  bind_to_port(sockfd, port);
  listen_to_port(sockfd);

	return 0;
}

int set_up_socket(void){
  int sockfd; /* initialize file descriptor for our new socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0); /* initialize socket */
  return sockfd;
}

void bind_to_port(int sockfd, char *port){
  int portno; /* the port number to listen on */
  /*struct sockaddr_in serv_addr; address structs for server and client */
  bzero((char*) &server_addr, sizeof(server_addr)); /* zero out the serv_addr struct */
  portno = atoi(port); /* convert port character string to integer */
  server_addr.sin_family = AF_INET; /* establish serv_addr values */
  server_addr.sin_port = htons(portno); /* set port number */
  server_addr.sin_addr.s_addr = INADDR_ANY; /* set address to localhost */

  /* bind socket to port */
  if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){ /* bind socket to serv_addr using sockfd */
    chilog(INFO, "Error binding to socket\n");
  }
}

void listen_to_port(int sockfd){

  int clilen; /* size of client address */
  struct sockaddr_in cli_addr; /* address structs for server and client */
  struct new_connection current_conn;
  int newsockfd; /* socket id for new connections */

  /* listen to socket and spawn new threads as needed */
  while (1) {
    listen(sockfd, 5); /* listen to socket; 5 is max backlog queue (5 is max for most systems) */
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); /* wait for an incoming connection */
    current_conn.newsockfd = newsockfd; /* add to the connection struct */
    current_conn.client_addr = cli_addr; /* add to the connection struct */
    if (newsockfd < 0){ /* check that things are working properly (hopefully) */
      chilog (INFO, "Error accepting socket connection\n");
    }
    handle_new_connection((void*) &current_conn); /* pass to handle new connection - will eventually be a separate thread */
  }

}

void handle_new_connection(void *connection){
  struct new_connection current_conn = *(struct new_connection*) connection;
  char buffer[256]; /* read characters from socket into this buffer */
  int nick_rec = 0;
  int user_rec = 0;
  char nick[MAX_NICK];
  char user[MAX_USER];
  bzero(nick, MAX_NICK);
  bzero(user, MAX_USER);
  int n;
  while (1){

    /* read from the socket */
    bzero(buffer, 256); /* zero out buffer prior to writing the incoming message */
    n = read(current_conn.newsockfd, buffer, 255); /* read from the socket */
    if (n < 0){
      chilog(INFO, "Error reading from socket");
    }

    process_user_message(current_conn, buffer, &nick_rec, &user_rec, nick, user);
    chilog(INFO,"in main loop nick = %s user = %s\n", nick, user);

    if (n < 0){
      chilog(INFO, "Error writing to socket");
    }

  }
}

void process_user_message(struct new_connection connection, char message[256], int *nick_rec, int *user_rec, char nick[], char user[]){

  int i;
  for (i = 0; i < 256; ++i){
    if (message[i] == '\r' && message[i+1] == '\n'){
      message[i] = '\0';
    }
  }

  const char s[2] = " ";
  char *token;
  token = strtok(message, s);
  int n;

  while (token != NULL){
    if (strcmp(token, "NICK") == 0){
      *nick_rec = 1;
      strcpy(nick, strtok(NULL, s));
      n = check_nick(nick);
      if (n == 0){
        *nick_rec = 0;
        chilog(INFO,"NICK IN USE\n");
      }
      else if (n == 1){
        add_nick(nick);
      }
      if (*nick_rec == 1 && *user_rec == 1){
        send_response(server_addr, connection.client_addr, 1, nick, user, connection.newsockfd);
      }
    }
    else if (strcmp(token, "USER") == 0){
      *user_rec = 1;
      strcpy(user, strtok(NULL, s));
      if (*nick_rec == 1 && *user_rec == 1){
        chilog(INFO, "%s %s", nick, user);
        send_response(server_addr, connection.client_addr, 1, nick, user, connection.newsockfd);
      }
    }
    else if (strcmp(token, "EXIT") == 0){
      close(connection.newsockfd);
    }
    else {
      chilog(INFO, "%s", token);
    }
    token = strtok(NULL, s);
  }
}

int check_nick(char nick[]){
  int i;
  for (i = 0; i < current_users; ++i){
    if (strcmp(nick, nicks[i]) == 0){
      return 0;
    }
  }
  return 1;
}

void add_nick(char nick[]){
  chilog(INFO, "ADDING NICK %s", nick);
  int n = strlen(nick);
  char *perm_nick = (char *) malloc(n);
  strcpy(perm_nick, nick);
  nicks[current_users] = perm_nick;
  ++current_users;
}

int send_response(struct sockaddr_in sender, struct sockaddr_in dest, int msg_code,  char nick[], char user[], int sockpd){
  /* get ip addresses */
  char s_addr[INET_ADDRSTRLEN];
  char d_addr[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(sender.sin_addr), s_addr, INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &(dest.sin_addr), d_addr, INET_ADDRSTRLEN);

  /* set up msg code */
  char code[4];
  sprintf(code, "%03d", msg_code);

  /* set up user id */
  int uid_l = strlen(nick)+ 1 + strlen(user)+ 1 + strlen(d_addr);
  char uid[uid_l];
  sprintf(uid, "%s!%s@%s", nick, user, d_addr);
  char msg[39];

  if (msg_code == 1){
    /* compose welcome message */
    sprintf(msg, ":Welcome to the Internet Relay Network");
  }

  int s_final_msg = 1 + strlen(s_addr) + 1 + 3 + 1 + strlen(nick) + 1 + 38 + 1 + uid_l + 1;
  char buffer[s_final_msg];
  bzero(buffer, s_final_msg);

  sprintf(buffer, ":%s %s %s %s %s\n", s_addr, code, nick, msg, uid);
  send(sockpd, buffer, s_final_msg, 0);

  return 0;
}
