#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "log.h"

struct linked_list create_new_list(void){
  struct linked_list list;
  list.head = NULL;
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

int search(struct linked_list list, char search_term[]){
  if (list.head == NULL) {
    return 0;
  }
  else {
    struct node *current = list.head;
    while (current != NULL){
      if (strcmp(search_term, (*(*current).connected_user).nick) == 0){
        return 1;
      }
      current = (*current).next;
    }
    return 0;
  }
}

void delete_element(struct linked_list *list, char *name_to_delete){
  struct node *pointer1 = (*list).head;
  if (strcmp(name_to_delete, (*(*pointer1).connected_user).nick) == 0){
    chilog(INFO, "Name found for deletion\n");
    (*list).head = (*pointer1).next;
    free(pointer1);
  }
  else{
    struct node *pointer2 = (*pointer1).next;
    while (pointer2 != NULL){
      if (strcmp(name_to_delete, (*(*pointer2).connected_user).nick) == 0){
        chilog(INFO, "Name found for deletion\n");
        (*pointer1).next = (*pointer2).next;
        free(pointer2);
      }
      pointer1 = pointer2;
      pointer2 = (*pointer1).next;
    }
  }
}

void print_node_value(struct node *node_to_print){
  /*printf("%s\n", (*node_to_print).name_pointer);*/
  chilog(INFO,"%s\n", (*(*node_to_print).connected_user).nick);
}