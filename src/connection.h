#include <stdio.h>
#include <pthread.h>

struct new_connection{
  pthread_t *thread;
  int *newsockfd;
  struct sockaddr_in *client_addr;
  char *nick;
  char *user;
  char *realname;
  char *away;
  int *num_channels;
  int *is_global_operator;
  int *is_channel_operator;
};
