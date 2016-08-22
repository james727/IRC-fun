#include <stdio.h>

struct new_connection{
  int newsockfd;
  struct sockaddr_in client_addr;
};
