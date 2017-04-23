#include "parallelCuckooHashMap.h"

cuckooHashTable* cuckoohashtable;

cuckooHashTable* createHashTable(int num_buckets){

    cuckooHashTable* hashtable = (cuckooHashTable *) malloc(sizeof(cuckooHashTable));
    hashtable->buckets = (bucketNode *) malloc(num_buckets * sizeof(bucketNode));

    int i = 0, j = 0;
    for(; i<num_buckets; i++){
        hashtable->buckets[i].firstNode = (entryNode*)malloc(NUM_SLOTS * sizeof(entryNode));
        pthread_mutex_init(&(hashtable->buckets[i].bucketLock),NULL);
        for(j = 0; j<NUM_SLOTS; j++){
            hashtable->buckets[i].firstNode[j].key = NULL;
            hashtable->buckets[i].firstNode[j].value = NULL;
        }
    }

    hashtable->num_buckets = num_buckets;
    pthread_rwlock_init(&(hashtable->hashTableLock),NULL);
    return hashtable;
}

void computehash(char* key, uint32_t *h1, uint32_t *h2){
    hashlittle2((void *)key, strlen(key), h1, h2);
}

void printHashTable(void){
    //printf("Print Hash Table\n");
    int buckets = cuckoohashtable->num_buckets;
    int i,j;
    bucketNode *newNode;

    for(i=0; i<buckets; i++){
         newNode = &(cuckoohashtable->buckets[i]);
         for(j=0; j<NUM_SLOTS;j++){
             if(newNode->firstNode[j].key != NULL){
                 printf("%s ",newNode->firstNode[j].key);
             }
         }
    }
    printf("\n\n");
}

char* get(char* key){
    pthread_rwlock_rdlock(&(cuckoohashtable->hashTableLock));
    uint32_t h1 = 0, h2 = 0;
    entryNode* temp;
    int i;    

    computehash(key, &h1, &h2);
    //printf("GET H1: %d, H2: %d, key: %s\n", h1, h2, key);
    
    h1 = h1 % cuckoohashtable->num_buckets;
    pthread_mutex_lock(&(cuckoohashtable->buckets[h1].bucketLock));
    temp = cuckoohashtable->buckets[h1].firstNode;    
    for(i = 0; i< NUM_SLOTS; i++){
       if(temp[i].key != NULL && !strcmp(temp[i].key, key)){
           //printf("Get : Key : %s at [%d][%d]\n", temp[i].key, h1, i);
           pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
           pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock));
           return temp[i].value;
       }
    } 
    pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
    
    h2 = h2 % cuckoohashtable->num_buckets;
    pthread_mutex_lock(&(cuckoohashtable->buckets[h2].bucketLock));
    temp = cuckoohashtable->buckets[h2].firstNode;    
    for(i = 0; i< NUM_SLOTS; i++){
       if(temp[i].key != NULL && !strcmp(temp[i].key, key)){
          //printf("Get : Key : %s at [%d][%d]\n", temp[i].key, h2, i);
          pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
          pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock));
          return temp[i].value;
       }
    }     
    pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
    
    pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock));
    printf("Get : NOT Found key : %s\n", key);
    return NULL;
}

bool removeKey(char* key){
    pthread_rwlock_rdlock(&(cuckoohashtable->hashTableLock));
    uint32_t h1 = 0, h2 = 0;
    entryNode* temp;
    int i;    

    computehash(key, &h1, &h2);
    
    h1 = h1 % cuckoohashtable->num_buckets;
    pthread_mutex_lock(&(cuckoohashtable->buckets[h1].bucketLock));
    temp = cuckoohashtable->buckets[h1].firstNode;    
    for(i = 0; i< NUM_SLOTS; i++){
       if(temp[i].key != NULL && !strcmp(temp[i].key, key)){
           free(temp[i].key);
           free(temp[i].value);
           temp[i].key = NULL;
           temp[i].value = NULL;
           pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
           pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock));
           return true;
       }
    } 

    h2 = h2 % cuckoohashtable->num_buckets;
    pthread_mutex_lock(&(cuckoohashtable->buckets[h2].bucketLock));
    temp = cuckoohashtable->buckets[h2].firstNode;    
    for(i = 0; i< NUM_SLOTS; i++){
       if(temp[i].key != NULL && !strcmp(temp[i].key, key)){
           free(temp[i].key);
           free(temp[i].value);
           temp[i].key = NULL;
           temp[i].value = NULL;
           pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
           pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock));
           return true;
       }
    }
    
    pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
    pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
    pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock));
    return false;
}

/*void freeHashTable(cuckooHashTable* newHashTable){
    int i,j;

    for(i=0; i<newHashTable->num_buckets; i++){
        for(j=0;j<NUM_SLOTS;j++){
            if(newHashTable->buckets[i].firstNode[j].key!=NULL){
                free(newHashTable->buckets[i].firstNode[j].key);
            }
            if(newHashTable->buckets[i].firstNode[j].value!=NULL){
                free(newHashTable->buckets[i].firstNode[j].value);
            }
        }
        free(newHashTable->buckets[i].firstNode);
    }
    free(newHashTable->buckets);
    free(newHashTable);
    return;
}*/

entryNode* _put(char *key, char *value, int thread_id){
    int num_iterations = 0;
    uint32_t h1 = 0, h2 = 0;
    entryNode *first, *second;
    entryNode evictentry;
    unsigned int t = (unsigned)thread_id;
    int i;

    char* temp_key = (char *) malloc(sizeof(char) * MAX_SIZE);
    char* temp_value = (char *) malloc(sizeof(char) * MAX_SIZE);
    char* curr_key = (char *) malloc(sizeof(char) * MAX_SIZE);
    char* curr_value = (char *) malloc(sizeof(char) * MAX_SIZE);
    strcpy(curr_key, key);
    strcpy(curr_value,value);

    srand48((unsigned)t);
    while(num_iterations < MAX_ITERATIONS){
        //printf("Num Iterations %d for key %s\n", num_iterations, key);
        num_iterations++;
        computehash(curr_key, &h1, &h2);
        //printf("PUT H1: %d, H2: %d, key: %s\n", h1, h2, curr_key);

        h1 = h1 % cuckoohashtable->num_buckets;
        first = cuckoohashtable->buckets[h1].firstNode;    
        h2 = h2 % cuckoohashtable->num_buckets;
        second = cuckoohashtable->buckets[h2].firstNode;

        if(h1>h2){
            uint32_t temp=h1; 
            h1=h2;
            h2=temp;
        }
       
        //printf("_put: Waiting for h1: %d lock thread_id : %d \n", h1, thread_id);
        pthread_mutex_lock(&(cuckoohashtable->buckets[h1].bucketLock));
        //printf("_put: acquired for h1: %d lock thread_id : %d \n", h1, thread_id);
        for(i = 0; i< NUM_SLOTS; i++){
            if(first[i].key == NULL){
                first[i].key = (char *) malloc(sizeof(char) * MAX_SIZE);
                first[i].value = (char *) malloc(sizeof(char) * MAX_SIZE);
                strcpy(first[i].key, curr_key);
                strcpy(first[i].value, curr_value);
                //printf("First: Inserted Key %s in bucket %d, %d\n", key, h1, i); 
                pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
                //printf("_put: Released h1: %d\n", h1);
                return NULL;
            }
            else if(!strcmp(first[i].key,curr_key)){
                strcpy(first[i].value, curr_value);
                pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
                printf("_put: Released h1: %d\n", h1);
                return NULL;
            }
        }
       
        
        if(h1!=h2){
            //printf("_put: Waiting for h2:%d lock thread_id: %d\n", h2, thread_id);
            pthread_mutex_lock(&(cuckoohashtable->buckets[h2].bucketLock));
            //printf("_put: Acquired for h2:%d lock thread_id: %d\n", h2, thread_id);
            for(i = 0; i< NUM_SLOTS; i++){
                if(second[i].key == NULL){
                    second[i].key = (char *) malloc(sizeof(char) * MAX_SIZE);
                    second[i].value = (char *) malloc(sizeof(char) * MAX_SIZE);
                    strcpy(second[i].key, curr_key);
                    strcpy(second[i].value, curr_value);
                    //printf("Second: Inserted Key %s in bucket %d, %d\n", key, h2, i);
                    pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
                    pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
                    //printf("_put: Released h2: %d\n", h2);
                    return NULL;
                }
                else if(!strcmp(second[i].key,curr_key)){
                    strcpy(second[i].value, curr_value);
                    pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
                    pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
                    //printf("_put: Released h2: %d\n", h2);
                    return NULL;
                }
            }
        }

        int index = rand_r(&t) % (2*NUM_SLOTS);

        if(index >= 0 && index <NUM_SLOTS){
            evictentry = first[index];
        }
        else{
            evictentry = second[index - NUM_SLOTS];
        }

        //printf("Evicted Key %s with index %d\n", evictentry.key, index);
        strcpy(temp_key, evictentry.key);
        strcpy(temp_value, evictentry.value);

        strcpy(evictentry.key, curr_key);
        strcpy(evictentry.value, curr_value);
        //printHashTable();
        
        strcpy(curr_key, temp_key);
        strcpy(curr_value, temp_value);

        pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
        if(h1!=h2){
            pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
        }
    }
    
    entryNode* finalevictedNode = (entryNode *) malloc(sizeof(entryNode));
    finalevictedNode->key = curr_key;
    finalevictedNode->value = curr_value;
    return finalevictedNode;
}

void resize(int req_buckets, int thread_id){
    //printf("In Resize============================== num_buckets = %d thread_id = %d\n", cuckoohashtable->num_buckets, thread_id);
    //printf("resize: Waiting to lock, thread_id : %d \n", thread_id);
    pthread_rwlock_wrlock(&(cuckoohashtable->hashTableLock)); 
    //printf("resize: Acquired lock, thread_id : %d \n", thread_id);
    if(req_buckets<=cuckoohashtable->num_buckets){
        pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock)); 
        //printf("resize: Released lock no change, thread_id : %d \n", thread_id);
        return;
    }

    bucketNode* oldBuckets = cuckoohashtable->buckets;
    int old_num_buckets = cuckoohashtable->num_buckets;

    int i, j;
    entryNode *res, *bucketRow;

    while(true){
        cuckoohashtable->num_buckets*=2;
        cuckoohashtable->buckets = (bucketNode*)malloc(cuckoohashtable->num_buckets*sizeof(bucketNode));

        for(i=0;i<cuckoohashtable->num_buckets;i++){
            cuckoohashtable->buckets[i].firstNode = (entryNode*)malloc(NUM_SLOTS*sizeof(entryNode));
            pthread_mutex_init(&(cuckoohashtable->buckets[i].bucketLock),NULL);
            for(j=0;j<NUM_SLOTS;j++){
                cuckoohashtable->buckets[i].firstNode[j].key=NULL;
                cuckoohashtable->buckets[i].firstNode[j].value=NULL;
            }
        }

        for(i = 0; i < old_num_buckets; i++){
            bucketRow = oldBuckets[i].firstNode; 
            for(j=0; j < NUM_SLOTS; j++){
                if(bucketRow[j].key != NULL){
                    res = _put(bucketRow[j].key, bucketRow[j].value, thread_id);
                    if(res != NULL){
                        break;
                    }
                }
            }
            if(res != NULL)
                break;        
        }
        if(res != NULL){
            printf("RESIZE: Value evicted left behind: %s\n", res->key);
            //printf("Inside Resize: Free and Resize\n");
            //printHashTable();
            bucketNode* temp = cuckoohashtable->buckets;
            free(temp);
        }
        else{
            break;
        }
    }
    
    //printHashTable();
    pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock)); 
    free(oldBuckets);
    //printf("resize: Released lock, thread_id : %d \n", thread_id);
}

void put(char* key, char* value, int thread_id){
    //printf("\nput: Waiting for readlock thread_id: %d\n", thread_id);
    pthread_rwlock_rdlock(&(cuckoohashtable->hashTableLock)); 
    //printf("put: Acquired readlock thread_id %d\n\n", thread_id);
    entryNode* res = _put(key, value, thread_id);
   
    if(res != NULL){
        //printf("Value evicted left behind: %s\n", res->key);
        //printHashTable();
        int old_num_buckets = cuckoohashtable->num_buckets;
        pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock)); 
        resize(old_num_buckets*2, thread_id);
        pthread_rwlock_rdlock(&(cuckoohashtable->hashTableLock)); 
        _put(res->key, res->value, thread_id);
        //printHashTable();
    }
    //printf("Put a New value : %s\n", value);
    //printHashTable();
    pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock)); 
    //printf("put: Released Readlock %d\n", thread_id);
    return;
}

void *putthreadfunc(void *id){
    int thread_id = *(int *)id;

    char* keys = (char*)malloc(30*sizeof(char)); 
    char* value = (char*)malloc(30*sizeof(char)); 
 
    int i;
    for(i=0;i<10;i++){
        //printf("allocated key %d\n",i);
        snprintf(keys,10,"%d",i + (10*thread_id));
        //printf("Thread: %d key%d: %s\n", thread_id, i, keys);
        snprintf(value,20,"%s%s",keys,"values");
        //printf("value: %s\n",value);
        put(keys,value,thread_id);
        printf("put in entry %s by %d\n", keys, thread_id);
    }
    
    return NULL;
}

void *getthreadfunc(void *id){ 
    int thread_id = *(int *)id;
    char* keys = (char*)malloc(20*sizeof(char)); 
 
    int i;
    for(i=0;i<10;i++){
        snprintf(keys,10,"%d",i + (10*thread_id));
        printf("GET key: %s, value: %s, thread: %d\n", keys, get(keys), thread_id);
        if(get(keys)==NULL){
            get(keys);
        }
    }
    return NULL;
}


int main(void){
    cuckoohashtable = createHashTable(2);
    printf("createHashTable: num_buckets %d\n", cuckoohashtable->num_buckets);

    pthread_t putthreads[5];
    pthread_t getthreads[5];

    int count = 0;
    for(count = 0; count < 5; count ++){
    	pthread_create(&putthreads[count], NULL, putthreadfunc, (void *) &count); 
    }
     
    for(count = 0; count < 5; count ++){
	pthread_join(putthreads[count], NULL);
    }

  
    for(count = 0; count < 5; count ++){ 
	pthread_create(&getthreads[count], NULL, getthreadfunc, (void *) &count); 
    }

  	 
    for(count = 0; count < 5; count ++){
	pthread_join(getthreads[count], NULL);
    }

    printHashTable();
    printf("createHashTable: num_buckets %d\n", cuckoohashtable->num_buckets);

    printf("Program Completed\n");
    return 0;
}
