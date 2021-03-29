#include "server.h"

//epoll implement reference
//https://blog.csdn.net/xiajun07061225/article/details/9250579
//https://blog.csdn.net/ljx0305/article/details/4065058
//https://stackoverflow.com/questions/27247/could-you-recommend-some-guides-about-epoll-on-linux

//build up binary tree reference
//https://www.geeksforgeeks.org/binary-tree-set-1-introduction/
//https://stackoverflow.com/questions/9181146/freeing-memory-of-a-binary-tree-c

void endian_swap(uint8_t* array, int size){
    //end is the last index of the array;
    int end = size - 1;
    uint8_t temp;
    for(int i = 0; i < end; i++){
        temp = array[i];
        array[i] = array[end];
        array[end] = temp;
        end--;
    }
}

uint8_t* count_regular_file(char* path, uint8_t* current_payload, uint64_t* counter){
    DIR *dir = opendir(path);
    struct dirent *file;
    
    //loop through the whole directory
    while( (file = readdir(dir)) != NULL ){
        if(file->d_type == DT_REG){
            for(int i = 0; i < strlen(file->d_name); i++){
                current_payload = realloc(current_payload, sizeof(uint8_t) * ((*counter) + 1));
                current_payload[*counter] = file->d_name[i];
                (*counter) += 1;
            }

            //add the null ternimate between each filename
            current_payload = realloc(current_payload, sizeof(uint8_t) * ((*counter) + 1));
            current_payload[*counter] = 0x00;
            (*counter) += 1;
        }
    }
    closedir(dir);
    return current_payload;
}


void server_operation(struct information* info, struct retrive_file_list* file_list, 
                        char* file_path, struct dictionary* dict_list, struct node* root){

    uint8_t buffer[BUFFER_SIZE];
    uint8_t msg_error[BUFFER_SIZE];
    uint8_t payload_byte[PAYLOAD_LENGTH];
    uint8_t null_respon[RESPON_SIZE] = {0x00};
    bzero(msg_error, sizeof(msg_error));
    bzero(buffer, sizeof(buffer));
    bzero(payload_byte ,0);
    msg_error[0] = 0xf0;

    //if the recv() <= 0, mean the connection error or close.
    //then close the fd and delete the fd from inerest list
    if (recv(info->client_fd, buffer, sizeof(buffer), 0) <= 0){
        epoll_ctl(info->epoll_fd, EPOLL_CTL_DEL, info->client_fd, NULL);
        close(info->client_fd);    
        return;
    }
    //right shift 4 bit to get the first four bit of the byte
    uint8_t type_bit = buffer[0] >> 4;
    uint8_t compression_bit = buffer[0] & 0b00001000;
    uint8_t req_comp_bit = buffer[0] & 0b00000100;

    if(req_comp_bit == 4){
        buffer[0] = buffer[0] | 0b00001000;
        buffer[0] = buffer[0] & 0b11111000; // get rid of 6th bit

    }else if(compression_bit == 8 && req_comp_bit == 0){
        buffer[0] = buffer[0] & 0b11110000;  
    }
    

    //cover networkbyte order to little endian to get the len
    for(int i = 1; i < BUFFER_SIZE; i++){
        payload_byte[PAYLOAD_LENGTH-i] = buffer[i];
    }

    uint64_t payload_len = *((uint64_t *) payload_byte);

    if(type_bit == 0x0){
        //set the respon header is 0x1;
        buffer[0] = buffer[0] | 0b00010000;

        //echo is different with other command
        //Only need to change the bit when the 5th and 6th is different. 
        if(compression_bit == 8){
            buffer[0] = 0x18;
        }
        
        uint8_t* payload = malloc(payload_len);
        memset(payload, 0, payload_len);
        recv(info->client_fd, payload, payload_len, 0);

        //if the respon need compress
        if (req_comp_bit == 4 && compression_bit == 0){

            uint8_t* new_payload = NULL;
            uint64_t new_payload_length = 0;
            new_payload = server_compress(dict_list, payload_len, payload, 
                                            &new_payload_length, new_payload);

            uint8_t new_payload_size[PAYLOAD_LENGTH];
            bzero(new_payload_size, 0);
            memcpy(new_payload_size, &new_payload_length, sizeof(uint64_t));

            //change the endian to network byte order
            for(int i = 0; i < PAYLOAD_LENGTH; i++){
                buffer[LAST_INDEX-i] = new_payload_size[i];
            }
            send(info->client_fd, buffer, sizeof(buffer), 0);
            send(info->client_fd, new_payload, new_payload_length, 0);

            free(new_payload);
            
            
        }else{
            send(info->client_fd, buffer, sizeof(buffer), 0);
            send(info->client_fd, payload, payload_len, 0);
        }
        free(payload);
    
    }
    else if (type_bit == 0x2){
        //set the header respon to 0x3
        buffer[0] = buffer[0] | 0b00110000;

        uint64_t counter = 0;
        uint8_t* current_payload = NULL;
        current_payload = count_regular_file(file_path, current_payload, &counter);

        // directory is empty, send the zero back to client
        if(counter == 0){
            send(info->client_fd, buffer, sizeof(buffer), 0);
            send(info->client_fd, null_respon, sizeof(null_respon), 0);
            return;
        }

        //if need to compress and send back
        if(req_comp_bit == 4){
            uint8_t* compress_payload = NULL;
            uint64_t compress_payload_length = 0;
            compress_payload = server_compress(dict_list, counter, current_payload, 
                                                    &compress_payload_length, compress_payload);
            
            //cover uint64_t into uint8_t [8]
            uint8_t compress_payload_size[PAYLOAD_LENGTH];
            bzero(compress_payload_size, 0);
            memcpy(compress_payload_size, &compress_payload_length, sizeof(uint64_t));
            
            //change the endian to network byte order
            for(int i = 0; i < PAYLOAD_LENGTH; i++){
                buffer[LAST_INDEX-i] = compress_payload_size[i];
            }

            send(info->client_fd, buffer, sizeof(buffer), 0);
            send(info->client_fd, compress_payload, compress_payload_length, 0);

            free(compress_payload);
            free(current_payload);
            return;
        }
        
        uint8_t current_payload_size[PAYLOAD_LENGTH];
        bzero(current_payload_size, 0);
        memcpy(current_payload_size, &counter, sizeof(uint64_t));

        //swap the byte to the network byte order
        for(int i = 0; i < PAYLOAD_LENGTH; i++){
            buffer[LAST_INDEX-i] = current_payload_size[i];
        }

        send(info->client_fd, buffer, sizeof(buffer), 0);
        send(info->client_fd, current_payload, counter, 0);
        free(current_payload);
        
    }
    else if (type_bit == 0x4){
        //change the respon header is 0x5;
        buffer[0] = buffer[0] | 0b01010000;
        
        char* filename = malloc(payload_len);
        recv(info->client_fd, filename, payload_len, 0);

        //using strcat to get full file path
        char* target_file = strdup(file_path);
        target_file = realloc(target_file, (strlen(file_path) + payload_len + 2));
        target_file = strcat(target_file, "/");
        target_file = strcat(target_file, filename);

        FILE *file = fopen(target_file, "r");

        //if the file not exist, send the error message back and close the conection
        if(!file){
            send(info->client_fd, msg_error, sizeof(msg_error), 0);
            close(info->client_fd);
            free(filename);
            free(target_file);
            return;
        }

        //using st.size to get the file size;
        struct stat st;
        stat(target_file, &st);
        uint64_t file_size = st.st_size;
        uint8_t new_file_size [PAYLOAD_LENGTH];
        bzero(new_file_size, 0);
        memcpy(new_file_size, &file_size, sizeof(uint64_t));
        fclose(file);
        
        //endian swap from little to network byte order
        endian_swap(new_file_size, PAYLOAD_LENGTH);

        //compress the payload
        if (req_comp_bit == 4){
            uint8_t* compress_payload = NULL;
            uint64_t compress_payload_length = 0;
            compress_payload = server_compress(dict_list, PAYLOAD_LENGTH, new_file_size,
                                                &compress_payload_length, compress_payload);

            uint8_t compress_payload_size[PAYLOAD_LENGTH];
            bzero(compress_payload_size, 0);
            memcpy(compress_payload_size, &compress_payload_length, sizeof(uint64_t));

            for(int i = 0; i < PAYLOAD_LENGTH; i++){
                buffer[LAST_INDEX-i] = compress_payload_size[i];
            }

            send(info->client_fd, buffer, sizeof(buffer), 0);
            send(info->client_fd, compress_payload, compress_payload_length, 0);

            free(filename);
            free(compress_payload);
            return;
        }

        //if the payload don't need to compress
        //change the last index of the buffer to 8
        //it mean the payload behind the header is 8 byte
        buffer[LAST_INDEX] = PAYLOAD_LENGTH;
        send(info->client_fd, buffer, sizeof(buffer), 0);
        send(info->client_fd, new_file_size, sizeof(new_file_size), 0);
        free(filename);
        
    }
    else if (type_bit == 0x6){
        //set the respon header is 0x7;  
        buffer[0] = buffer[0] | 0b01110000;
        
        //delcare and initilze array
        pthread_t thread_id;
        uint8_t empty_payload[PAYLOAD_LENGTH];
        uint8_t recv_id[ID_LENGTH];
        uint8_t recv_offset[PAYLOAD_LENGTH];
        uint8_t recv_range[PAYLOAD_LENGTH];
        bzero(recv_id, 0);
        bzero(recv_offset, 0);
        bzero(recv_range, 0);
        bzero(empty_payload, 0);

        uint8_t* current_payload = malloc(payload_len);
        recv(info->client_fd, current_payload, payload_len, 0);

        //if the compression_bit is 1, then need to do decompress first
        //if the 5th bit is 1, then the demical value is 8
        if(compression_bit == 8){
            uint8_t* decompress_payload = NULL;
            uint64_t decompress_payload_length = 0;
            decompress_payload = server_decompress(root, payload_len, current_payload,
                                                        &decompress_payload_length, decompress_payload);
            //copy the decompress result to current_payload
            payload_len = decompress_payload_length;
            current_payload = realloc(current_payload, sizeof(uint8_t) * decompress_payload_length);
            memset(current_payload, 0, decompress_payload_length);
            memcpy(current_payload, decompress_payload, decompress_payload_length);
            free(decompress_payload);
        }

        //using memcpy to copy all the value to the array that delcared above
        memcpy(recv_id, current_payload, ID_LENGTH);
        memcpy(recv_offset, &current_payload[ID_LENGTH], PAYLOAD_LENGTH);
        memcpy(recv_range, &current_payload[ID_LENGTH + PAYLOAD_LENGTH], PAYLOAD_LENGTH);
        
        //big endian swap to little endian
        endian_swap(recv_range, PAYLOAD_LENGTH);
        endian_swap(recv_offset, PAYLOAD_LENGTH);

        //20 is the size of total byte before the file name
        //minus it to get the the file name size;
        //however, the payload does not contain the null terminate.
        //need to malloc one more extra space to place in the null terminate. 
        //and start get the filename from index 20
        int file_name_size = payload_len - 20;
        char* filename = malloc(file_name_size + 1);
        bzero(filename, 0);
        memcpy(filename, &current_payload[20], file_name_size);
        filename[file_name_size] = '\0';

        //Using strcat and strdup to get the full file path
        char* target_file = strdup(file_path);
        target_file = realloc(target_file, (strlen(file_path) + file_name_size + 2));
        target_file = strcat(target_file, "/");
        target_file = strcat(target_file, filename);
        free(filename);
        free(current_payload);

        //check the file is exist or not
        //if not, send the error message back.
        FILE* file = fopen(target_file, "rb");
        if(!file){
            send(info->client_fd, msg_error, sizeof(msg_error), 0);
            close(info->client_fd);
            free(target_file);
            return;
        }

        //using the same method in 0x4 command, to get the file size
        //and cover all the uint8_t array to uint64_t variable
        struct stat st;
        stat(target_file, &st);
        uint64_t file_size = st.st_size;
        uint64_t file_range_recv = *((uint64_t *) recv_range);
        int session_id = *((int*) recv_id);
        uint64_t offset = *((uint64_t*) recv_offset);

        // check the require range is out of the file size or not
        // if it is, send the error message back. 
        if(file_range_recv + offset > file_size){
            send(info->client_fd, msg_error, sizeof(msg_error), 0);
            close(info->client_fd);
            free(target_file);
            return;
        }

        //if the retive_list is empty
        if (file_list->retrive_list == NULL){
            file_list->retrive_list = realloc(file_list->retrive_list, 
                sizeof(struct retrive_file) * (file_list->size + 1));
            
            file_list->retrive_list[file_list->size].offset = offset;
            file_list->retrive_list[file_list->size].range = file_range_recv;
            file_list->retrive_list[file_list->size].session_id = session_id;
            file_list->retrive_list[file_list->size].file_name = strdup(target_file);
            file_list->retrive_list[file_list->size].tid = thread_id;
            file_list->size += 1;
        }else{
            //loop through the list to dertermine the file is transfering to client or not
            int client_find = 0;
            for(int i = 0; i < file_list->size; i++){
                if(file_list->retrive_list[i].session_id == session_id){

                    //same session_id but different file, or different requested file range
                    if ((strcmp(file_list->retrive_list[i].file_name, target_file) != 0)
                        || (file_list->retrive_list[i].range != file_range_recv)){
                            
                            //To dertermine the status of the thread
                            int pthread_signal;
                            pthread_signal = pthread_kill(file_list->retrive_list[i].tid, 0);

                            //no thread currently working for this session_id
                            //server accept this as new request and add it to the list
                            if(pthread_signal != 0){
                                file_list->retrive_list = realloc(file_list->retrive_list, 
                                            sizeof(struct retrive_file) * (file_list->size + 1));
            
                                file_list->retrive_list[file_list->size].offset = offset;
                                file_list->retrive_list[file_list->size].range = file_range_recv;
                                file_list->retrive_list[file_list->size].session_id = session_id;
                                file_list->retrive_list[file_list->size].file_name = strdup(target_file);
                                file_list->retrive_list[file_list->size].tid = thread_id;
                                file_list->size += 1;

                            }else{
                                //if the thread still alive, it means still sending the data
                                //it should be invaild.
                                send(info->client_fd, msg_error, sizeof(msg_error), 0);
                                close(info->client_fd);
                                free(target_file);
                                return;
                            }

                        }else{

                            //To dertermine the status of the thread
                            int pthread_signal;
                            pthread_signal = pthread_kill(file_list->retrive_list[i].tid, 0);


                            if(pthread_signal == 0){
                                buffer[0] = 0x70;
                                for(int i = 0; i < PAYLOAD_LENGTH; i++){
                                    buffer[i+1] = empty_payload[i];
                                }
                                send(info->client_fd, buffer, sizeof(buffer), 0);
                                free(target_file);
                                return;
                            }else{
                                send(info->client_fd, msg_error, sizeof(msg_error), 0);
                                close(info->client_fd);
                                free(target_file);
                                return;
                            }
                        }

                    client_find = 1;
                    break;
                }
            }

            //if session id not in the list, accept this as the new request
            if(!client_find){
                file_list->retrive_list = realloc(file_list->retrive_list, 
                                            sizeof(struct retrive_file) * (file_list->size + 1));
            
                file_list->retrive_list[file_list->size].offset = offset;
                file_list->retrive_list[file_list->size].range = file_range_recv;
                file_list->retrive_list[file_list->size].session_id = session_id;
                file_list->retrive_list[file_list->size].file_name = strdup(target_file);
                file_list->retrive_list[file_list->size].tid = thread_id;
                file_list->size += 1;
            }
        }

        //using fseek to set the file cursor to the specifc offset
        if (fseek(file, offset, SEEK_SET) != 0){
            printf("move the file descriptor fail\n");
        }

        //malloc a char* to store all the content from the file
        char* target_content = malloc(file_range_recv + 1);
        fread(target_content, file_range_recv, 1, file);
        fclose(file);


        //little endian swap back to big endian
        endian_swap(recv_range, PAYLOAD_LENGTH);
        endian_swap(recv_offset, PAYLOAD_LENGTH);
        
        //combine all the information into one payload
        uint64_t current_payload_size = 20 + file_range_recv;
        uint8_t* cur_payload = malloc(sizeof(uint8_t) * current_payload_size);
        memcpy(cur_payload, recv_id, ID_LENGTH);
        memcpy(&cur_payload[ID_LENGTH], recv_offset, PAYLOAD_LENGTH);
        memcpy(&cur_payload[ID_LENGTH + PAYLOAD_LENGTH], recv_range, PAYLOAD_LENGTH);
        memcpy(&cur_payload[ID_LENGTH + (PAYLOAD_LENGTH * 2)], target_content, file_range_recv);
        
        //initilze a struct for passing into thread method
        struct send_request* data = malloc(sizeof(struct send_request));
        data->client_fd = info->client_fd;

        //if need to compress, do the compress first, then memcpy to the send_request struct
        if(req_comp_bit == 4){
            uint8_t* compress_payload = NULL;
            uint64_t compress_payload_length = 0;
            compress_payload = server_compress(dict_list, current_payload_size, cur_payload,
                                                &compress_payload_length, compress_payload);
            uint8_t compress_payload_size[PAYLOAD_LENGTH];
            bzero(compress_payload_size, 0);
            memcpy(compress_payload_size, &compress_payload_length, sizeof(uint64_t));

            //swap buffer endian
            for(int i = 0; i < PAYLOAD_LENGTH; i++){
                buffer[LAST_INDEX-i] = compress_payload_size[i];
            }

            data->payload_size = compress_payload_length;
            data->payload = malloc(sizeof(uint8_t) * compress_payload_length);
            memcpy(data->buffer, buffer, BUFFER_SIZE);
            memcpy(data->payload, compress_payload, compress_payload_length);
            
        }else{
            uint8_t new_payload[PAYLOAD_LENGTH];
            bzero(new_payload, 0);
            memcpy(new_payload, &current_payload_size, sizeof(uint64_t));

            for(int i = 0; i < PAYLOAD_LENGTH; i++){
                buffer[LAST_INDEX-i] = new_payload[i];
            }

            //assign value to data pointer
            data->payload_size = current_payload_size;
            data->payload = malloc(sizeof(uint8_t) * current_payload_size);
            memcpy(data->buffer, buffer, BUFFER_SIZE);
            memcpy(data->payload, cur_payload, current_payload_size);
        }

        //creating a new thread to send the data back
        pthread_create(&thread_id, NULL, handle_function, (void*) data);
        free(cur_payload);
        free(target_file);
        free(target_content);
    
    }else if (type_bit == 0x8){  
        shutdown(info->socket_fd, SHUT_RD);
        free(info);

        for(int i = 0; i< file_list->size; i++){
            free(file_list->retrive_list[i].file_name);
        }

        //free the binary tree
        free_node(root);
        free(file_list->retrive_list);
        free(file_list);
        exit(0);
    }else{
        send(info->client_fd, msg_error, sizeof(msg_error),0);
        close(info->client_fd);
    }

}

void* handle_function(void* args){
    struct send_request* data = (struct send_request*) args;

    send(data->client_fd, data->buffer, BUFFER_SIZE, 0);
    send(data->client_fd, data->payload, data->payload_size, 0);
    free(data->payload);
    free(data);
    return NULL;
}

int main(int argc, char** argv){
    if(argc < 2)
        return 1;

    FILE *f = fopen(argv[1], "rb");
    FILE *dict = fopen("compression.dict", "rb");

    //if the file or the dictionary cannot open then exit the programm
    //or not given configure file
    if(!f || !dict) 
        return 1;
    
    //set up dictionary 
    fseek(dict, 0, SEEK_END);
    int dictionary_size = ftell(dict);
    fseek(dict, 0, SEEK_SET);
    uint8_t src_dict [MAX_FILE_SIZE];
    bzero(src_dict, 0);
    fread(src_dict, dictionary_size, 1, dict);

    struct dictionary dict_list[FILE_SIZE];
    for(int i = 0; i < FILE_SIZE; i++){
        dict_list[i].size = 0;
        memset(dict_list[i].content, 0, sizeof(dict_list[i].content));
    }
    init_dictionary(src_dict, dict_list);

    //set up binary tree
    struct node* root = newNode(-1);
    init_tree(dict_list, root);

    
    uint32_t ip;
    uint16_t port;
    char file_path[FILE_SIZE];
    int c, file_counter = 0;

    fread(&ip, sizeof(ip), 1, f);
    fread(&port, sizeof(port), 1, f);

    while((c = fgetc(f)) != EOF){
        file_path[file_counter] = c;
        file_counter += 1;
    }
    fclose(f);

    //add the null terminate at the end
    file_path[file_counter] = '\0'; 

    int client_fd, epoll_fd, socket_fd = -1;
    int option, running = 1;
    struct sockaddr_in local;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_fd < 0){
        puts("Fail to set up socket!\n");
        exit(1);
    }
    
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = ip;
    local.sin_port = port;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option, sizeof(int));

    if(bind(socket_fd, (struct sockaddr*) &local, sizeof(struct sockaddr_in))){
        puts("Bind fail!\n");
        exit(1);
    }

    listen(socket_fd, 100);

    //initialize epoll
    epoll_fd = epoll_create1(0);
    struct epoll_event event, events[MAX_EVENT];
    event.data.fd = socket_fd;
    event.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);

    //store all the nesscesary information to struct information
    struct information* info = malloc(sizeof(struct information));
    info->socket_fd = socket_fd;
    info->client_fd = 0;
    info->epoll_fd = epoll_fd;

    //initialize the struct retrive_file_list 
    struct retrive_file_list* file_list = malloc(sizeof(struct retrive_file_list));
    file_list->size = 0;
    file_list->retrive_list = NULL;

    while(running){
        //Get how many active fd.
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENT, -1);

        for(int i = 0; i < num_events; i++){
            //if current event fd is socket_fd then accept the new client
            //and add the new client fd to the interest list
            if(events[i].data.fd == socket_fd){
                uint32_t addrlen = sizeof(struct sockaddr_in);
                client_fd = accept(socket_fd, (struct sockaddr*) &local, &addrlen);
                
                //if accepting the new client fail, then break the loop
                if (client_fd < 0){
                    printf("Accept new client fail\n");
                    break;
                }
                event.data.fd = client_fd;
                event.events = EPOLLIN;

                //if adding new event to interest list fail, then break the loop
                if ((epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event)) == -1){
                    printf("Adding new connection to queue fail\n");
                    break;
                }

            }else if (events[i].events == EPOLLIN){
                //update the client fd 
                info->client_fd = events[i].data.fd;
                server_operation(info, file_list, file_path, dict_list, root);

            }else if (events[i].events == EPOLLHUP){
                //if the client shutdown from their side,
                //then it should be removed from the interest list.
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, &event);
                close(events[i].data.fd);
                break;
            }
        }

    }
    close(socket_fd);

    return 0;
}