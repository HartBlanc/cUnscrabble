#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <zlib.h>

#include "cgaddag.h"

/* Internal functions */

Result* gdg_create_result(const char *str, Result* next) {
    /* Create a new Result */
    Result* self = malloc(sizeof(struct Result_Struct));
    if (self == NULL) return NULL;

    if (next) next->prev = self;

    self->str = strdup(str);
    if (self->str == NULL) return NULL;
    self->next = next;
    self->prev = NULL;

    return self;
}

int gdg_ch_to_idx(char ch) {
    /* Turn a character into a bitmask index */
    ch = tolower(ch);
    if (ch == '+') return REVERSE;
    else if (ch == '?') return BLANK;
    else if (ch >= 97 && ch <= 122) return ch - 96;
    else return -1;
}

char gdg_idx_to_ch(uint8_t idx) {
    /* Turn a bitmask index into a character */
    if (idx == REVERSE) return '+';
    else if (idx >= 1 && idx <= 27) return idx + 96;
    else return '\0';
}

bool gdg_grow(GADDAG *gdg, uint32_t new_cap) {
    /* Increase the node capacity of a GADDAG */
    if (new_cap == gdg->cap) return true;
    uint32_t old_cap = gdg->cap;
    gdg->cap = new_cap;

    size_t new_node_size = new_cap * sizeof(uint32_t);
    size_t new_node_diff = (new_cap - old_cap) * sizeof(uint32_t);
    size_t new_edge_size = new_node_size * GDG_MAX_CHARS;
    size_t new_edge_diff = new_node_diff * GDG_MAX_CHARS;

    uint32_t *new_edges = realloc(gdg->edges, new_edge_size);
    if (new_edges == NULL) return false;
    else gdg->edges = new_edges;
    memset(gdg->edges + old_cap * GDG_MAX_CHARS, 0, new_edge_diff);

    uint32_t *new_letter_sets = realloc(gdg->letter_sets, new_node_size);
    if (new_letter_sets == NULL) return false;
    else gdg->letter_sets = new_letter_sets;
    memset(gdg->letter_sets + old_cap, 0, new_node_diff);

    return true;
}

void gdg_set_edge(GADDAG *gdg, uint32_t node, char ch, uint32_t dst) {
    /* Create an edge from one node to another */
    int ch_idx = gdg_ch_to_idx(ch);
    gdg->edges[node * GDG_MAX_CHARS + ch_idx] = dst;
    gdg->num_edges++;
}

uint32_t gdg_add_edge(GADDAG *gdg, uint32_t node, char ch) {
    /* Add an edge to a node (if it does not already exist), returning the */
    /* destination node */
    uint32_t dst = gdg_follow_edge(gdg, node, ch);
    if (dst == 0) {
        dst = gdg->num_nodes++;
        if (gdg->num_nodes >= gdg->cap) {
            if(!gdg_grow(gdg, gdg->cap + GDG_CAP)) return 0;
        }
        gdg_set_edge(gdg, node, ch, dst);
    }
    return dst;
}

void gdg_add_end(GADDAG *gdg, uint32_t node, char ch) {
    /* Add a letter to a node's letter set */
    int ch_idx = gdg_ch_to_idx(ch);
    gdg->letter_sets[node] |= (1 << ch_idx);
}

uint32_t gdg_add_final_edge(GADDAG *gdg, uint32_t node, char ch, char end_ch) {
    /* Add an edge to a node (if it does not already exist) and add a letter */
    /* to the destination node's letter set */
    uint32_t dst = gdg_add_edge(gdg, node, ch);
    gdg_add_end(gdg, dst, end_ch);
    return dst;
}

bool gdg_force_edge(GADDAG *gdg, uint32_t node, char ch, uint32_t dst) {
    uint32_t next_node = gdg_follow_edge(gdg, node, ch);
    if (next_node != dst) {
        if (next_node != 0) return false;
        gdg_set_edge(gdg, node, ch, dst);
    }
    return true;
}

Result* gdg_crawl(const GADDAG *gdg, const uint32_t node,
                  const char *partial_word, const bool wrapped, Result *res) {
    /* Find all possible words starting from a node */
    const size_t len = strlen(partial_word);
    const uint8_t start_idx = wrapped ? 1 : 0;

    for (uint32_t i = start_idx; i < GDG_MAX_CHARS; i++) {
        const char ch = gdg_idx_to_ch(i);
        const uint32_t letter_set = gdg->letter_sets[node];
        const uint32_t next_node = gdg_follow_edge(gdg, node, ch);

        if (i > 0 && (letter_set >> i) & 1) {
            char *word = calloc(len + 2, sizeof(char));
            if (word == NULL) return NULL;

            if (wrapped) {
                memcpy(word, partial_word, len);
                word[len] = ch;
            } else {
                word[0] = ch;
                memcpy(word + 1, partial_word, len);
            }
            
            if (!res) res = gdg_create_result(word, NULL);
            else res = gdg_create_result(word, res);
            free(word);
        }

        if (next_node) {
            if (i == 0) res = gdg_crawl(gdg, next_node, partial_word, 1, res);
            else {
                char *new_partial_word = calloc(len + 2, sizeof(char));
                if (new_partial_word == NULL) return NULL;

                if (wrapped) {
                    memcpy(new_partial_word, partial_word, len);
                    new_partial_word[len] = ch;
                } else {
                    new_partial_word[0] = ch;
                    memcpy(new_partial_word + 1, partial_word, len);
                }

                res = gdg_crawl(gdg, next_node, new_partial_word, wrapped, res);
                free(new_partial_word);
            }
        }
    }

    return res;
}

Result* gdg_crawl_end(const GADDAG *gdg, const uint32_t node,
                      const char *partial_word, Result *res) {
    /* Find all possible words starting from a node only by appending */
    /* letters */
    const size_t len = strlen(partial_word);

    for (uint32_t i = 1; i < GDG_MAX_CHARS; i++) {
        const char ch = gdg_idx_to_ch(i);
        const uint32_t letter_set = gdg->letter_sets[node];
        uint32_t next_node = gdg_follow_edge(gdg, node, ch);

        if ((letter_set >> i) & 1) {
            char *word = calloc(len + 2, sizeof(char));
            if (word == NULL) return NULL;

            word[0] = ch;
            memcpy(word + 1, partial_word, len);
            
            if (!res) res = gdg_create_result(word, NULL);
            else res = gdg_create_result(word, res);
            free(word);
        }

        if (next_node) {
            char *new_partial_word = calloc(len + 2, sizeof(char));
            if (new_partial_word == NULL) return NULL;

            new_partial_word[0] = ch;
            memcpy(new_partial_word + 1, partial_word, len);

            res = gdg_crawl_end(gdg, next_node, new_partial_word, res);
            free(new_partial_word);
        }
    }

    return res;
}

/* External interface */

GADDAG *gdg_create(void) {
    /* Create a new GADDAG */
    GADDAG *gdg = malloc(sizeof(struct GADDAG_Struct));

    gdg->cap = GDG_CAP;
    gdg->edges = calloc(GDG_MAX_CHARS * gdg->cap, sizeof(uint32_t));
    gdg->letter_sets = calloc(gdg->cap, sizeof(uint32_t));
    gdg->num_words = 0;
    gdg->num_nodes = 1;
    gdg->num_edges = 0;

    return gdg;
}

off_t gdg_save(const GADDAG *gdg, const char *path) {
    /* Save a GADDAG to file */
    off_t total_wrote = 0;
    size_t size = sizeof(uint32_t);
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    total_wrote += fwrite(&gdg->cap, size, 1, fp);
    total_wrote += fwrite(&gdg->num_words, size, 1, fp);
    total_wrote += fwrite(&gdg->num_nodes, size, 1, fp);
    total_wrote += fwrite(&gdg->num_edges, size, 1, fp);
    total_wrote += fwrite(gdg->edges, size, gdg->cap * GDG_MAX_CHARS, fp);
    total_wrote += fwrite(gdg->letter_sets, size, gdg->cap, fp);
    total_wrote *= size;

    fclose(fp);

    return total_wrote;
}

off_t gdg_save_compressed(const GADDAG *gdg, const char *path) {
    /* Save a compressed GADDAG to file */
    off_t total_wrote = 0;
    size_t size = sizeof(uint32_t);
    gzFile fp = gzopen(path, "wb");
    if (!fp) return -1;

    total_wrote += gzwrite(fp, &gdg->cap, size);
    total_wrote += gzwrite(fp, &gdg->num_words, size);
    total_wrote += gzwrite(fp, &gdg->num_nodes, size);
    total_wrote += gzwrite(fp, &gdg->num_edges, size);
    total_wrote += gzwrite(fp, gdg->edges, size * gdg->cap * GDG_MAX_CHARS);
    total_wrote += gzwrite(fp, gdg->letter_sets, size * gdg->cap);

    gzclose(fp);

    return total_wrote;
}

GADDAG* gdg_load(const char *path) {
    /* Load a (compressed or uncompressed) GADDAG from file */
    GADDAG *gdg = gdg_create();
    size_t size = sizeof(uint32_t);
    uint32_t cap;

    gzFile fp = gzopen(path, "rb");
    if (!fp) {
        gdg_destroy(gdg);
        return NULL;
    }

    gzread(fp, &cap, size);
    gdg_grow(gdg, cap);

    gzread(fp, &gdg->num_words, size);
    gzread(fp, &gdg->num_nodes, size);
    gzread(fp, &gdg->num_edges, size);
    gzread(fp, gdg->edges, size * gdg->cap * GDG_MAX_CHARS);
    gzread(fp, gdg->letter_sets, size * gdg->cap);

    gzclose(fp);

    return gdg;
}

int gdg_add_word(GADDAG *gdg, const char *word) {
    /* Add a word to a GADDAG */
    /* Returns: */
    /*     0 if the word was successfully added */
    /*     1 if the word contains invalid characters */
    /*     2 if the word could not be added to the GADDAG due to running */
    /*         out of memory, leaving the GADDAG in an undefined state */
    const size_t l = strlen(word);

    for (size_t i = 0; i < l; i++) {
        if (gdg_ch_to_idx(word[i]) == -1) return 1;
    }

    gdg->num_words++;

    // Add path from last letter in word
    uint32_t node = 0;
    for (int i = l - 1; i >= 2; --i) {
        node = gdg_add_edge(gdg, node, word[i]);
        if (!node) return 2;
    }
    
    node = gdg_add_final_edge(gdg, node, word[1], word[0]);
    if (!node) return 2;

    if (l == 1) return 0;

    // Add path from penultimate letter in word
    node = 0;
    for (int i = l - 2; i >= 0; --i) {
        node = gdg_add_edge(gdg, node, word[i]);
        if (!node) return 2;
    }
    node = gdg_add_final_edge(gdg, node, '+', word[l - 1]);
    if (!node) return 2;

    // Create remaining paths
    for (int m = l - 3; m >= 0; --m) {
        const uint32_t force_node = node;
        node = 0;
        for (int i = m; i >= 0; --i) {
            node = gdg_add_edge(gdg, node, word[i]);
            if (!node) return 2;
        }
        node = gdg_add_edge(gdg, node, '+');
        if (!node) return 2;
        if (!gdg_force_edge(gdg, node, word[m + 1], force_node)) return 2;
    }
    
    return 0;
}

int gdg_letter_set(const GADDAG *gdg, const uint32_t node, char *buffer) {
    /* Place the letter set of a node into a buffer */
    uint8_t i = 0;

    for (int offset = 1; offset < GDG_MAX_CHARS; offset++) {
        if ((gdg->letter_sets[node] >> offset) & 1) {
            buffer[i++] = gdg_idx_to_ch(offset);
        }
    }
    return i;
}

int gdg_edges(const GADDAG *gdg, const uint32_t node, char *buffer) {
    /* Place the edges of a node into a buffer */
    uint8_t i = 0;

    for (int offset = 0; offset < GDG_MAX_CHARS; offset++) {
        char ch = gdg_idx_to_ch(offset);
        uint32_t next_node = gdg_follow_edge(gdg, node, ch);
        if (next_node) buffer[i++] = ch;
    }
    return i;
}

bool gdg_is_end(const GADDAG *gdg, const uint32_t node, const char ch) {
    /* Check if a character is part of a node's letter set */
    const int ch_idx = gdg_ch_to_idx(ch);
    if (ch_idx == -1) return false;
    return gdg->letter_sets[node] & (1 << ch_idx);
}

bool gdg_has(const GADDAG *gdg, const char *word) {
    /* Check if a GADDAG contains a word */ 
    const size_t l = strlen(word);

    uint32_t node = 0;
    for (int i = l - 1; i > 0; --i) {
        node = gdg_follow_edge(gdg, node, word[i]);
        if (!node) return false;
    }

    return gdg_is_end(gdg, node, word[0]);
}

Result* gdg_starts_with(const GADDAG *gdg, const char *prefix) {
    /* Get all words in a GADDAG which start with a prefix */
    const size_t l = strlen(prefix);
    Result *res = NULL;
    uint32_t node = 0;

    for (int i = l - 1; i >= 0; --i) {
        if (i == 0 && gdg_is_end(gdg, node, prefix[i])) {
            res = gdg_create_result(prefix, NULL);
        }
        node = gdg_follow_edge(gdg, node, prefix[i]);
        if (!node) return NULL;
    }

    node = gdg_follow_edge(gdg, node, '+');
    if (!node) return NULL;

    return gdg_crawl(gdg, node, prefix, 1, res);
}

Result* gdg_contains(const GADDAG *gdg, const char *sub) {
    /* Get all words in a GADDAG which contain a substring */
    const size_t l = strlen(sub);
    Result *res = NULL;
    uint32_t node = 0;

    for (int i = l - 1; i >= 0; --i) {
        if (i == 0 && gdg_is_end(gdg, node, sub[i])) {
            res = gdg_create_result(sub, NULL);
        }
        node = gdg_follow_edge(gdg, node, sub[i]);
        if (!node) return NULL;
    }

    return gdg_crawl(gdg, node, sub, 0, res);
}

Result* gdg_ends_with(const GADDAG *gdg, const char *suffix) {
    /* Get all words in a GADDAG which end with a suffix */
    const size_t l = strlen(suffix);
    Result *res = NULL;
    uint32_t node = 0;

    for (int i = l - 1; i >= 0; --i) {
        if (i == 0 && gdg_is_end(gdg, node, suffix[i])) {
            res = gdg_create_result(suffix, NULL);
        }
        node = gdg_follow_edge(gdg, node, suffix[i]);
        if (!node) return NULL;
    }

    return gdg_crawl_end(gdg, node, suffix, res);
}

uint32_t gdg_follow_edge(const GADDAG *gdg, const uint32_t node, const char ch) {
    /* Follow an edge from a node, returning 0 if no such edge exists */
    const int ch_idx = gdg_ch_to_idx(ch);
    if (ch_idx == -1) return 0;
    return gdg->edges[node * GDG_MAX_CHARS + ch_idx];
}

void gdg_destroy(GADDAG *gdg) {
    /* Destroy a GADDAG */
    free(gdg->edges);
    free(gdg->letter_sets);
    free(gdg);
}

void gdg_destroy_result(const Result *res) {
    /* Destroy a Result */
    Result *tmp = (Result*)res;
    Result *last = NULL;
    while (tmp) {
        last = tmp;
        tmp = tmp->next;
    }

    Result *prev;
    while (last) {
        prev = last->prev;
        free((char*)(last->str));
        free((Result*)last);
        last = prev;
    }
}

void gdg_build_adj_edges(GADDAG* gdg){

    uint32_t next_node;

    // allocate outer-arrays
    gdg->edge_nodes = malloc(sizeof(uint32_t*) * gdg->num_nodes);
    gdg->edge_chars = malloc(sizeof(char*) * gdg->num_nodes);
    gdg->edge_counts = calloc(gdg->num_nodes, sizeof(uint8_t));
    

    for (size_t node = 0; node < gdg->num_nodes; node++){

        // over-allocate inner-arrays
        gdg->edge_nodes[node] = malloc(sizeof(uint32_t) * GDG_MAX_CHARS);
        gdg->edge_chars[node] = malloc(sizeof(char) * GDG_MAX_CHARS);
        
        // populate inner-arrays
        for (size_t ch_idx = 1; ch_idx < GDG_MAX_CHARS; ch_idx++){

            if ((next_node = gdg->edges[node * GDG_MAX_CHARS + ch_idx])){
                gdg->edge_nodes[node][gdg->edge_counts[node]] = next_node;
                gdg->edge_chars[node][gdg->edge_counts[node]] = ch_idx;
                gdg->edge_counts[node]++;
            }
        }
        
        // reallocate inner-arrays
        gdg->edge_nodes[node] = realloc(gdg->edge_nodes[node], sizeof(uint32_t) * gdg->edge_counts[node]);
        gdg->edge_chars[node] = realloc(gdg->edge_chars[node], sizeof(char) * gdg->edge_counts[node]);
    }
}


// int main(int argc, char* argv[])
// {
//     GADDAG* gdg = gdg_create();

//     const char* file_path = "ENABLE.txt";
//     FILE* file = fopen(file_path, "r");
//     char line[MAX_WORD_LENGTH];

//     while (fgets(line, sizeof(line), file)) {
//         printf("%s", line); 
//         gdg_add_word(gdg, strtok(line, "\n"));
//     }
//     printf("\nNum Nodes: %d", gdg->num_nodes);
//     gdg_build_adj_edges(gdg);

//     for(size_t i = 0; i < gdg->num_nodes; i++)
//         for(size_t j=0; j < gdg->edge_counts[i]; j++)
//             printf("\nNode:%d Char:%c", gdg->edge_nodes[i][j], gdg_idx_to_ch(gdg->edge_chars[i][j]));

//     printf("\nSaving GADDAG to 'example.gdg': ");
//     gdg_save(gdg, "example.gdg");


//     return 0;
// }
