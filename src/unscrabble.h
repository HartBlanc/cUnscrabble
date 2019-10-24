#ifndef UNSCRABBLE_H_INCLUDED
#define UNSCRABBLE_H_INCLUDED

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <zlib.h>
#include "cgaddag.h"

#define HORIZONTAL 1
#define VERTICAL 0
#define RACKSIZE 7
#define BOARDSIZE 11
#define REVERSE 28
#define BLANK 29
#define BINGO 35


/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1ULL<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1ULL<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1ULL<<(b)))
#define BIT_CHECK(a,b) (!!((a) & (1ULL<<(b))))        // '!!' to make sure this returns 0 or 1

/* x=target variable, y=mask */
// #define BITMASK_SET(x,y) ((x) |= (y))
// #define BITMASK_CLEAR(x,y) ((x) &= (~(y)))
// #define BITMASK_FLIP(x,y) ((x) ^= (y))
// #define BITMASK_CHECK_ALL(x,y) (((x) & (y)) == (y))   // warning: evaluates y twice
// #define BITMASK_CHECK_ANY(x,y) ((x) & (y))

#define gdg_follow_edge_idx(gdg, node, ch_idx) gdg->edges[node * GDG_MAX_CHARS + ch_idx]


typedef struct play play;
typedef struct position position;
typedef struct rack rack;
typedef struct tile tile;
typedef struct linked_char linked_char;

struct tile
{
	bool anchor;
	bool empty;
	uint8_t ch_idx;
	uint8_t value;
	uint8_t lm;
	uint8_t wm;

	uint16_t h_cross_score;
	uint16_t v_cross_score;
	uint16_t cross_score;

	uint32_t h_cross_set;
	uint32_t v_cross_set;
	uint32_t cross_set;

};

struct linked_char
{
	char L;
	linked_char* prev;
	linked_char* next;
};

struct play{
	uint8_t* gdg_path;
	uint8_t x;
	uint8_t y;
	bool direction;
	bool* blanks; //or bitset? uint16/32_t
	uint16_t score;
};

struct rack{
	uint32_t bitset;
	uint8_t* char_counts; //includes blanks
	uint8_t* chars;
};

char* gdg_path_to_ch(uint8_t* gdg_path, uint8_t arc_count);

void Gen(uint8_t start_x, uint8_t curr_x, uint8_t y, uint32_t curr_node, rack* rck, tile*** brd, GADDAG* lexicon,
		 uint8_t* gdg_path, bool* blanks, uint8_t arc_count, uint16_t hori_score, uint16_t cross_score, uint8_t hm, uint8_t empties);

void GoOn(uint8_t start_x, uint8_t curr_x, uint8_t y, uint8_t ch_idx, uint8_t* gdg_path, bool* blanks, uint32_t old_node,
		  uint32_t next_node, rack* rck, tile*** brd, GADDAG* lexicon, uint8_t arc_count, bool blank, 
		  uint16_t hori_score, uint16_t cross_score, uint8_t hm, uint8_t empties);

bool follow_word_x(tile*** brd, GADDAG* lexicon, uint32_t curr_node,
				 uint8_t y, uint8_t x, uint8_t limit, int dir);

bool follow_word_y(tile*** brd, GADDAG* lexicon, uint32_t curr_node,
				 uint8_t y, uint8_t x, uint8_t limit, int dir);

void RecordPlay(uint8_t* gdg_path, uint8_t x, uint8_t y, bool* blanks, uint16_t score);

uint8_t last_occupied_below(tile*** brd, uint8_t y, uint8_t x);

uint8_t last_occupied_above(tile*** brd, uint8_t y, uint8_t x);

void update_v_cross_sets(tile*** brd, GADDAG* lexicon, uint8_t left_limit, uint8_t right_limit, uint8_t y);

void update_h_cross_sets(tile*** brd, GADDAG* lexicon, uint8_t upper_limit, uint8_t lower_limit, uint8_t x);

void place(play* ply, tile*** brd, rack* rck, GADDAG* lexicon);

void first_plays(tile*** brd, rack* rck, GADDAG* lexicon);

tile*** init_board(void);


GADDAG* init_gdg(const char* file_path);

#endif