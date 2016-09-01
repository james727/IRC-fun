#include <stdio.h>

struct channel{
  char *name;
  char *topic;
  struct linked_list *users;
  struct linked_list *operators;
  struct linked_list *voices;
  int *num_users;
  int *moderated_mode;
  int *topic_mode;
};
