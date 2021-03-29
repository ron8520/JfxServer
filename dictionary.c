#include "dictionary.h"

//a helper function
int bit_read_only(int index, uint8_t* src_dict){
    if((src_dict[index/BYTE_SIZE] & (0X80 >> (index % BYTE_SIZE))) == 0){
        return 0;
    }else{
        return 1;
    }
}

void init_dictionary(uint8_t* src_dict, struct dictionary* dict_list){
    int current_index = 0;

    //Total segement is 256, so it only need to loop through 256 times
    for(int i = 0; i < DICTIONARY_SIZE ; i++){
        int bit = 0;
        int pos = 0;

        //Get the first 8 bit, to dertermine the segement length
        for(int j = 0; j < BYTE_SIZE; j++){
            //using % operation to get the which index of the byte should read
            //and using << or >> shift operation push the bit to that position
            //finally, use & operation get the value and using | to store into struct
            bit = bit_read_only(current_index + j, src_dict);
            pos = (LAST_BIT_POSITION - (j % BYTE_SIZE));
            bit = bit << pos;
            dict_list[i].size |= bit; 
        }

        current_index += BYTE_SIZE; 
        //similar step shows above
        //store each single bit into the struct dictionary
        for(int j = 0; j < dict_list[i].size; j++){
            bit = bit_read_only(current_index + j, src_dict);
            int index = j / BYTE_SIZE;
            pos = (LAST_BIT_POSITION - (j % BYTE_SIZE));
            bit = bit << pos;
            dict_list[i].content[index] |= bit;
        }
        current_index += dict_list[i].size;
    }
}

struct node* newNode(uint8_t key){
    struct node* new_node = malloc(sizeof(struct node));

    //initilize the value inside the pointer
    new_node->key = key;
    new_node->left = NULL;
    new_node->right = NULL;
    
    return new_node;
}

void init_tree(struct dictionary* dict_list, struct node* root){
    //use a cursor to store the current position
    struct node* cursor = root;

    for(int i = 0; i < DICTIONARY_SIZE; i++){
        uint8_t size = dict_list[i].size;
        //if the value of the bit is 0, move to left and create a newNode if not exist
        //if the value of the bit is 1, move to right and create a newNode if not exist
        //if it is the last bit of current dictionary, put current index into the next movement node.
       
       //each iteration start from the root and iterate each bit 
        cursor = root;
        for(int j = 0; j < size; j++){
            //determine which bucket store in the struct should go first
            int bit = 1;
            int content_index = j / BYTE_SIZE;
            int pos = (LAST_BIT_POSITION - (j % BYTE_SIZE));

            bit = bit << pos;
            bit = bit & dict_list[i].content[content_index];
            bit = bit >> pos;

            if(bit == 0){
                //bit equal 0 move to left
                if(cursor->left == NULL){
                    cursor->left = newNode(-1);

                }
                cursor = cursor->left;    
                
            }else{
                // bit equal 1 move to right
                if(cursor->right == NULL){
                    cursor->right = newNode(-1);

                }  
                cursor = cursor->right;   
            }
        }
        //set up the index into the last node after the iteration.
        cursor->key = i;
    }
}

uint8_t* server_compress(struct dictionary* dict_list, uint64_t prev_payload_length,
                            uint8_t* prev_payload, uint64_t* new_payload_length, uint8_t* new_payload){
    
    uint8_t padding = 0;
    int temp_payload_size = 500;
    new_payload = realloc(new_payload, sizeof(uint8_t) * temp_payload_size);
    memset(new_payload, 0, temp_payload_size);

    //count how many bit we wrote
    int counter = 0; 
    for(int i = 0; i < prev_payload_length; i++){
        uint8_t index =  prev_payload[i];
        for(int j = 0; j < dict_list[index].size; j++){
            int content_index = j / BYTE_SIZE;
            int pos = (LAST_BIT_POSITION - (j % BYTE_SIZE));

            //get the left-most bit first, then move back to the original place
            //for example, at the beginning the flag equal is 00001;
            //then the pos is 3, it will shift 3 bit first. move to 01000;
            //using & operation to get that bit value, and shift back to inital position
            
            int flag = 1;
            flag = flag << pos;
            flag = flag & dict_list[index].content[content_index];
            flag = flag >> pos;
             
            int bit_pos = (LAST_BIT_POSITION - (counter % BYTE_SIZE));
            flag = flag << bit_pos;
            new_payload[counter/BYTE_SIZE] |= flag;
            counter += 1;

            //if the size is not enough, double the size of the new_payload
            if(counter == temp_payload_size){
                temp_payload_size = temp_payload_size * 2;
                new_payload = realloc(new_payload, (sizeof(uint8_t) * temp_payload_size));
            }
        }
        
    }

    padding = BYTE_SIZE - counter % BYTE_SIZE;
    *new_payload_length = counter / BYTE_SIZE;

    //remainder equal 0
    if(padding == BYTE_SIZE){
        new_payload[counter / BYTE_SIZE] = 0X00;
    }else{
        int padding_index = (counter / BYTE_SIZE) + 1;
        new_payload[padding_index] = padding;
        
        *new_payload_length += 1;
    }
    
    //extra 0X00 at the end
    *new_payload_length += 1;

    return new_payload;
}

uint8_t* server_decompress(struct node* root, uint64_t prev_payload_length,
                                uint8_t* prev_payload, uint64_t* new_payload_length, uint8_t* new_payload){
    struct node* cursor = root;
    uint8_t padding = prev_payload[prev_payload_length - 1];
    int total_bit = (prev_payload_length - 1) * BYTE_SIZE - padding;

    for(int i = 0; i < total_bit; i++){
        int bit_pos = i % BYTE_SIZE;
        int bit_index = i / BYTE_SIZE;
        //start from the left most
        int bit = (prev_payload[bit_index] >> (LAST_BIT_POSITION - bit_pos)) & 0x01;
        
        //if the bit value is 0, then the cursor move left
        if(bit == 0){
            cursor = cursor->left;

        }else{
            //else move right
            cursor = cursor->right;
        }

        //when both left and right is NULL, it mean it reach the end
        //need to reset the cursor to the root
        if(cursor->left == NULL && cursor->right == NULL){
            new_payload = realloc(new_payload, sizeof(uint8_t) * (*new_payload_length + 1));
            new_payload[*new_payload_length] = cursor->key;
            *new_payload_length += 1;
            cursor = root;
        }
    }

    return new_payload;
}

void free_node(struct node* root){
    //recruse the right first
    if(root->right != NULL){
        free_node(root->right);
    }
    //then recruse the left
    if(root->left != NULL){
        free_node(root->left);
    }

    free(root);
}