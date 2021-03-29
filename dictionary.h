#ifndef __DICTIONARY_H__
#define __DICTIONARY_H__

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTE_SIZE 8
#define LAST_BIT_POSITION 7
#define DICTIONARY_SIZE 256

/*
* this sturct store the information read from compression.dict
* struct dictionary has two variable
* uint8_t size used to store the total bit 
* uint8_t content[4] relevant bit 
*/
struct dictionary{
    uint8_t size;
    uint8_t content[4];  
};

/*
* struct node is a binary tree, is used for decompress
* uint8_t key store the index of the node
* struct node* left, right are two pointer
*/
struct node{
    struct node* left;
    struct node* right;
    uint8_t key;
};

struct node* newNode(uint8_t key);    

void init_tree(struct dictionary* dict_list, struct node* root);

void init_dictionary(uint8_t* src_dict, struct dictionary* dict_list);

int bit_read_only(int index, uint8_t* src_dict);

uint8_t* server_compress(struct dictionary* dict_list, uint64_t prev_payload_length,
                            uint8_t* prev_payload, uint64_t* new_payload_length, uint8_t* new_payload);

uint8_t* server_decompress(struct node* root, uint64_t prev_payload_length,
                            uint8_t* prev_payload, uint64_t* new_payload_length, uint8_t* new_payload);

void free_node(struct node* root);

#endif