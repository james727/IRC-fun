#include <stdio.h>

struct channel{
  char *name;
  char *topic;
  struct linked_list *users;
  int *num_users;
  int *moderated_mode;
  int *topic_mode;
};
