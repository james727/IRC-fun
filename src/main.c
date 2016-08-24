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
#include <netdb.h>
#include "log.h"
#include "list.h"

#define MAX_NICK 20
#define MAX_USER 50
#define MAX_NICKS 100

int set_up_socket(void);
void bind_to_port(int sockfd, char *port);
void listen_to_port(int sockfd);
void *handle_new_connection (void *newsockfd);
struct new_connection *create_new_connection(int newsockfd, struct sockaddr_in client_addr, pthread_t *thread);
void process_user_message(struct new_connection *conn, char message[256]);
int send_greeting(struct new_connection *conn);
int check_nick(char nick[]);
int check_connection_complete(struct new_connection *conn);
void add_nick(struct new_connection *conn);
void close_connection(struct new_connection *user_conn);

int current_users = 0;
struct sockaddr_in server_addr;
struct linked_list connections;

void handle_nick(char *args);

typedef void (*CmdHandler)(char *);

/*#define CMD_COUNT 3
char *commands[] = {"NICK", "USER", "EXIT"};
CmdHandler handlers[] = {handle_nick, handle_user, handle_exit};

void handle_nick(char *args) {
  // do stuff
}*/

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

  connections = create_new_list();
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

  socklen_t clilen; /* size of client address */
  struct sockaddr_in cli_addr; /* address structs for server and client */
  int newsockfd; /* socket id for new connections */

  /* listen to socket and spawn new threads as needed */
  while (1) {
    listen(sockfd, 5); /* listen to socket; 5 is max backlog queue (5 is max for most systems) */
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); /* wait for an incoming connection */
    pthread_t new_thread;
    struct new_connection *current_conn = create_new_connection(newsockfd, cli_addr, &new_thread);
    if (newsockfd < 0){ /* check that things are working properly (hopefully) */
      chilog (INFO, "Error accepting socket connection\n");
    }
    pthread_create(&new_thread, NULL, handle_new_connection, (void*) current_conn);
  }

}

void *handle_new_connection(void *connection){
  struct new_connection *current_conn = (struct new_connection*) connection;
  const int BUF_SIZE = 256;
  char buffer[BUF_SIZE]; /* read characters from socket into this buffer */
  int readpos = 0;
  while (1){
    int n;
    /* read from the socket */
    // TODO: protect from over-long messages
    n = read(*((*current_conn).newsockfd), &buffer[readpos], BUF_SIZE-readpos); /* read from the socket */
    if (n < 0){
      break;
    }
    readpos += n;
    int i;
    for (i = readpos-n; i < readpos-1; ++i){
      if (buffer[i] == '\r' && buffer[i+1] == '\n'){
        buffer[i] = '\0';
        process_user_message(current_conn, buffer); /* send the buffer for processing */

        /* move remainder of buffer to beginning */
        memmove(buffer, buffer+i+2, readpos-(i+2));
        readpos = readpos-(i+2);
      }
    }

    /*process_user_message(current_conn, buffer);*/


  }

  return NULL;

}

/*char *parse_args(char *arg_start){
  const char s[2] = " ";

}*/

void process_user_message(struct new_connection *connection, char message[256]){

  const char s[2] = " ";
  char *token;
  char *save;
  token = strtok_r(message, s, &save);
  int n;


  while (token != NULL){
    /*int i;
    for (i = 0; i < CMD_COUNT; ++i){
      if (strcmp(token, commands[i]) == 0){
        handlers[i](strtok_r(NULL, s, &save));
      }
    }*/

    /* NICK command */
    if (strcmp(token, "NICK") == 0){
      /* copy nick to connection struct */
      strcpy((*connection).nick, strtok_r(NULL, s, &save));

      /* check if nick exists already */
      n = check_nick((*connection).nick);
      if (n == 1){
        bzero((*connection).nick, MAX_NICK);
        chilog(INFO,"NICK IN USE\n");
      }
      else if (n == 0){
        add_nick(connection);
      }

      /* check if both nick and user have been received */
      if (check_connection_complete(connection)==1){
        send_greeting(connection);
      }

    }

    /* USER command */
    else if (strcmp(token, "USER") == 0){
      /* copy the user to the connection */
      strcpy((*connection).user, strtok_r(NULL, s, &save));

      /* check if both user and nick have been received */
      if (check_connection_complete(connection)==1){
        send_greeting(connection);
      }

    }

    /* EXIT command */
    else if (strcmp(token, "EXIT") == 0){
      close_connection(connection);
    }

    /* OTHER commands - to be built */
    else {
      chilog(INFO, "%s", token);
    }

    token = strtok(NULL, s);

  }
}

struct new_connection *create_new_connection(int newsockfd, struct sockaddr_in client_addr, pthread_t *thread){

  /* allocate space for the struct and member variables */
  struct new_connection *user = malloc(sizeof(struct new_connection));
  user -> newsockfd = malloc(sizeof(int));
  user -> client_addr = malloc(sizeof(client_addr));
  user -> nick = malloc(MAX_NICK);
  user -> user = malloc(MAX_USER);

  /* zero out nick and user */
  bzero((*user).nick, MAX_NICK);
  bzero((*user).user, MAX_USER);

  /* copy arguments to member variables and return */
  (*user).thread = thread;
  *((*user).newsockfd) = newsockfd;
  memcpy((*user).client_addr, &client_addr, sizeof(struct sockaddr_in));
  return user;

}

int check_nick(char nick[]){
  return search(connections, nick);
}

void add_nick(struct new_connection *user_conn){
  chilog(INFO, "ADDING NICK %s\n", (*user_conn).nick);
  insert_element(user_conn, &connections);
  ++current_users;
  chilog(INFO, "Printing connections...\n");
  print_list(connections);
}

void close_connection(struct new_connection *user_conn){
  chilog(INFO, "Closing connection for NICK %s\n", (*user_conn).nick);
  delete_element(&connections, (*user_conn).nick);
  chilog(INFO, "Printing connections...\n");
  print_list(connections);
  free(user_conn);
  close(*(*user_conn).newsockfd);
  /*pthread_join(*(*user_conn).thread, NULL);*/
  pthread_exit(NULL);
}

int check_connection_complete(struct new_connection *connection){
  if (*((*connection).nick) != '\0' && *((*connection).user) != '\0'){
    return 1;
  }
  return 0;
}

int send_greeting(struct new_connection *conn){
  /* get ip addresses */
  char s_addr[INET_ADDRSTRLEN];
  char d_addr[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(server_addr.sin_addr), s_addr, INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &((*(*conn).client_addr).sin_addr), d_addr, INET_ADDRSTRLEN);
  struct hostent *client_host = gethostbyaddr(&(*(*conn).client_addr).sin_addr, sizeof(struct in_addr), AF_INET);
  if (client_host != NULL){
    chilog(INFO, "Successfully resolved host\n");
    sprintf(d_addr, (*client_host).h_name, strlen((*client_host).h_name));
  }


  /* set up msg code */
  char code[4];
  sprintf(code, "%03d", 1);

  /* set up user id */
  int uid_l = strlen((*conn).nick)+ 1 + strlen((*conn).user)+ 1 + strlen(d_addr);
  char uid[uid_l];
  sprintf(uid, "%s!%s@%s", (*conn).nick, (*conn).user, d_addr);

  /* compose welcome message */
  char msg[39];
  sprintf(msg, ":Welcome to the Internet Relay Network");
  int s_final_msg = 1 + strlen(s_addr) + 1 + 3 + 1 + strlen((*conn).nick) + 1 + 38 + 1 + uid_l + 2;
  char buffer[s_final_msg];
  bzero(buffer, s_final_msg);

  sprintf(buffer, ":%s %s %s %s %s\r\n", s_addr, code, (*conn).nick, msg, uid);
  send(*((*conn).newsockfd), buffer, s_final_msg, 0);

  return 0;
}
