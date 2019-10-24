#include "../src/unscrabble.h"
#include <assert.h>



void init_board_tests(void){

	tile*** brd = init_board();
	
	for (size_t y = 0; y < BOARDSIZE / 2; y++){
		for (size_t x = 0; x < BOARDSIZE / 2; x++){
			assert(brd[y][x]->lm == brd[y][(BOARDSIZE - 1) - x]->lm);
			assert(brd[y][x]->lm == brd[(BOARDSIZE - 1) - y][x]->lm);
			assert(brd[y][x]->wm == brd[y][(BOARDSIZE - 1) - x]->wm);
			assert(brd[y][x]->wm == brd[(BOARDSIZE - 1) - y][x]->wm);
		}	
	}

}


int main(void){
	
	init_board_tests();
	
	
	return 0;
}