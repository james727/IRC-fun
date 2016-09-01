#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "log.h"

struct linked_list *create_new_list(void){
  struct linked_list *list = malloc(sizeof(struct linked_list));
  (*list).head = NULL;
  return list;
}

struct channel_list *create_channel_list(void){
  struct channel_list *list = malloc(sizeof(struct channel_list));
  (*list).head = NULL;
  return list;
}

void insert_element(struct new_connection *user_conn, struct linked_list *list){
  /* initialize node */
  struct node *name_node = malloc(sizeof(struct node));
  (*name_node).connected_user = user_conn;

  /* prepend to list */
  (*name_node).next = (*list).head;
  (*list).head = name_node;
}

void insert_channel(struct channel *new_channel, struct channel_list *list){
  /* initialize node */
  struct channel_node *channel_node = malloc(sizeof(struct channel_node));
  (*channel_node).channel_data = new_channel;

  /* prepend to list */
  (*channel_node).next = (*list).head;
  (*list).head = channel_node;
}

void print_list(struct linked_list list_to_print){
  if (list_to_print.head == NULL) {
    chilog(INFO,"empty list\n");
  }
  else {
    struct node *current = list_to_print.head;
    while (current != NULL){
      print_node_value(current);
      current = (*current).next;
    }
  }
}

struct new_connection *search(struct linked_list list, char search_term[]){
  if (list.head == NULL) {
    return NULL;
  }
  else {
    struct node *current = list.head;
    while (current != NULL){
      if (strcmp(search_term, (*(*current).connected_user).nick) == 0){
        return (*current).connected_user;
      }
      current = (*current).next;
    }
    return NULL;
  }
}

struct channel *search_channels(struct channel_list list, char *search_channel_name){
  if (list.head == NULL) {
    return NULL;
  }
  else {
    struct channel_node *current = list.head;
    while (current != NULL){
      if (strcmp(search_channel_name, (*(*current).channel_data).name) == 0){
        return (*current).channel_data;
      }
      current = (*current).next;
    }
    return NULL;
  }
}

void delete_element(struct linked_list *list, char *name_to_delete){
  struct node *pointer1 = (*list).head;
  if (pointer1 == NULL){
    return;
  }
  if (strcmp(name_to_delete, (*(*pointer1).connected_user).nick) == 0){
    (*list).head = (*pointer1).next;
    free(pointer1);
    return;
  }
  else{
    struct node *pointer2 = (*pointer1).next;
    while (pointer2 != NULL){
      if (strcmp(name_to_delete, (*(*pointer2).connected_user).nick) == 0){
        chilog(INFO, "Name found for deletion\n");
        (*pointer1).next = (*pointer2).next;
        free(pointer2);
        return;
      }
      pointer1 = pointer2;
      pointer2 = (*pointer1).next;
    }
  }
}

void delete_channel(struct channel_list *list, char *channel_name_to_delete){
  struct channel_node *pointer1 = (*list).head;
  if (strcmp(channel_name_to_delete, (*(*pointer1).channel_data).name) == 0){
    (*list).head = (*pointer1).next;
    free(pointer1);
  }
  else{
    struct channel_node *pointer2 = (*pointer1).next;
    while (pointer2 != NULL){
      if (strcmp(channel_name_to_delete, (*(*pointer2).channel_data).name) == 0){
        (*pointer1).next = (*pointer2).next;
        free(pointer2);
      }
      pointer1 = pointer2;
      pointer2 = (*pointer1).next;
    }
  }
}

void print_node_value(struct node *node_to_print){
  chilog(INFO,"%s\n", (*(*node_to_print).connected_user).nick);
}
