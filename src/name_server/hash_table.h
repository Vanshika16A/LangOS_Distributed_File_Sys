#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdlib.h>
#include <string.h>
#include "CRWD.c" // We need FileMetadata definition

#define HT_SIZE 1024 // Power of 2 is good practice

// Hash Table item (a node in the collision chain)
typedef struct HT_Item {
    char key[100];
    FileMetadata* file;
    struct HT_Item* next;
} HT_Item;

// The Hash Table itself
typedef struct HashTable {
    HT_Item** buckets; // Array of pointers to HT_Items
} HashTable;

// --- Hash Function (djb2) ---
static unsigned long hash_function(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash % HT_SIZE;
}

// --- Public API ---
HashTable* ht_create() {
    HashTable* table = (HashTable*)malloc(sizeof(HashTable));
    table->buckets = (HT_Item**)calloc(HT_SIZE, sizeof(HT_Item*));
    return table;
}

void ht_insert(HashTable* table, FileMetadata* file) {
    if (!table || !file) return;
    unsigned long index = hash_function(file->filename);
    
    // Create new item
    HT_Item* new_item = (HT_Item*)malloc(sizeof(HT_Item));
    strcpy(new_item->key, file->filename);
    new_item->file = file;
    
    // Insert at head of the chain
    new_item->next = table->buckets[index];
    table->buckets[index] = new_item;
}

FileMetadata* ht_search(HashTable* table, const char* key) {
    if (!table || !key) return NULL;
    unsigned long index = hash_function(key);
    
    HT_Item* current = table->buckets[index];
    while (current) {
        if (strcmp(current->key, key) == 0) {
            return current->file;
        }
        current = current->next;
    }
    return NULL;
}

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
            // Note: We free the HT_Item, but the caller is responsible
            // for freeing the FileMetadata struct itself.
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

#endif // HASH_TABLE_H