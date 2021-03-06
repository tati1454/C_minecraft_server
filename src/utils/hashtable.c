#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

int hashFunction(HashTable *ht, int n)
{
    return n % ht->size;
}

HashTable *hashtableCreate(int size, int elementSize)
{
    HashTable *ht = malloc(sizeof(HashTable));
    memset(ht, 0, sizeof(HashTable));

    ht->size = size;
    ht->elementSize = elementSize;
    ht->data = malloc(sizeof(HashTableKeyValue) * size);
    memset(ht->data, 0, sizeof(HashTableKeyValue) * size);

    return ht;
}

HashTableKeyValue *hashtableSearchKey(HashTable* ht, int key)
{
    HashTableKeyValue *keyAddress = &ht->data[hashFunction(ht, key)];
    
    for(;;)
    {
        if(keyAddress->data == NULL)
        {
            return NULL;
        }

        if(keyAddress->key == key)
        {
            return keyAddress;
        }

        if(keyAddress->next == NULL) return NULL;

        keyAddress = keyAddress->next;
    }
}

void* hashtableGetElement(HashTable *ht, int key)
{
    HashTableKeyValue *keyAddress = hashtableSearchKey(ht, key);
    if(keyAddress == NULL){
        return NULL;
    }

    return keyAddress->data;
}

int hashtableSetElement(HashTable *ht, int key, void *element)
{
    HashTableKeyValue *keyAddress = &ht->data[hashFunction(ht, key)];

    for(;;){
        if(keyAddress->data == NULL){
            keyAddress->key = key;
            keyAddress->data = malloc(ht->elementSize);
            memcpy(keyAddress->data, element, ht->elementSize);
            return 0;
        }

        if(keyAddress->next == NULL){
            keyAddress->next = malloc(sizeof(HashTableKeyValue));

            HashTableKeyValue *actualKey = keyAddress->next;
            actualKey->key = key;
            actualKey->data = malloc(ht->elementSize);
            actualKey->next = NULL;

            memcpy(actualKey->data, element, ht->elementSize);
            return 0;
        }

        if(keyAddress->next != NULL){
            keyAddress = keyAddress->next;
        }
    }
}

int hashtableDeleteElement(HashTable *ht, int key)
{
    HashTableKeyValue *head = &ht->data[hashFunction(ht, key)];
    HashTableKeyValue *keyAddress = head;
    HashTableKeyValue *prevKey = NULL;

    for(;;)
    {
        if(keyAddress->data == NULL)
        {
            return -1;
        }

        if(keyAddress->key == key)
        {
            if(keyAddress == head)
            {
                free(keyAddress->data);
                if(keyAddress->next == NULL)
                {
                    memset(keyAddress, 0, sizeof(HashTableKeyValue));
                    return 0;
                }
                prevKey = keyAddress->next;
                memcpy(head, keyAddress->next, sizeof(HashTableKeyValue));
                free(prevKey);
                return 0;
            }

            if(keyAddress->next != NULL) {
                prevKey->next = keyAddress->next;
                free(keyAddress->data);
                free(keyAddress);
                return 0;
            }
            
            if(keyAddress->next == NULL){
                prevKey->next = NULL;
                free(keyAddress->data);
                free(keyAddress);
                return 0;
            }
        }

        prevKey = keyAddress;
        keyAddress = keyAddress->next;
    }
}

void hashtableDestroy(HashTable *ht)
{
    for(int i = 0; i < ht->size; ++i){
        HashTableKeyValue* element = &ht->data[i];
        for(;;) {
            if(element->data == NULL) break;
            free(element->data);

            if(element->next == NULL) break;
            element = element->next;
        }
    }

    free(ht);
}
