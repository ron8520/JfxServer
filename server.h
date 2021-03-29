#ifndef __SERVER_H__
#define __SERVER_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdint.h>
#include <inttypes.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <sys/epoll.h>
#include "dictionary.h"


#define BUFFER_SIZE 9
#define MAX_EVENT 10
#define RESPON_SIZE 1 
#define FILE_SIZE 256
#define MAX_FILE_SIZE 1024
#define PAYLOAD_LENGTH 8
#define ID_LENGTH 4
#define LAST_INDEX 8

/*
* A struct store all the file desciptor
*/
struct information{
  int socket_fd;
  int client_fd;
  int epoll_fd;
};

/*
* A struct store all the information that need to use in 
* thread method
*/
struct send_request{
    int client_fd;
    uint8_t buffer[BUFFER_SIZE];
    uint8_t* payload;
    uint64_t payload_size;
};

/*
* A struct is used to store all the file information,
* such as session_id (int type), filename (char* type)
* offset (uint64_t), range(uint64_t range) 
*/
struct retrive_file{
    int session_id;
    char* file_name;
    uint64_t offset;
    uint64_t range;
    pthread_t tid;
};

/*
* A struct store all retrive_file
*/
struct retrive_file_list{
    struct retrive_file* retrive_list;
    int size;
};


void server_operation(struct information* info, struct retrive_file_list* file_list, 
                            char* file_path, struct dictionary* dict_list, struct node* root);

uint8_t* count_regular_file(char* path, uint8_t* current_payload, uint64_t* counter);

void* handle_function(void* args);

void endian_swap(uint8_t* array, int size);

#endif