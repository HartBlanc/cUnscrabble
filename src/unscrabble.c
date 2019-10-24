// #include "cboard.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <zlib.h>
#include "cgaddag.h"
#include "cgaddag.c"
#include "unscrabble.h"

#define CAP_SIZE 500


//								/0 /1 /2 /3 /4 /5 /6 /7 /8 /9 /10 /11/12/13/14/15/16/17 /18/19/20/21/22/23/24/25/26 /27/28
//				          		// /a /b /c /d /e /f /g /h /i /j  /k /l /m /n /o /p /q  /r /s /t /u /v /w /x /y /z  /. /+
const uint8_t LETTER_VALUES[] = {0, 1, 4, 4, 2, 1, 4, 3, 3, 1, 10, 5, 2, 4, 2, 1, 4, 10, 1, 1, 1, 2, 5, 4, 8, 3, 10, 0, 0};
const uint8_t LETTER_COUNTS[] = {0, 5, 1, 1, 2, 7, 1, 1, 1, 4, 1,  1, 2, 1, 2, 4, 1, 1,  2, 4, 2, 1, 1, 1, 1, 1, 1,  2, 0};

play* plays;
uint32_t play_count = 0;
uint32_t play_size = CAP_SIZE;

char* gdg_path_to_ch(uint8_t* gdg_path, uint8_t arc_count){
	char* ch_path = malloc(arc_count * sizeof(char));
	size_t i = 0;

	for (i = 0; i < arc_count; i++)
		ch_path[i] = gdg_idx_to_ch(gdg_path[i]);

	ch_path[i] = '\0';
	return ch_path;
}

bool str_check(uint8_t* gdg_path, uint8_t* cmp, uint8_t arc_count){
	for (size_t i=0; i < arc_count; i++){
		if (gdg_idx_to_ch(gdg_path[i]) != cmp[i])
			return 0;
	}
	return 1;
}

uint8_t blank_pos(rack* rck){
	for (size_t i=0; i < RACKSIZE; i++){
		if (rck->chars[i] == BLANK) return i + 1;
	}
	return 0;
}

rack new_rack(const rack* old_rck, uint8_t L, uint8_t i){
	rack new_rck = *old_rck;
	if (new_rck.char_counts[L] == 1)
		BIT_CLEAR(new_rck.bitset, L);
	new_rck.chars[i] = '\0';
	new_rck.char_counts[L]--;
	return new_rck;
}

// Gen takes a pos and provides a new L (from the board, rack, or blank tile) to extend by.
// updates rack, L (and therefore node)
void Gen(uint8_t start_x, uint8_t curr_x, uint8_t y, uint32_t curr_node, rack* rck, tile*** brd, GADDAG* lexicon,
		 uint8_t* gdg_path, bool* blanks, uint8_t arc_count, uint16_t hori_score, uint16_t cross_score, uint8_t hm, uint8_t empties){

	uint32_t next_node;
	uint32_t prune_set;
	uint8_t ch_idx;

	prune_set = brd[y][curr_x]->cross_set; 

	if (!brd[y][curr_x]->empty){	// the square is not empty, move on to next tile
		//#BUG: If anchor is left-most tile of word it will extend left rather than right
		//#SOLUTION: define anchor as always right-most tile of contiguous 
		ch_idx = brd[y][curr_x]->ch_idx;
		next_node = gdg_follow_edge_idx(lexicon, curr_node, ch_idx);
		//don't get cross_score, lm or wm from non-empty tiles
		hori_score += brd[y][curr_x]->value;

		GoOn(start_x, curr_x, y, ch_idx, gdg_path, blanks, curr_node, next_node, rck, brd, lexicon,
			 arc_count, 0, hori_score, cross_score, hm, empties);
	}
	else if (rck->bitset){      //if rack is not empty fill empty space with tile or blank

		       // prune traversal by rack and cross-set
		//#TO-DO: Compare with alternative pruning / traversal methods (final states, bitsets, dense vs sparse)
		for (size_t i = 0; i < RACKSIZE; i++){ // traverse edges
			ch_idx = rck->chars[i];
			if (ch_idx && BIT_CHECK(prune_set, ch_idx)){ //Ensure that cross_set does not include blank
				next_node = gdg_follow_edge_idx(lexicon, curr_node, ch_idx);

				uint8_t tile_points = LETTER_VALUES[ch_idx] * brd[y][curr_x]->lm;
				uint8_t cross_points = brd[y][curr_x]->cross_score;

				rck->chars[i] = '\0';		// update rack
				GoOn(start_x, curr_x, y, ch_idx, gdg_path, blanks, curr_node, next_node, rck, brd, lexicon, arc_count, 0,
					 hori_score + tile_points,
					 cross_score + (cross_points ? (cross_points + tile_points) * brd[y][curr_x]->wm : 0),
					 hm * brd[y][curr_x]->wm,
					 empties + 1);
				rck->chars[i] = ch_idx;
			}
		}
		
		uint8_t b_pos = blank_pos(rck);
		if (b_pos){ // if has blank - iterate through all possible values
			for (ch_idx = 1; ch_idx < 27; ch_idx++){
				if (BIT_CHECK(prune_set, ch_idx)){ // would this be optimised as a dense array
					next_node = gdg_follow_edge_idx(lexicon, curr_node, ch_idx);
					rck->chars[b_pos - 1] = '\0';
	 				GoOn(start_x, curr_x, y, ch_idx, gdg_path, blanks, curr_node, next_node, rck, brd, lexicon, arc_count, 1,
	 					 hori_score,
	 					 cross_score + (brd[y][curr_x]->cross_score * brd[y][curr_x]->wm),
	 					 hm * brd[y][curr_x]->wm,
	 					 empties + 1);
	 				rck->chars[b_pos - 1] = BLANK;
	 			}
			}
		}
	}
}

// GoOn takes L (and node), builds the word, records play and uses board_context to determine next pos
// pos is passed back to Gen
void GoOn(uint8_t start_x, uint8_t curr_x, uint8_t y, uint8_t ch_idx, uint8_t* gdg_path, bool* blanks, uint32_t old_node,
		  uint32_t next_node, rack* rck, tile*** brd, GADDAG* lexicon, uint8_t arc_count, bool blank, 
		  uint16_t hori_score, uint16_t cross_score, uint8_t hm, uint8_t empties){
	
	if (curr_x <= start_x){
		blanks[arc_count] = blank;
		gdg_path[arc_count++] = ch_idx;
		if (BIT_CHECK(lexicon->letter_sets[old_node], ch_idx) && (curr_x == 0 || brd[y][curr_x - 1]->empty)){
			gdg_path[arc_count] = '\0';
			printf("HERE %d %d %d %d\n", hori_score, hm, empties, cross_score);
			RecordPlay(gdg_path, start_x, y, blanks, (hori_score * hm) + (empties == 7 ? BINGO : 0) + cross_score);
		}
		if (next_node){
			// If there is room to the left get the next char to the left
			if (curr_x > 0)
				Gen(start_x, curr_x - 1, y, next_node, rck, brd, lexicon, gdg_path, blanks, arc_count,
					hori_score, cross_score, hm, empties);

			//If possible extend the current prefix rightwards
			next_node = gdg_follow_edge_idx(lexicon, next_node, REVERSE); // change_direction
			if (next_node && (curr_x == 0 || brd[y][curr_x - 1]->empty) && ((start_x + 1) < BOARDSIZE)){
				blanks[arc_count] = 0;
				gdg_path[arc_count++] = REVERSE;
				Gen(start_x, start_x + 1, y, next_node, rck, brd, lexicon, gdg_path, blanks, arc_count,
					hori_score, cross_score, hm, empties);
			}
		}
	}
	else if (curr_x > start_x){
		blanks[arc_count] = blank;
		gdg_path[arc_count++] = ch_idx;
		if (BIT_CHECK(lexicon->letter_sets[old_node], ch_idx) && ( (curr_x + 1) == BOARDSIZE || brd[y][curr_x + 1]->empty)){
			gdg_path[arc_count] = '\0';
			printf("HERE %d %d %d %d\n", hori_score, hm, empties, cross_score);
			RecordPlay(gdg_path, start_x, y, blanks, (hori_score * hm) + (empties == 7 ? BINGO : 0) + cross_score);
		}
		if (next_node && (curr_x + 1) < BOARDSIZE)
			Gen(start_x, curr_x + 1, y, next_node, rck, brd, lexicon, gdg_path, blanks, arc_count,
				hori_score, cross_score, hm, empties);
	}
}

void RecordPlay(uint8_t* gdg_path, uint8_t x, uint8_t y, bool* blanks, uint16_t score){

	play* new_play = malloc(sizeof(play));
	new_play->gdg_path = gdg_path;
	new_play->x = x;
	new_play->y = y;
	new_play->direction = HORIZONTAL;
	new_play->score = score;
	new_play->blanks = malloc((BOARDSIZE+1) * sizeof(bool));
	memcpy(new_play->blanks, blanks, (BOARDSIZE+1));

	char word[BOARDSIZE + 2];
	size_t i;
	for (i = 0; gdg_path[i] != '\0' ; i++){
		word[i] = gdg_idx_to_ch(gdg_path[i]);
	}
	word[i] = '\0';

	printf("WORD: %s, X: %d, Y: %d, Score: %d, Horizontal?: %d\n\n", word, x, y, score, HORIZONTAL);

	if (play_count > play_size) {
		play_size *= 2;
		plays = realloc(plays, play_size);
		if (plays == NULL) printf("plays too large....");
	}

        plays[play_count++] = *new_play;
}

void first_plays(tile*** brd, rack* rck, GADDAG* lexicon){
	Gen(BOARDSIZE/2, BOARDSIZE/2, BOARDSIZE/2, 0, rck, brd, lexicon,  malloc(sizeof(char)*(BOARDSIZE+2)), calloc((BOARDSIZE+1), sizeof(char)), 0, 0, 0, 1, 0);
}

play* select_move(){
	return &plays[0];
}

//#TO-DO implement cross_score calculation
void place(play* ply, tile*** brd, rack* rck, GADDAG* lexicon){
	uint8_t pos = 0;
	uint8_t ch_idx;
	bool reversed = false;
	uint8_t left_limit = ply->x;

	for (size_t i = 0; (ch_idx = ply->gdg_path[i]) != '\0'; i++){
		
		if (ch_idx != REVERSE){
			if (brd[ply->y][ply->x + pos]->empty){
				brd[ply->y][ply->x + pos]->ch_idx = ch_idx;
				brd[ply->y][ply->x + pos]->value = LETTER_VALUES[ch_idx];
				brd[ply->y][ply->x + pos]->empty = 0;
				//no need to update wms/lms
				
				uint8_t loa = last_occupied_above(brd, ply->y, ply->x + pos);
				uint8_t lob = last_occupied_below(brd, ply->y, ply->x + pos);

				// get prefix + suffix here so can calculate prefix and suffix score

				update_h_cross_sets(brd, lexicon, loa, lob, ply->x + pos);
				
			}
			(pos <= 0) ? pos -- : pos++;
		}
		else
			left_limit = ply->x + pos;
			reversed = true;
			pos = 1;

	}

	if (reversed) //pos is rightmost tile, left_limit defined
		update_v_cross_sets(brd, lexicon, left_limit, ply->x + pos, ply->y);

	else{ //pos is leftmost tile, anchor is rightmost tile
		update_v_cross_sets(brd, lexicon, ply->y, ply->x + pos, ply->y);
		
		//anchor only needs to be updated if was empty before playing word	
		update_v_cross_sets(brd, lexicon, ply->y, ply->x, ply->y);
	}

}

uint8_t last_occupied_above(tile*** brd, uint8_t y, uint8_t x){
	for (uint8_t curr = y; curr > 0; curr--){
		if (brd[curr - 1][x]->empty)
			return curr;
	}
	return 0;
}

uint8_t last_occupied_below(tile*** brd, uint8_t y, uint8_t x){
	
	for (uint8_t curr = y; curr < (BOARDSIZE - 1); curr++){
		if (brd[curr + 1][x]->empty)
			return curr;
	}

	return BOARDSIZE - 1;
}


//h_cross_set: a 
void update_h_cross_sets(tile*** brd, GADDAG* lexicon, uint8_t upper_limit, uint8_t lower_limit, uint8_t x){
	uint32_t curr_node = 0;
	uint16_t cross_score = 0;

	for (uint8_t curr = lower_limit; curr > upper_limit; curr--)
		curr_node  = gdg_follow_edge_idx(lexicon, curr_node, brd[curr][x]->ch_idx);
		cross_score += brd[curr][x]->value;



	uint32_t start_node = curr_node;

	//special case where two words are seperated by one square
	if (upper_limit > 1 && !brd[upper_limit - 2][x]->empty){
		uint8_t prefix_y;
		brd[upper_limit - 1][x]->h_cross_set = 0;

		for (prefix_y = upper_limit - 2; prefix_y >= 0 && !brd[prefix_y][x]->empty; prefix_y--)
			prefix_score += brd[prefix_y][x]->value;

		uint8_t prefix_limit = prefix_y - 1;

		for (size_t i = 0; i < lexicon->edge_counts[start_node]; i++){
			uint8_t ch_idx = lexicon->edge_chars[curr_node][i];
			curr_node = gdg_follow_edge_idx(lexicon, start_node, ch_idx);
			uint16_t prefix_score = 0;

			for (prefix_y = upper_limit - 2; curr_node && prefix_y > prefix_limit; prefix_y--)
				curr_node = gdg_follow_edge_idx(lexicon, curr_node, brd[prefix_y][x]->ch_idx);
				prefix_score += brd[prefix_y][x]->value;

			if (curr_node && BIT_CHECK(lexicon->letter_sets[curr_node], brd[prefix_y][x]->ch_idx))
				BIT_SET(brd[upper_limit - 1][x]->h_cross_set, ch_idx);
		}
	}

	else if (upper_limit > 0)
		brd[upper_limit - 1][x]->h_cross_score = cross_score;
		brd[upper_limit - 1][x]->h_cross_set = lexicon->letter_sets[curr_node];
		


	if (lower_limit < (BOARDSIZE - 1)){
		uint32_t rev_node  = gdg_follow_edge_idx(lexicon, curr_node, REVERSE);
		if (lower_limit < (BOARDSIZE - 2) && !brd[lower_limit + 2][x]->empty){ //special case where cross-set needs to consider prefix and suffix
			uint8_t suffix_y;
			brd[lower_limit + 1][x]->h_cross_set = 0;

			for (size_t i = 0; i < lexicon->edge_counts[rev_node]; i++){
				uint8_t ch_idx = lexicon->edge_chars[rev_node][i];
				curr_node = gdg_follow_edge_idx(lexicon, rev_node, ch_idx);

				for (suffix_y = lower_limit + 2; curr_node && suffix_y < BOARDSIZE - 1 && !brd[suffix_y + 1][x]->empty; suffix_y++)
					curr_node = gdg_follow_edge_idx(lexicon, curr_node, brd[suffix_y][x]->ch_idx);

				if (curr_node && BIT_CHECK(lexicon->letter_sets[curr_node], brd[suffix_y][x]->ch_idx))
					BIT_SET(brd[lower_limit + 1][x]->h_cross_set, ch_idx);
			}
		}

		else
			brd[lower_limit + 1][x]->h_cross_score = cross_score;
			brd[lower_limit + 1][x]->h_cross_set = lexicon->letter_sets[curr_node];
	}
}

void update_v_cross_sets(tile*** brd, GADDAG* lexicon, uint8_t left_limit, uint8_t right_limit, uint8_t y){
	uint32_t curr_node = 0;

	for (uint8_t curr = right_limit; curr > left_limit; curr--)
		curr_node = gdg_follow_edge_idx(lexicon, curr_node, brd[y][curr]->ch_idx);

	uint32_t start_node = curr_node;

	if (left_limit > 1 && !brd[y][left_limit - 2]->empty){ //special case where cross-set needs to consider prefix and suffix
		uint8_t prefix_x;
		brd[y][left_limit - 1]->v_cross_set = 0;

		for (size_t i = 0; i < lexicon->edge_counts[start_node]; i++){
			//#TO-DO check if need to handle rev node?
			uint8_t ch_idx = lexicon->edge_chars[curr_node][i];
			curr_node = gdg_follow_edge_idx(lexicon, start_node, ch_idx);

			for (prefix_x = left_limit - 2; curr_node && prefix_x > 0 && !brd[y][prefix_x - 1]->empty; prefix_x++)
				curr_node = gdg_follow_edge_idx(lexicon, curr_node, brd[y][prefix_x]->ch_idx);	

			if (curr_node && BIT_CHECK(lexicon->letter_sets[curr_node], brd[y][prefix_x]->ch_idx))
				BIT_SET(brd[y][left_limit - 1]->h_cross_set, ch_idx);	

			if (follow_word_x(brd, lexicon, curr_node, y, left_limit - 2, 0, -1))
				BIT_SET(brd[y][left_limit - 1]->v_cross_set, ch_idx);
		}
	}

	else if (left_limit > 0)
		brd[y][left_limit - 1]->v_cross_set = lexicon->letter_sets[start_node];

	if (right_limit < (BOARDSIZE - 1)){
		uint32_t rev_node  = gdg_follow_edge_idx(lexicon, start_node, REVERSE);

		if (right_limit < (BOARDSIZE - 2) && !brd[y][right_limit + 2]->empty){ //special case where cross_set needs to consider prefix and suffix
			uint8_t suffix_x;
			brd[y][right_limit + 1]->v_cross_set = 0;

			for (size_t i = 0; i < lexicon->edge_counts[rev_node]; i++){
				uint8_t ch_idx = lexicon->edge_chars[rev_node][i];
				curr_node = gdg_follow_edge_idx(lexicon, rev_node, ch_idx);

				for (suffix_x = right_limit + 2; curr_node && suffix_x < BOARDSIZE - 1 && !brd[y][suffix_x + 1]->empty; suffix_x++)
					curr_node = gdg_follow_edge_idx(lexicon, curr_node, brd[y][suffix_x]->ch_idx);

				if (curr_node && BIT_CHECK(lexicon->letter_sets[curr_node], brd[y][suffix_x]->ch_idx))
					BIT_SET(brd[y][right_limit + 1]->v_cross_set, ch_idx);
			}
		}

		else
			brd[y][right_limit + 1]->v_cross_set = lexicon->letter_sets[rev_node];
	}
}

// void legal_plays(tile*** brd, rack* rck, GADDAG* lexicon){
// 	bool anchor;
// 	play_count = 0;

// 	for (size_t y = 0; y < BOARDSIZE; y++){
// 		for (size_t x = 0; x < BOARDSIZE; x++){
// 			//#TO-DO handle board edges
// 			anchor = brd->empties[y][x-1] & (!brd->empties[y][x] | (brd->empties[y][x+1] & (!brd->empties[y-1][x] | !brd->empties[y+1][x])));
// 			if(anchor) //#TO-DO implement anchor check 
// 				Gen(x, x, y, 0, rck, brd, lexicon, malloc(sizeof(char)*(BOARDSIZE+2)), calloc((BOARDSIZE+1), sizeof(char)), 0);
// 		}
// 	}
// 	//#TO-DO vertical plays
// 	//	Determine vertical anchor rule
// 	//bool anchor = brd.empties[y-1][x] & (!brd.empties[y][x] | (brd.empties[y+1][x] & (!brd.empties[y][x+1] | !brd.empties[y][x-1])));
// }

tile*** init_board(){

		uint8_t LMS[BOARDSIZE][BOARDSIZE] = {
			{3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3},
			{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
			{1, 1, 3, 1, 2, 1, 2, 1, 3, 1, 1},
			{1, 1, 1, 3, 1, 1, 1, 3, 1, 1, 1},
			{1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1},
			{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
			{1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1},
			{1, 1, 1, 3, 1, 1, 1, 3, 1, 1, 1},
			{1, 1, 3, 1, 2, 1, 2, 1, 3, 1, 1},
			{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
			{3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3}
		};

		uint8_t WMS[BOARDSIZE][BOARDSIZE] = {
			{1, 1, 3, 1, 1, 1, 1, 1, 3, 1, 1},
			{1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1},
			{3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3},
			{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
			{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
			{1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1},
			{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
			{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
			{3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3},
			{1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1},
			{1, 1, 3, 1, 1, 1, 1, 1, 3, 1, 1}
		};

		tile*** brd = malloc(BOARDSIZE * sizeof(tile**));

		for (size_t y = 0; y < BOARDSIZE; y++){
			brd[y] = malloc(BOARDSIZE * sizeof(tile*));
			for (size_t x = 0; x < BOARDSIZE; x++){
				
				brd[y][x] = malloc(sizeof(tile));
				brd[y][x]->anchor = !!(y == BOARDSIZE/2 && x == BOARDSIZE/2);
				
				//#TO-DO COULD CH_IDX REPLACE EMPTY?			
				brd[y][x]->empty = 1;
				brd[y][x]->ch_idx = 0;
				brd[y][x]->value = 0;

				brd[y][x]->lm = LMS[y][x];
				brd[y][x]->wm = WMS[y][x];

				brd[y][x]->h_cross_score = 0;
				brd[y][x]->v_cross_score = 0;
				brd[y][x]->cross_score = 0;

				brd[y][x]->h_cross_set = 0b111111111111111111111111111;
				brd[y][x]->v_cross_set = 0b111111111111111111111111111;
				brd[y][x]->cross_set = 0b111111111111111111111111111;

			}
		}
	return brd;
}

GADDAG* init_gdg(const char* file_path){

	GADDAG* gdg = gdg_create();

    FILE* file = fopen(file_path, "r");
    char line[MAX_WORD_LENGTH];

    while (fgets(line, sizeof(line), file)) {
        gdg_add_word(gdg, strtok(line, "\n"));
    }
    gdg_build_adj_edges(gdg);

    printf("\nGADDAG INITIALISED WITH %d nodes", gdg->num_nodes);
    // gdg_save(gdg, "example.gdg");

    return gdg;
}



int main(){
	char* rck_str = "jackleg";
	rack rck;
	rck.bitset = 0;
	rck.char_counts = calloc(GDG_MAX_CHARS, sizeof(uint8_t));
	rck.chars = malloc((RACKSIZE) * sizeof(uint8_t));
	for (size_t i = 0; i < RACKSIZE; i++){
		rck.chars[i] = rck_str[i] - 96;
	}
	tile*** brd = init_board();

	GADDAG* lexicon = init_gdg("ENABLE.txt");

	// generate first play if board is empty
	// play* all_plays = gen_first_plays(board, rack, lexicon)
	// place(select_move(all_plays))
	plays = malloc(sizeof(play) * CAP_SIZE);
	printf("%d", lexicon->edge_counts[0]);
	// while (true){
	// printf("Which tiles are on your rack\n");
	// fgets(rck_str, 500, stdin);
	printf("\nMADE IT THIS FAR\n");
	for (size_t i = 0; i < RACKSIZE; i++){
		BIT_SET(rck.bitset, rck_str[i] - 96);
		rck.char_counts[rck_str[i] - 96]++;
	}
	printf("MADE IT THIS FAR\n");
    first_plays(brd, &rck, lexicon);
    // place();
    printf("\n %d \n", play_count);

	// then generate
}
