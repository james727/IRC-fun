#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "connection.h"

struct node {
  struct new_connection *connected_user;
  struct node *next;
};

struct linked_list {
  struct node *head;
};

struct linked_list create_new_list(void);
void insert_element(struct new_connection *user_conn, struct linked_list *list);
void print_list(struct linked_list list_to_print);
void print_node_value(struct node *node_to_print);
struct new_connection *search(struct linked_list list, char *search_nick);
void delete_element(struct linked_list *list, char *nick_to_delete);
