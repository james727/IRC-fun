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
#define MAX_REALNAME 50
#define MAX_NICKS 100
#define MAX_HOST 50
#define MAX_MESSAGE 512
#define MAX_TOPIC 100

int set_up_socket(void);
void bind_to_port(int sockfd, char *port);
void listen_to_port(int sockfd);
void *handle_new_connection (void *newsockfd);
struct new_connection *create_new_connection(int newsockfd, struct sockaddr_in client_addr, pthread_t *thread);
int process_user_message(struct new_connection *conn, char message[256]);
int send_greetings(struct new_connection *conn);
int send_welcome(struct new_connection *conn);
int send_yourhost(struct new_connection *conn);
int send_myinfo(struct new_connection *conn);
int send_created(struct new_connection *conn);
int send_motd(struct new_connection *conn, FILE *fp);
int send_nosuchnick(struct new_connection *conn, char *nick);
int send_privmsg(struct new_connection *conn, struct new_connection *dest_conn, char *message);
int send_channelmsg(struct new_connection *conn, struct channel *dest_channel, char *message);
int send_notice(struct new_connection *conn, struct new_connection *dest_conn, char *message);
int send_channelnotice(struct new_connection *conn, struct channel *dest_channel, char *message);
int send_whois(struct new_connection *conn, struct new_connection *whois_conn);
int check_nick(char nick[]);
int check_connection_complete(struct new_connection *conn);
void add_nick(struct new_connection *conn);
void close_connection(struct new_connection *user_conn);
int send_message(struct new_connection *conn, char *message_body, int message_code);
int handle_quit(struct new_connection *conn, char *message);
int handle_nick(struct new_connection *conn, char *nick);
int handle_user(struct new_connection *conn, char *user);
int handle_privmsg(struct new_connection *conn, char *user_msg);
int handle_ping(struct new_connection *conn, char *params);
int handle_pong(struct new_connection *conn, char *params);
int handle_motd(struct new_connection *conn, char *params);
int handle_lusers(struct new_connection *conn, char *params);
int handle_whois(struct new_connection *conn, char *params);
int handle_notice(struct new_connection *conn, char *user_msg);
int handle_list(struct new_connection *conn, char *params);
int handle_join(struct new_connection *conn, char *channel);
int handle_names(struct new_connection *conn, char *params);
int send_command_not_found(struct new_connection *conn, char *command);
int send_list_repl(struct new_connection *conn, struct channel *channel_to_send);
char *get_last_param(char *params);
struct channel *create_channel(char *name, char *topic);
int add_user(struct channel *channel, struct new_connection *user);
int send_topic(struct new_connection *conn, struct channel *channel);
int send_name_message(struct new_connection *conn, struct channel *channel);
char *get_nick_list(struct channel *channel);
int check_channel_permission(struct new_connection *conn, struct channel *chann);

int current_users = 0;
int current_unknown_connections = 0;
int current_operators = 0;
int current_servers = 1;
int current_services = 0;
int current_channels = 0;
struct sockaddr_in server_addr;
struct linked_list connections;
struct channel_list channels;

typedef int (*CmdHandler)(struct new_connection *, char *);

#define CMD_COUNT 13
char *commands[] = {"NICK", "USER", "QUIT", "PRIVMSG", "PING", "PONG", "MOTD", "LUSERS", "WHOIS", "NOTICE", "LIST", "JOIN", "NAMES"};
CmdHandler handlers[] = {handle_nick, handle_user, handle_quit, handle_privmsg, handle_ping, handle_pong, handle_motd, handle_lusers, handle_whois, handle_notice, handle_list, handle_join, handle_names};
char *version = "1.0";
char *server_info = "The greatest IRC server of all time";
time_t t;
struct tm create_time;

int main(int argc, char *argv[])
{
    t = time(NULL);
    create_time = *localtime(&t);
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

  /* set up list of clients */
  connections = *create_new_list();

  /* set up 2 channels (for testing) */
  char *channel1name = "General";
  char *channel2name = "Random";
  char *topic1 = "General discussion for the masses";
  struct channel *channel1 = create_channel(channel1name, topic1);
  struct channel *channel2 = create_channel(channel2name, NULL);
  channels = *create_channel_list();
  insert_channel(channel1, &channels);
  insert_channel(channel2, &channels);

  /* socket fun */
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
  const int BUF_SIZE = 700;
  char buffer[BUF_SIZE]; /* read characters from socket into this buffer */
  int readpos = 0;
  int characters_read;
  ++current_unknown_connections;

  while (1){
    /* read from the socket and increment readpos */
    characters_read = read(*((*current_conn).newsockfd), &buffer[readpos], BUF_SIZE-readpos); /* read from the socket */
    if (characters_read < 0){
      pthread_exit(NULL);
      break;
    }
    readpos += characters_read;

    /* loop through chars starting at beginning of last readpos-1 (to check if there was a \r) */
    for (int i = readpos-characters_read-1; i < readpos-1; ++i){
      if (buffer[i] == '\r' && buffer[i+1] == '\n'){
        buffer[i] = '\0';
        buffer[i+1] = ' ';
        process_user_message(current_conn, buffer); /* send the buffer for processing */
        memmove(buffer, buffer+i+2, readpos-(i+2)); /* move remainder of buffer to beginning */
        readpos = readpos-(i+2);
      }
    }
  }
  return NULL;
}

int process_user_message(struct new_connection *connection, char message[700]){

  const char s[2] = " ";
  char *token;
  char *save;
  token = strtok_r(message, s, &save);

  /* check if in recognized commands */
  for (int i = 0; i < CMD_COUNT; ++i){
    if (strcmp(token, commands[i]) == 0){
      handlers[i](connection, save);
      return 0;
    }
  }
  send_command_not_found(connection, token);
  return 0;
}

struct new_connection *create_new_connection(int newsockfd, struct sockaddr_in client_addr, pthread_t *thread){

  /* allocate space for the struct and member variables */
  struct new_connection *user = malloc(sizeof(struct new_connection));
  user -> newsockfd = malloc(sizeof(int));
  user -> client_addr = malloc(sizeof(client_addr));
  user -> nick = malloc(MAX_NICK);
  user -> user = malloc(MAX_USER);
  user -> realname = malloc(MAX_REALNAME);

  /* zero out nick and user */
  bzero((*user).nick, MAX_NICK);
  bzero((*user).user, MAX_USER);
  bzero((*user).realname, MAX_REALNAME);

  /* copy arguments to member variables and return */
  (*user).thread = thread;
  *((*user).newsockfd) = newsockfd;
  memcpy((*user).client_addr, &client_addr, sizeof(struct sockaddr_in));
  return user;

}

int check_nick(char nick[]){
  if (search(connections, nick) != NULL){
    return 1;
  }
  return 0;
}

void add_nick(struct new_connection *user_conn){
  insert_element(user_conn, &connections);
}

void close_connection(struct new_connection *user_conn){
  delete_element(&connections, (*user_conn).nick);
  close(*(*user_conn).newsockfd);

  /* free stuff */
  free((*user_conn).nick);
  free((*user_conn).user);
  free((*user_conn).realname);
  free((*user_conn).realname);
  free((*user_conn).client_addr);
  free((*user_conn).newsockfd);
  free((*user_conn).thread);
  free(user_conn);

  /* shut down thread */
  pthread_exit(NULL);
}

int check_connection_complete(struct new_connection *connection){
  if (*((*connection).nick) != '\0' && *((*connection).user) != '\0'){
    return 1;
  }
  return 0;
}

int send_welcome(struct new_connection *conn){
  /* compose welcome message */
  char *msg = ":Welcome to the Internet Relay Network";

  /* set up host message */
  char host_addr[MAX_HOST];
  struct hostent *client_host = gethostbyaddr(&(*(*conn).client_addr).sin_addr, sizeof(struct in_addr), AF_INET);
  sprintf(host_addr, "%s", (*client_host).h_name);

  /* set up user id */
  int uid_l = strlen((*conn).nick)+ 1 + strlen((*conn).user)+ 1 + strlen(host_addr) + 1;
  char uid[uid_l];
  sprintf(uid, "%s!%s@%s", (*conn).nick, (*conn).user, host_addr);

  /* set up message */
  int msglen = strlen(msg) + 1 + strlen(uid)+1;
  char finalmsg[msglen];
  sprintf(finalmsg, "%s %s", msg, uid);
  send_message(conn, finalmsg, 1);

  return 0;
}

int send_myinfo(struct new_connection *conn){
  /* set up hostname */
  char host_addr[MAX_HOST];
  struct hostent *server_host = gethostbyaddr(&server_addr.sin_addr, sizeof(struct in_addr), AF_INET);
  if (server_host == NULL){
    inet_ntop(AF_INET, &(server_addr.sin_addr), host_addr, INET_ADDRSTRLEN);
  }
  else{
    sprintf(host_addr, "%s", (*server_host).h_name);
  }
  /* modes */
  char *modes = "ao mtov";
  /*compose msg */
  int msglen = 1 + strlen(host_addr) + 1 + strlen(version) + 1 + strlen(modes) + 1;
  char msg[msglen];
  sprintf(msg, "%s %s %s", host_addr, version, modes);
  send_message(conn, msg, 4);
  return 0;
}

int send_yourhost(struct new_connection *conn){
  /* set up standard message */
  char *msg1 = ":Your host is ";
  char *msg2 = ", running version ";

  /* set up hostname */
  char host_addr[MAX_HOST];
  struct hostent *server_host = gethostbyaddr(&server_addr.sin_addr, sizeof(struct in_addr), AF_INET);
  if (server_host == NULL){
    inet_ntop(AF_INET, &(server_addr.sin_addr), host_addr, INET_ADDRSTRLEN);
  }
  else{
    sprintf(host_addr, "%s", (*server_host).h_name);
  }

  int msglen = strlen(msg1)+strlen(msg2)+strlen(host_addr)+strlen(version)+1;
  char msg[msglen];
  sprintf(msg, "%s%s%s%s", msg1, host_addr, msg2, version);
  send_message(conn, msg, 2);
  return 0;
}

int send_created(struct new_connection *conn){
  /* generic message */
  char *msg1 = ":This server was created ";
  /* set up date */
  int year = create_time.tm_year + 1900;
  int day = create_time.tm_mday;
  int month = create_time.tm_mon;
  int datelen = 4 + 1 + 2 + 1 + 2;
  char date[datelen];
  sprintf(date, "%02d-%02d-%d", month, day, year);
  /* set up final message */
  int msglen = strlen(msg1) + strlen(date) + 1;
  char msg[msglen];
  sprintf(msg, "%s%s", msg1, date);
  send_message(conn, msg, 3);
  return 0;
}

int send_greetings(struct new_connection *conn){
  send_welcome(conn);
  send_yourhost(conn);
  send_created(conn);
  send_myinfo(conn);
  char *tmp = "dummy";
  handle_lusers(conn, tmp);
  handle_motd(conn, tmp);
  return 0;
}

int send_message(struct new_connection *conn, char *message_body, int message_code){
  /* get ip addresses and hosts */
  char s_addr[INET_ADDRSTRLEN];
  char d_addr[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(server_addr.sin_addr), s_addr, INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &((*(*conn).client_addr).sin_addr), d_addr, INET_ADDRSTRLEN);

  /* set up msg code */
  char code[4];
  sprintf(code, "%03d", message_code);

  /* set up and write final message */
  char *nick;
  if ((*conn).nick[0] == '\0'){
    nick = "*";
  }
  else {
    nick = (*conn).nick;
  }
  int s_final_msg = 1 + strlen(s_addr) + 1 + 3 + 1 + strlen(nick) + 1 + strlen(message_body) + 2;
  char buffer[s_final_msg];
  bzero(buffer, s_final_msg);
  sprintf(buffer, ":%s %s %s %s\r\n", s_addr, code, nick, message_body);
  send(*((*conn).newsockfd), buffer, s_final_msg, 0);

  return 0;
}

int send_nosuchnick(struct new_connection *conn, char *nick){
  char *msg2 = " :No such nick/channel";
  int msglen = 1 + strlen(nick) + strlen(msg2) + 1;
  char msg[msglen];
  sprintf(msg, "%s%s", nick, msg2);
  send_message(conn, msg, 401);
  return 0;
}

int send_privmsg(struct new_connection *conn, struct new_connection *dest_conn, char *msg){
  /* set up host message */
  char host_addr[MAX_HOST];
  struct hostent *client_host = gethostbyaddr(&(*(*conn).client_addr).sin_addr, sizeof(struct in_addr), AF_INET);
  sprintf(host_addr, "%s", (*client_host).h_name);

  /* set up user id */
  int uid_l = 1 + strlen((*conn).nick)+ 1 + strlen((*conn).user)+ 1 + strlen(host_addr) + 1;
  char uid[uid_l];
  sprintf(uid, ":%s!%s@%s", (*conn).nick, (*conn).user, host_addr);

  char *pm = "PRIVMSG";
  char *sender = (*dest_conn).nick;

  int msglen = strlen(uid) + 1 + strlen(pm) + 1 + strlen(sender) + 2 + strlen(msg) + 3;
  char msg_final[msglen];
  sprintf(msg_final, "%s %s %s :%s\r\n", uid, pm, sender, msg);
  if (msglen > 512){
    chilog(INFO,"WEIRDNESS REACHED");
    msg_final[510] = '\r';
    msg_final[511] = '\n';
    msglen = 512;
  }
  send(*((*dest_conn).newsockfd), msg_final, msglen, 0);

  return 0;
}

int handle_quit(struct new_connection *conn, char *message){
  /* get hostname */
  struct hostent *client_host = gethostbyaddr(&(*(*conn).client_addr).sin_addr, sizeof(struct in_addr), AF_INET);
  /* compose and send message */
  if (message == NULL){
    message = "Client Quit";
  }
  int msglen = 13 + strlen((*client_host).h_name) + 1 + strlen(message);
  char msg[msglen];
  sprintf(msg, "Closing link %s %s", (*client_host).h_name, message);
  send_message(conn, msg, 0);
  --current_users;
  /* close connection */
  close_connection(conn);
  return 0;
}

int handle_nick(struct new_connection *conn, char *nick_params){
  /* pull off nick */
  const char s[2] = " ";
  char *save;
  char *nick = strtok_r(nick_params, s, &save);
  /* check if nick exists already */
  if (check_nick(nick) == 1){
    int msglen = strlen(nick) + 1 + 28 + 1;
    char msg[msglen];
    sprintf(msg,"%s :Nickname is already in use", nick);
    send_message(conn, msg, 433);
  }
  else{
    if (*((*conn).nick) != '\0'){
      /* copy nick to connection struct */
      strcpy((*conn).nick, nick);
      return 0;
    }
    strcpy((*conn).nick, nick);
    add_nick(conn);
    /* check if both nick and user have been received */
    if (check_connection_complete(conn)==1){
      ++current_users;
      --current_unknown_connections;
      send_greetings(conn);
    }
  }
  return 0;
}

int handle_user(struct new_connection *conn, char *user_params){
  /* pull off user */
  const char s[2] = " ";
  char *save;
  char *user = strtok_r(user_params, s, &save);
  /* check if user command has already been sent */
  if (*((*conn).user) != '\0'){
    char *msg = ":Unauthorized command (already registered)";
    send_message(conn, msg, 462);
  }
  else{
    /* copy the user to the connection */
    strcpy((*conn).user, user);
    char *fullname = get_last_param(save);
    strcpy((*conn).realname, fullname);

    /* check if both user and nick have been received */
    if (check_connection_complete(conn)==1){
      ++current_users;
      --current_unknown_connections;
      send_greetings(conn);
    }
  }
  return 0;
}

int handle_privmsg(struct new_connection *conn, char *msg_user){
  /* pull of destination nick/channel */
  const char s[2] = " ";
  char *save;
  char *dest_nick = strtok_r(msg_user, s, &save);

  /* check if exists */
  struct new_connection *dest_conn = search(connections, dest_nick);
  if (dest_conn != NULL){
    send_privmsg(conn, dest_conn, save+1);     /* send message (+1 to avoid :) */
  }
  else{
    /* check if it's a channel */
    struct channel *dest_channel = search_channels(channels, dest_nick);
    if (dest_channel != NULL){
      send_channelmsg(conn, dest_channel, save+1);
    }
    else {
      send_nosuchnick(conn, dest_nick); /* send error msg */
    }
  }
  return 0;
}

int handle_ping(struct new_connection *conn, char *params){
  /* get ip addresses and hosts */
  char s_addr[INET_ADDRSTRLEN];
  char d_addr[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(server_addr.sin_addr), s_addr, INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &((*(*conn).client_addr).sin_addr), d_addr, INET_ADDRSTRLEN);

  char *repl = "PONG";

  int s_final_msg = 1 + strlen(s_addr) + 1 + strlen(repl) + 1 + strlen(s_addr) + 2;
  char buffer[s_final_msg];
  bzero(buffer, s_final_msg);
  sprintf(buffer, ":%s %s %s\r\n", s_addr, repl, s_addr);
  send(*((*conn).newsockfd), buffer, s_final_msg, 0);
  return 0;
}

int handle_pong(struct new_connection *conn, char *params){
  return 0;
}

int handle_motd(struct new_connection *conn, char *params){
  FILE *fp = fopen("motd.txt", "r");
  if (fp != NULL){
    send_motd(conn, fp);
  }
  else {
    char *msg = ":MOTD File is missing";
    send_message(conn, msg, 422);
  }
  return 0;
}

int send_motd(struct new_connection *conn, FILE *fp){
  /* set up hostname */
  char host_addr[MAX_HOST];
  struct hostent *server_host = gethostbyaddr(&server_addr.sin_addr, sizeof(struct in_addr), AF_INET);
  if (server_host == NULL){
    inet_ntop(AF_INET, &(server_addr.sin_addr), host_addr, INET_ADDRSTRLEN);
  }
  else{
    sprintf(host_addr, "%s", (*server_host).h_name);
  }

  /* set up messages */
  int msg1len = 3 + strlen(host_addr) + 22 + 1;
  char msg1[msg1len];
  sprintf(msg1, ":- %s Message of the day - ", host_addr);
  char *msg3 = ":End of MOTD command";

  /* send 1st message */
  send_message(conn, msg1, 375);

  /* loop over lines in file  */
  char current_line[256];
  current_line[0] = ':';
  current_line[1] = '-';
  current_line[2] = ' ';
  int i = 3;
  char current_char = fgetc(fp);
  while (current_char != EOF){
    if (current_char == '\n'){
      current_line[i] = '\0';
      i = 3;
      send_message(conn, &current_line[0], 372);
    }
    else{
      current_line[i] = current_char;
      ++i;
    }
    current_char = fgetc(fp);
  }
  if (i != 3){
    send_message(conn, &current_line[0], 372);
  }

  /* send final message */
  send_message(conn, msg3, 376);
  return 0;
}

int handle_lusers(struct new_connection *conn, char *params){
  /* first reply */
  int msg1len = 12 + 42 + 1;
  char msg1[msg1len];
  sprintf(msg1, ":There are %d users and %d services on %d servers", current_users, current_services, current_servers);
  send_message(conn, msg1, 251);

  /* second reply */
  int msg2len = 4 + 20 + 1;
  char msg2[msg2len];
  sprintf(msg2, "%d :operator(s) online", current_operators);
  send_message(conn, msg2, 252);

  /* third reply */
  int msg3len = 4 + 23 + 1;
  char msg3[msg3len];
  sprintf(msg3, "%d :unknown connection(s)", current_unknown_connections);
  send_message(conn, msg3, 253);

  /* fourth reply */
  int msg4len = 4 + 17 + 1;
  char msg4[msg4len];
  sprintf(msg4, "%d :channels formed", current_channels);
  send_message(conn, msg4, 254);

  /* fifth and final reply */
  int msg5len = 8 + 29 + 1;
  char msg5[msg5len];
  int total_clients = current_users + current_unknown_connections;
  sprintf(msg5, ":I have %d clients and %d servers", total_clients, current_servers);
  send_message(conn, msg5, 255);

  return 0;
}

int handle_whois(struct new_connection *conn, char *params){
  /* pull off nick */
  const char s[2] = " ";
  char *save;
  char *whois_nick = strtok_r(params, s, &save);

  /* check if exists */
  struct new_connection *whois_conn = search(connections, whois_nick);
  if (whois_conn != NULL){
    send_whois(conn, whois_conn);     /* send message */
  }
  else{
    send_nosuchnick(conn, whois_nick); /* send error msg */
  }
  return 0;
}

int send_whois(struct new_connection *conn, struct new_connection *whois_conn){
  /* set up outputs for 1st reply */
  char host_addr[MAX_HOST];
  struct hostent *client_host = gethostbyaddr(&(*(*whois_conn).client_addr).sin_addr, sizeof(struct in_addr), AF_INET);
  sprintf(host_addr, "%s", (*client_host).h_name);
  char *whoisnick = (*whois_conn).nick;
  char *whoisuser = (*whois_conn).user;
  char *whoisname = (*whois_conn).realname;
  if (whoisname == NULL){
    whoisname = "";
  }

  /* send 1st reply */
  int msg1len = strlen(whoisnick) + 1 + strlen(whoisuser) + 1 + strlen(host_addr) + 3 + strlen(whoisname) + 1;
  char msg1[msg1len];
  sprintf(msg1, "%s %s %s * :%s", whoisnick, whoisuser, host_addr, whoisname);
  send_message(conn, msg1, 311);

  /* set up 2nd reply (hostname first) */
  /* set up hostname */
  char server[MAX_HOST];
  struct hostent *server_host = gethostbyaddr(&server_addr.sin_addr, sizeof(struct in_addr), AF_INET);
  if (server_host == NULL){
    inet_ntop(AF_INET, &(server_addr.sin_addr), server, INET_ADDRSTRLEN);
  }
  else{
    sprintf(server, "%s", (*server_host).h_name);
  }
  int msg2len = strlen(whoisnick) + 1 + strlen(server) + 2 + strlen(server_info) + 1;
  char msg2[msg2len];
  sprintf(msg2, "%s %s :%s", whoisnick, server, server_info);
  send_message(conn, msg2, 312);

  /* end of whois setup */
  int msg3len = strlen(whoisnick) + 19 + 1;
  char msg3[msg3len];
  sprintf(msg3, "%s :End of WHOIS list", whoisnick);
  send_message(conn, msg3, 318);

  return 0;
}

int send_command_not_found(struct new_connection *conn, char *command){
  int msglen = strlen(command) + 17 + 1;
  char msg[msglen];
  sprintf(msg, "%s :Unknown command", command);
  send_message(conn, msg, 421);
  return 0;
}

int handle_notice(struct new_connection *conn, char *msg_user){
  /* pull of destination nick */
  const char s[2] = " ";
  char *save;
  char *dest_nick = strtok_r(msg_user, s, &save);

  /* check if exists */
  struct new_connection *dest_conn = search(connections, dest_nick);
  if (dest_conn != NULL){
    send_notice(conn, dest_conn, save+1);     /* send message (+1 to avoid :) */
  }
  else{
    /* check if it's a channel */
    struct channel *dest_channel = search_channels(channels, dest_nick);
    if (dest_channel != NULL){
      send_channelnotice(conn, dest_channel, save+1);
    }
  }
  return 0;
}

int send_notice(struct new_connection *conn, struct new_connection *dest_conn, char *msg){
  /* set up host message */
  char host_addr[MAX_HOST];
  struct hostent *client_host = gethostbyaddr(&(*(*conn).client_addr).sin_addr, sizeof(struct in_addr), AF_INET);
  sprintf(host_addr, "%s", (*client_host).h_name);

  /* set up user id */
  int uid_l = 1 + strlen((*conn).nick)+ 1 + strlen((*conn).user)+ 1 + strlen(host_addr) + 1;
  char uid[uid_l];
  sprintf(uid, ":%s!%s@%s", (*conn).nick, (*conn).user, host_addr);

  char *pm = "NOTICE";
  char *sender = (*dest_conn).nick;

  int msglen = strlen(uid) + 1 + strlen(pm) + 1 + strlen(sender) + 2 + strlen(msg) + 3;
  char msg_final[msglen];
  sprintf(msg_final, "%s %s %s :%s\r\n", uid, pm, sender, msg);
  send(*((*dest_conn).newsockfd), msg_final, msglen, 0);

  return 0;
}

char *get_last_param(char *params){
  char *current_pointer = params;
  while (*current_pointer != ':'){
    current_pointer++;
  }
  return current_pointer+1;
}

struct channel *create_channel(char *name, char *topic){
  /* allocate space for the struct and member variables */
  struct channel *channel_data = malloc(sizeof(struct channel));
  channel_data -> name = malloc(strlen(name)+1);
  memcpy((*channel_data).name, name, strlen(name)+1);
  channel_data -> topic = malloc(MAX_TOPIC);
  channel_data -> moderated_mode = malloc(sizeof(int));
  channel_data -> topic_mode = malloc(sizeof(int));
  channel_data -> users = create_new_list();
  channel_data -> num_users = malloc(sizeof(int));

  /* check if topic is passed in or NULL */
  if (topic == NULL){
    /* zero out topic */
    bzero((*channel_data).topic, MAX_TOPIC);
  }
  else {
    memcpy((*channel_data).topic, topic, strlen(topic)+1);
  }

  /* set int vars */
  *(*channel_data).moderated_mode = 0;
  *(*channel_data).topic_mode = 0;
  *(*channel_data).num_users = 0;

  return channel_data;
}

int handle_list(struct new_connection *conn, char *params){
  /* send channel replies */
  if (params == NULL){ /* if no params, send all channels */
    if (channels.head != NULL){
      struct channel_node *current = channels.head;
      while (current != NULL){
        send_list_repl(conn, (*current).channel_data);
        current = (*current).next;
      }
    }
  }
  else { /* search for specific channel and send */
    /* get channel name */
    char *channel_name = params;
    struct channel *searched_channel = search_channels(channels, channel_name);
    if (searched_channel != NULL){
      send_list_repl(conn, searched_channel);
    }
  }

  /* send end of list message */
  char *msg2 = ":End of LIST";
  send_message(conn, msg2, 323);

  return 0;
}

int send_list_repl(struct new_connection *conn, struct channel *channel_to_send){
  char *channel_name = (*channel_to_send).name;
  char *channel_topic = (*channel_to_send).topic;
  char num_users[10];
  sprintf(num_users, "%d", *(*channel_to_send).num_users);

  int msglen = strlen(channel_name) + 1 + strlen(num_users) + 2 + strlen(channel_topic) + 1;
  char msg[msglen];
  sprintf(msg, "%s %s :%s", channel_name, num_users, channel_topic);
  send_message(conn, msg, 322);
  return 0;
}

int handle_join(struct new_connection *conn, char *channel_to_join){
  /* see if channel exists ... */
  struct channel *searched_channel = search_channels(channels, channel_to_join);
  if (searched_channel == NULL){
    int msglen = strlen(channel_to_join) + 1 + 17 + 1;
    char msg[msglen];
    sprintf(msg, "%s :No such channel", channel_to_join);
    send_message(conn, msg, 403);
    return 0;
  }
  else {
    add_user(searched_channel, conn);
    send_topic(conn, searched_channel);
    handle_names(conn, channel_to_join);
  }
  return 0;
}

int add_user(struct channel *channel, struct new_connection *user){
  insert_element(user, (*channel).users);
  return 0;
}

int send_topic(struct new_connection *conn, struct channel *channel){
  /* check if topic has been set */
  if (*(*channel).topic == 0){
    return 0;
  }
  char *topic = (*channel).topic;
  char *name = (*channel).name;
  int msglen = strlen(name) + 2 + strlen(topic) + 1;
  char msg[msglen];
  sprintf(msg, "%s :%s", name, topic);
  send_message(conn, msg, 332);
  return 0;
}

int handle_names(struct new_connection *conn, char *params){
  if (params == NULL){
    /* send all ... */
    if (channels.head != NULL){
      struct channel_node *current = channels.head;
      while (current != NULL){
        struct channel *current_channel = (*current).channel_data;
        send_name_message(conn, current_channel);
        current = (*current).next;
      }
      /* send end of list message */
      char *msg2 = "* :End of NAMES list";
      send_message(conn, msg2, 366);
    }
  }
  else { /* search for channel */
    char *channel_name = params;
    struct channel *searched_channel = search_channels(channels, channel_name);
    if (searched_channel != NULL){
      send_name_message(conn, searched_channel);
    }
    /* send end of list message */
    int msg2len = strlen(channel_name) + 1 + 18 + 1;
    char msg2[msg2len];
    sprintf(msg2, "%s :End of NAMES list", channel_name);
    send_message(conn, msg2, 366);
  }

  return 0;
}

int send_name_message(struct new_connection *conn, struct channel *channel){
  char *nick_list = get_nick_list(channel);
  char *channel_name = (*channel).name;
  int msglen = 2 + strlen(channel_name) + 2 + strlen(nick_list) + 1;
  char msg[msglen];
  sprintf(msg, "= %s :%s", channel_name, nick_list);
  send_message(conn, msg, 353);
  free(nick_list);
  return 0;
}

char *get_nick_list(struct channel *channel){
  /* set up stuff based on MAX_NICK */
  int len_estimate = *(*channel).num_users*(MAX_NICK+1);
  char *nicks = malloc(len_estimate);
  int current_index = 0;
  struct linked_list user_list = *(*channel).users;
  if (user_list.head != NULL){
    /* loop through connected users and copy nick to buffer */
    struct node *current = user_list.head;
    while (current != NULL){
      char *current_nick = (*(*current).connected_user).nick;
      int nick_length = strlen(current_nick);
      sprintf(&nicks[current_index], "%s ", current_nick);
      current_index = current_index + nick_length + 1;
      current = (*current).next;
    }
    nicks[current_index-1] = '\0';
  }
  return nicks;
}

int send_channelmsg(struct new_connection *conn, struct channel *dest_channel, char *message){
  /* check permission for channel */
  int perm = check_channel_permission(conn, dest_channel);
  if (perm == 0){
    int msglen = strlen((*dest_channel).name) + 24 + 1;
    char msg[msglen];
    sprintf(msg, "%s :Cannot send to channel", (*dest_channel).name);
    send_message(conn, msg, 404);
    return 0;
  }

  /* get user list and check if empty */
  struct linked_list *user_list = (*dest_channel).users;
  if ((*user_list).head == NULL){
    return 0;
  }
  else {
    struct node *current = (*user_list).head;
    /* loop through channel members and send */
    while (current != NULL){
      struct new_connection *current_connection = (*current).connected_user;
      send_privmsg(conn, current_connection, message);
      current = (*current).next;
    }
    return 0;
  }
}

int check_channel_permission(struct new_connection *conn, struct channel *chann){
  /* check if user in channel */
  struct linked_list *channel_members = (*chann).users;
  struct new_connection *found_connection = search(*channel_members, (*conn).nick);
  if (found_connection != NULL){
    return 1;
  }
  return 0;
}

int send_channelnotice(struct new_connection *conn, struct channel *dest_channel, char *message){
  /* check permission for channel */
  int perm = check_channel_permission(conn, dest_channel);
  if (perm == 0){
    return 0; /* no error messages !! */
  }

  /* get user list and check if empty */
  struct linked_list *user_list = (*dest_channel).users;
  if ((*user_list).head == NULL){
    return 0;
  }
  else {
    struct node *current = (*user_list).head;
    /* loop through channel members and send */
    while (current != NULL){
      struct new_connection *current_connection = (*current).connected_user;
      send_privmsg(conn, current_connection, message);
      current = (*current).next;
    }
    return 0;
  }
}
