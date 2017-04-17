#include "parallelCuckooHashMap.h"

cuckooHashTable* cuckoohashtable;

int compare(void *a, void *b){
    return *(int *) a - *(int *)b;
}

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
    printf("Print Hash Table\n");
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
}

char* get(char* key){
    pthread_rwlock_rdlock(&(cuckoohashtable->hashTableLock));
    uint32_t h1 = 0, h2 = 0;
    entryNode* temp;
    int i;    

    computehash(key, &h1, &h2);
    printf("GET H1: %d, H2: %d, key: %s\n", h1, h2, key);
    
    h1 = h1 % cuckoohashtable->num_buckets;
    pthread_mutex_lock(&(cuckoohashtable->buckets[h1].bucketLock));
    temp = cuckoohashtable->buckets[h1].firstNode;    
    for(i = 0; i< NUM_SLOTS; i++){
       if(temp[i].key != NULL && !strcmp(temp[i].key, key)){
           printf("Get : Key : %s at [%d][%d]\n", temp[i].key, h1, i);
           pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
           pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock));
           return temp[i].value;
       }
    } 

    h2 = h2 % cuckoohashtable->num_buckets;
    pthread_mutex_lock(&(cuckoohashtable->buckets[h2].bucketLock));
    temp = cuckoohashtable->buckets[h2].firstNode;    
    for(i = 0; i< NUM_SLOTS; i++){
       if(temp[i].key != NULL && !strcmp(temp[i].key, key)){
          printf("Get : Key : %s at [%d][%d]\n", temp[i].key, h2, i);
          pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
          pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock));
          return temp[i].value;
       }
    }
     
    pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
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

void freeHashTable(cuckooHashTable* newHashTable){
    int i,j;

    for(i=0; i<newHashTable->num_buckets; i++){
        for(j=0;j<NUM_SLOTS;j++){
            free(newHashTable->buckets[i].firstNode[j].key);
            free(newHashTable->buckets[i].firstNode[j].value);
            free(&(newHashTable->buckets[i].firstNode[j]));
        }
        free(&(newHashTable->buckets[i]));
    }
    free(newHashTable);
    return;
}

entryNode* _put(cuckooHashTable* htptr, char *key, char *value){
    int num_iterations = 0;
    uint32_t h1 = 0, h2 = 0;
    entryNode *first, *second;
    entryNode evictentry;
    char *curr_key, *curr_value;
    int i;

    curr_key = key;
    curr_value = value;
    char* temp_key = (char *) malloc(sizeof(char) * MAX_SIZE);
    char* temp_value = (char *) malloc(sizeof(char) * MAX_SIZE);
    
    while(num_iterations < MAX_ITERATIONS){
        printf("Num Iterations %d for key %s\n", num_iterations, key);
        num_iterations++;
        computehash(curr_key, &h1, &h2);
        srand48((unsigned)h1);
        printf("PUT H1: %d, H2: %d, key: %s\n", h1, h2, curr_key);

        h1 = h1 % htptr->num_buckets;
        first = htptr->buckets[h1].firstNode;    
        h2 = h2 % htptr->num_buckets;
        second = htptr->buckets[h2].firstNode;
       
        printf("_put: Waiting for h1: %d lock\n", h1);
        pthread_mutex_lock(&(cuckoohashtable->buckets[h1].bucketLock));
        for(i = 0; i< NUM_SLOTS; i++){
            if(first[i].key == NULL){
                first[i].key = (char *) malloc(sizeof(char) * MAX_SIZE);
                first[i].value = (char *) malloc(sizeof(char) * MAX_SIZE);
                strcpy(first[i].key, curr_key);
                strcpy(first[i].value, curr_value);
                printf("First: Inserted Key %s in bucket %d, %d\n", key, h1, i); 
                pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
                printf("_put: Released h1: %d\n", h1);
                return NULL;
            }
            else if(!strcmp(first[i].key,curr_key)){
                strcpy(first[i].value, curr_value);
                pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
                printf("_put: Released h1: %d\n", h1);
                return NULL;
            }
        }
       
        printf("_put: Waiting for h2:%d lock\n", h2);
        
        if(h1!=h2){
        pthread_mutex_lock(&(cuckoohashtable->buckets[h2].bucketLock));
        for(i = 0; i< NUM_SLOTS; i++){
            if(second[i].key == NULL){
                second[i].key = (char *) malloc(sizeof(char) * MAX_SIZE);
                second[i].value = (char *) malloc(sizeof(char) * MAX_SIZE);
                strcpy(second[i].key, curr_key);
                strcpy(second[i].value, curr_value);
                printf("Second: Inserted Key %s in bucket %d, %d\n", key, h2, i);
                pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
                pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
                printf("_put: Released h2: %d\n", h2);
                return NULL;
            }
            else if(!strcmp(second[i].key,curr_key)){
                strcpy(second[i].value, curr_value);
                pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
                pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
                printf("_put: Released h2: %d\n", h2);
                return NULL;
            }
        }
        }

        int index = rand_r(&h1) % (2*NUM_SLOTS);

        if(index >= 0 && index <NUM_SLOTS){
            evictentry = first[index];
        }
        else{
            evictentry = second[index - NUM_SLOTS];
        }

        printf("Evicted Key %s with index %d\n", evictentry.key, index);
        strcpy(temp_key, evictentry.key);
        strcpy(temp_value, evictentry.value);

        strcpy(evictentry.key, curr_key);
        strcpy(evictentry.value, curr_value);
        printHashTable();
        
        strcpy(curr_key, temp_key);
        strcpy(curr_value, temp_value);

        pthread_mutex_unlock(&(cuckoohashtable->buckets[h1].bucketLock));
        pthread_mutex_unlock(&(cuckoohashtable->buckets[h2].bucketLock));
    }
    
    entryNode* finalevictedNode = (entryNode *) malloc(sizeof(entryNode));
    finalevictedNode->key = curr_key;
    finalevictedNode->value = curr_value;
    return finalevictedNode;
}

/*void resize(int num_buckets){
    printf("In Resize\n");
    cuckooHashTable *newHashTable = createHashTable(num_buckets);
    int i, j;
    entryNode* res;

    for(i = 0; i < cuckoohashtable->num_buckets; i++){
        entryNode *bucketRow = cuckoohashtable->buckets[i]; 
        for(j=0; j < NUM_SLOTS; j++){
            if(bucketRow[j].key != NULL){
                res = _put(newHashTable, bucketRow[j].key, bucketRow[j].value);
                if(res != NULL){
                    break;
                }
            }
        }
        if(res != NULL)
            break;        
    }
    if(res != NULL){
        printf("Inside Resize: Free and Resize\n");
        printHashTable();
        freeHashTable(newHashTable);
        resize(2 * num_buckets);
        _put(cuckoohashtable, res->key, res->value);
    }
    cuckoohashtable = newHashTable;
}*/

void put(char* key, char* value){
    printf("\nput: Waiting for readlock\n");
    pthread_rwlock_rdlock(&(cuckoohashtable->hashTableLock)); 
    printf("put: Acquired readlock\n\n");
    entryNode* res = _put(cuckoohashtable, key, value);
   
    if(res != NULL){
        
        printf("RESSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSs\n");
        printHashTable();
        pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock)); 
        //resize((cuckoohashtable->num_buckets) * 2);
        pthread_rwlock_rdlock(&(cuckoohashtable->hashTableLock)); 
        _put(cuckoohashtable, res->key, res->value);
        printHashTable();
    }
    printf("Put a New value : %s\n", value);
    pthread_rwlock_unlock(&(cuckoohashtable->hashTableLock)); 
    printf("put: Released Readlock\n");
    return;
}

void *putthreadfunc(void *id){
    int thread_id = *(int *)id;

    char* keys = (char*)malloc(30*sizeof(char)); 
    char* value = (char*)malloc(30*sizeof(char)); 
 
    int i;
    for(i=0;i<20;i++){
        //keys[i] = (char*)malloc(10*sizeof(char));
        //printf("allocated key %d\n",i);
        snprintf(keys,10,"%d",i + (i*thread_id));
        //printf("key%d: %s\n", i, keys[i]);
        snprintf(value,20,"%d%s",i,"values");
        //printf("value: %s\n",value);
        put(keys,value);
        printf("put in entry %s by %d\n", keys, thread_id);
    }
    
    return NULL;
}

void *getthreadfunc(void *id){ 
    int thread_id = *(int *)id;
    char* keys = (char*)malloc(20*sizeof(char)); 
 
    int i;
    for(i=0;i<20;i++){
        snprintf(keys,10,"%d",i + (i*thread_id));
	printf("GET key: %s, value: %s, thread: %d\n", keys,get(keys), thread_id);
    }
    return NULL;
}


int main(void){
    cuckoohashtable = createHashTable(10);
    printf("createHashTable: num_buckets %d\n", cuckoohashtable->num_buckets);

    pthread_t putthreads[2];
    pthread_t getthreads[2];

    int count = 0;
    for(count = 0; count < 2; count ++){
    	pthread_create(&putthreads[count], NULL, putthreadfunc, (void *) &count); 
    }
   
    //for(count = 0; count < 2; count ++){ 
//	pthread_create(&getthreads[count], NULL, getthreadfunc, (void *) &count); 
  //  }

  	 
    for(count = 0; count < 2; count ++){
	pthread_join(putthreads[count], NULL);
	//pthread_join(getthreads[count], NULL);
    }

    printf("Program Completed\n");
    return 0;
}