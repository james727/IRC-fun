#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {
  char *name_pointer;
  struct node *next;
};

struct linked_list {
  struct node *head;
};

struct linked_list create_new_list(void);
void insert_element(char *name, struct linked_list *list);
void print_list(struct linked_list list_to_print);
void print_node_value(struct node *node_to_print);
int search(struct linked_list list, char *search_term);
void delete_element(struct linked_list list, char *term_to_delete);
