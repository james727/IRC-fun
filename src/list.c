#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"

/*int main(){
  struct linked_list test_list;
  test_list = create_new_list();
  char name1[6] = "james";
  char name2[4] = "dan";
  char name3[8] = "1234567";
  insert_element(name1, &test_list);
  insert_element(name2, &test_list);
  insert_element(name3, &test_list);
  print_list(test_list);
  printf("Search for term 'abc': %d\n", search(test_list, "abc"));
  printf("Search for term 'dan': %d\n", search(test_list, "dan"));
  delete_element(test_list, name2);
  print_list(test_list);
  printf("Search for term 'dan': %d\n", search(test_list, "dan"));

}*/

struct linked_list create_new_list(void){
  struct linked_list list;
  list.head = NULL;
  return list;
}

void insert_element(char name[], struct linked_list *list){
  /* initialize node */
  struct node *name_node = malloc(sizeof(struct node));
  name_node -> name_pointer = malloc(strlen(name));
  strcpy((*name_node).name_pointer, name);

  /* prepend to list */
  (*name_node).next = (*list).head;
  (*list).head = name_node;
}

void print_list(struct linked_list list_to_print){
  if (list_to_print.head == NULL) {
    printf("empty list\n");
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
      if (strcmp(search_term, (*current).name_pointer) == 0){
        return 1;
      }
      current = (*current).next;
    }
    return 0;
  }
}

void delete_element(struct linked_list list, char name_to_delete[]){
  struct node *pointer1 = list.head;
  if (strcmp(name_to_delete, (*pointer1).name_pointer) == 0){
    list.head = (*pointer1).next;
    free((*pointer1).name_pointer);
    free(pointer1);
  }
  else{
    struct node *pointer2 = (*pointer1).next;
    while (pointer2 != NULL){
      if (strcmp(name_to_delete, (*pointer2).name_pointer) == 0){
        (*pointer1).next = (*pointer2).next;
        free((*pointer2).name_pointer);
        free(pointer2);
      }
      pointer1 = pointer2;
      pointer2 = (*pointer1).next;
    }
  }
}

void print_node_value(struct node *node_to_print){
  printf("%s\n", (*node_to_print).name_pointer);
}
