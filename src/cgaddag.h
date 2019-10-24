#ifndef GADDAG_H_INCLUDED
#define GADDAG_H_INCLUDED

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <zlib.h>

#define GDG_MAX_CHARS 27
#define GDG_CAP 100
#define MAX_WORD_LENGTH 128
#define REVERSE 28
#define BLANK 29


typedef struct Result_Struct Result;
typedef struct GADDAG_Struct GADDAG;

struct Result_Struct {
    const char* str;
    Result *next;
    Result *prev;
};

struct GADDAG_Struct {
    uint32_t cap;
    uint32_t num_words;
    uint32_t num_nodes;
    uint32_t num_edges;
    uint32_t* edges;
    uint32_t* letter_sets;
    uint32_t** edge_nodes;
    char** edge_chars;
    uint8_t* edge_counts;
};

/* Create a new GADDAG */
GADDAG* gdg_create(void);

/* Save a GADDAG to file */
off_t gdg_save(const GADDAG* gdg, const char *path);
off_t gdg_save_compressed(const GADDAG* gdg, const char *path);

/* Load a (compressed or uncompressed) GADDAG from file */
GADDAG* gdg_load(const char *path);

/* Destroy a GADDAG */
void gdg_destroy(GADDAG* gdg);

/* Add a word to a GADDAG */
/* Returns: */
/*     0 if the word was successfully added */
/*     1 if the word contains invalid characters */
/*     2 if the word could not be added to the GADDAG due to running */
/*         out of memory, leaving the GADDAG in an undefined state */
int gdg_add_word(GADDAG *gdg, const char *word);

/* Check if a GADDAG contains a word */
_Bool gdg_has(const GADDAG *gdg, const char *word);

/* Get all words in a GADDAG which start with a prefix */
Result* gdg_starts_with(const GADDAG *gdg, const char *prefix);

/* Get all words in a GADDAG which contain a substring */
Result* gdg_contains(const GADDAG *gdg, const char *sub);

/* Get all words in a GADDAG which end with a suffix */
Result* gdg_ends_with(const GADDAG *gdg, const char *suffix);

/* Place the edges of a node into a buffer */
int gdg_edges(const GADDAG *gdg, const uint32_t node, char *buffer);

/* Place the letter set of a node into a buffer */
int gdg_letter_set(const GADDAG *gdg, const uint32_t node, char *buffer);

/* Check if a character is part of a node's letter set */
_Bool gdg_is_end(const GADDAG *gdg, const uint32_t node, const char ch);

/* Follow an edge from a node, returning 0 if no such edge exists */
uint32_t gdg_follow_edge(const GADDAG *gdg, const uint32_t node, const char ch);

/* Destroy a Result */
void gdg_destroy_result(const Result *res);

/* Buid the adjacency list Representation */
void gdg_build_adj_edges(GADDAG* gdg);

#endif

