#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdlib.h>
#include <string.h>
#include "types.h" // Include our new types file for the FileMetadata definition

#define HT_SIZE 1024 // Power of 2 is good practice

// Hash Table item (a node in a collision chain)
typedef struct HT_Item {
    char key[100];
    void* value;
    struct HT_Item* next;
} HT_Item;

// The Hash Table itself
typedef struct HashTable {
    HT_Item** buckets; // An array of pointers to HT_Items
} HashTable;

// --- Hash Function (a good, simple one called djb2) ---
static unsigned long hash_function(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % HT_SIZE;
}

// Creates an empty hash table
HashTable* ht_create() {
    HashTable* table = (HashTable*)malloc(sizeof(HashTable));
    table->buckets = (HT_Item**)calloc(HT_SIZE, sizeof(HT_Item*));
    return table;
}

// Inserts a file into the hash table
void ht_insert(HashTable* table, const char* key, void* value) {
    if (!table || !key) return;
    unsigned long index = hash_function(key);
    
    HT_Item* new_item = (HT_Item*)malloc(sizeof(HT_Item));
    strcpy(new_item->key, key);
    new_item->value=value;
    
    // Insert at the beginning of the chain at this index
    new_item->next = table->buckets[index];
    table->buckets[index] = new_item;
}

// Searches for a file by its key (filename)
void* ht_search(HashTable* table, const char* key) { // Return void*
    if (!table || !key) return NULL;
    unsigned long index = hash_function(key);
    
    HT_Item* current = table->buckets[index];
    while (current) {
        if (strcmp(current->key, key) == 0) {
            return current->value; // Return the generic value
        }
        current = current->next;
    }
    return NULL;
}

// Deletes a file from the hash table
void ht_delete(HashTable* table, const char* key) {
    if (!table || !key) return;
    unsigned long index = hash_function(key);
    
    HT_Item* current = table->buckets[index];
    HT_Item* prev = NULL;

    while (current) {
        if (strcmp(current->key, key) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                table->buckets[index] = current->next;
            }
            // The caller is responsible for freeing the actual value if needed.
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

#endif // HASH_TABLE_H