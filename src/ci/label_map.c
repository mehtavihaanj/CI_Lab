#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "label_map.h"
#include <stdio.h>

static void          free_entry(Entry *e);
static void          free_entries(Entry *e);
static unsigned long hash_function(char *s);
static bool entry_init(char *id, Command *command, Entry* entry);

bool label_map_init(LabelMap *map, int capacity) {
    
    map->capacity = capacity;
    map->entries = (Entry**) calloc(capacity, sizeof(Entry*));

    for (int i = 0; i < capacity; i++) {
        map->entries[i] = (Entry*) calloc(1, sizeof(Entry));
    }
    return true;
}

/**
 * @brief Frees the resources for a `singular` entry.
 *
 * @param e The pointer to the entry to free.
 */
static void free_entry(Entry *e) {
    // STUDENT TODO: Free a *singular* entry
    // Do not free children; see below
    free(e->id);
    free(e);
}

/**
 * @brief Frees the given list of entries.
 *
 * @param e A pointer to the first entry to free.
 */
static void free_entries(Entry *e) {
    // STUDENT TODO: Free an entry and its children
    
    while (e->next != NULL) {
        Entry* temp = e;
        e = e->next;
        free_entry(temp);
    }
    free_entry(e);
}

void label_map_free(LabelMap *map) {
   
    for (int i = 0; i < map->capacity; i++) {     
        free_entries(map->entries[i]);
    }
    free(map->entries);
}

/**
 * @brief Returns a hash of the specified id.
 *
 * @param s The string to hash.
 * @return The hash of `s`
 */
static unsigned long hash_function(char *s) {
    unsigned long i = 0;
    for (int j = 0; s[j]; j++) {
        i += s[j];
    }

    return i;
}

/**
 * @brief Initializes the given entry's state.
 *
 * @param entry The given entry to modify.
 * @param id The id associated with this entry.
 * @param command The command associated with this entry.
 * @return True if the entry was initialized successfully, false otherwise.
 */
static bool entry_init(char *id, Command *command, Entry* entry) {
    
    
    // Initialize at first bucket
    if (entry->id == NULL || entry->command == NULL) {
        entry->id = id;
        entry->command = command;
        return true;
    }
    
    // Go through bucket and find next open spot
    
    Entry* entryTempTraverse = entry;
    while (entryTempTraverse->next != NULL) {
        entryTempTraverse = entryTempTraverse->next;
    }
    
    Entry* chainedEntry = (Entry*) calloc(1, sizeof(Entry));
    chainedEntry->id = id;
    chainedEntry->command = command;
    chainedEntry->next = NULL;
    
    entryTempTraverse->next = chainedEntry;
    
    return true;
}

bool put_label(LabelMap *map, char *id, Command *command) {
    
  
    unsigned long labelId = hash_function(id) % map->capacity;
    entry_init(id, command, map->entries[labelId]);
    return true;
}

Entry *get_label(LabelMap *map, char *id) {
    unsigned long labelId = hash_function(id) % map->capacity;
    return map->entries[labelId];
}
