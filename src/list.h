#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "connection.h"
#include "channel.h"

struct node {
  struct new_connection *connected_user;
  struct node *next;
};

struct channel_node {
  struct channel *channel_data;
  struct channel_node *next;
};

struct linked_list {
  struct node *head;
};

struct channel_list {
  struct channel_node *head;
};

struct linked_list *create_new_list(void);
struct channel_list *create_channel_list(void);
void insert_element(struct new_connection *user_conn, struct linked_list *list);
void insert_channel(struct channel *new_channel, struct channel_list *list);
void print_list(struct linked_list list_to_print);
void print_node_value(struct node *node_to_print);
struct new_connection *search(struct linked_list list, char *search_nick);
struct channel *search_channels(struct channel_list list, char *search_channel_name);
void delete_element(struct linked_list *list, char *nick_to_delete);
void delete_channel(struct channel_list *list, char *channel_name_to_delete);
