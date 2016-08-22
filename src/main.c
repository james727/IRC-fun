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
void *handle_new_connection (void *newsockfd);
struct new_connection *create_new_connection(int newsockfd, struct sockaddr_in client_addr);
void process_user_message(struct new_connection conn, char message[256]);
int send_greeting(struct new_connection conn);
int check_nick(char nick[]);
int check_connection_complete(struct new_connection conn);
void add_nick(char nick[]);

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
  int newsockfd; /* socket id for new connections */

  /* listen to socket and spawn new threads as needed */
  while (1) {
    listen(sockfd, 5); /* listen to socket; 5 is max backlog queue (5 is max for most systems) */
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); /* wait for an incoming connection */
    struct new_connection *current_conn = create_new_connection(newsockfd, cli_addr);
    if (newsockfd < 0){ /* check that things are working properly (hopefully) */
      chilog (INFO, "Error accepting socket connection\n");
    }
    pthread_t new_thread;
    pthread_create(&new_thread, NULL, handle_new_connection, (void*) current_conn);
  }

}

void *handle_new_connection(void *connection){
  struct new_connection current_conn = *(struct new_connection*) connection;
  char buffer[256]; /* read characters from socket into this buffer */
  int n;
  while (1){
    
    /* read from the socket */
    bzero(buffer, 256); /* zero out buffer prior to writing the incoming message */
    n = read(*(current_conn.newsockfd), buffer, 255); /* read from the socket */
    if (n < 0){
      chilog(INFO, "Error reading from socket");
    }

    process_user_message(current_conn, buffer); /* send the buffer for processing */

  }

  return NULL;

}

void process_user_message(struct new_connection connection, char message[256]){

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

    /* NICK command */
    if (strcmp(token, "NICK") == 0){
      /* copy nick to connection struct */
      strcpy(connection.nick, strtok(NULL, s));

      /* check if nick exists already */
      n = check_nick(connection.nick);
      if (n == 0){
        bzero(connection.nick, MAX_NICK);
        chilog(INFO,"NICK IN USE\n");
      }
      else if (n == 1){
        add_nick(connection.nick);
      }

      /* check if both nick and user have been received */
      if (check_connection_complete(connection)==1){
        send_greeting(connection);
      }

    }

    /* USER command */
    else if (strcmp(token, "USER") == 0){
      /* copy the user to the connection */
      strcpy(connection.user, strtok(NULL, s));

      /* check if both user and nick have been received */
      if (check_connection_complete(connection)==1){
        send_greeting(connection);
      }

    }

    /* EXIT command */
    else if (strcmp(token, "EXIT") == 0){
      close(*(connection.newsockfd));
    }

    /* OTHER commands - to be built */
    else {
      chilog(INFO, "%s", token);
    }

    token = strtok(NULL, s);

  }
}

struct new_connection *create_new_connection(int newsockfd, struct sockaddr_in client_addr){
  struct new_connection *user = malloc(sizeof(struct new_connection)); /* allocate space for the struct */

  /* space for member variables */
  user -> newsockfd = malloc(sizeof(int));
  user -> client_addr = malloc(sizeof(client_addr));
  user -> nick = malloc(MAX_NICK);
  user -> user = malloc(MAX_USER);

  /* zero out nick and user */
  bzero((*user).nick, MAX_NICK);
  bzero((*user).user, MAX_USER);

  /* copy arguments to member variables and return */
  *((*user).newsockfd) = newsockfd;
  memcpy((*user).client_addr, &client_addr, sizeof(struct sockaddr_in));
  return user;
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

int check_connection_complete(struct new_connection connection){
  if (*(connection.nick) != '\0' && *(connection.user) != '\0'){
    return 1;
  }
  return 0;
}

int send_greeting(struct new_connection conn){
  /* get ip addresses */
  char s_addr[INET_ADDRSTRLEN];
  char d_addr[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(server_addr.sin_addr), s_addr, INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &((*conn.client_addr).sin_addr), d_addr, INET_ADDRSTRLEN);

  /* set up msg code */
  char code[4];
  sprintf(code, "%03d", 1);

  /* set up user id */
  int uid_l = strlen(conn.nick)+ 1 + strlen(conn.user)+ 1 + strlen(d_addr);
  char uid[uid_l];
  sprintf(uid, "%s!%s@%s", conn.nick, conn.user, d_addr);
  char msg[39];

  /* compose welcome message */
  sprintf(msg, ":Welcome to the Internet Relay Network");

  int s_final_msg = 1 + strlen(s_addr) + 1 + 3 + 1 + strlen(conn.nick) + 1 + 38 + 1 + uid_l + 1;
  char buffer[s_final_msg];
  bzero(buffer, s_final_msg);

  sprintf(buffer, ":%s %s %s %s %s\n", s_addr, code, conn.nick, msg, uid);
  send(*(conn.newsockfd), buffer, s_final_msg, 0);

  return 0;
}
