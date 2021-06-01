#ifndef _HASHTABLE
#define _HASHTABLE

typedef struct s_HashTableElement{
    int key;
    void *data;
    struct s_HashTableElement *next;
} HashTableKeyValue;

typedef struct {
    int elementSize;
    int size;
    void *data;
} HashTable;

HashTable *hashtableCreate(int size, int elementSize);

int hashtableGetElement(HashTable *ht, int key, void **buf);

int hashtableSetElement(HashTable *ht, int key, void *element);

int hashtableDeleteElement(HashTable *ht, int key);

void hashtableDestroy(HashTable *ht);

#endif