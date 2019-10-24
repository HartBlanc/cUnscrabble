#define BOARDSIZE 11

struct board
{
	anchors[BOARDSIZE][BOARDSIZE] bool;
	empty[BOARDSIZE][BOARDSIZE] bool;
	value[BOARDSIZE][BOARDSIZE] int;
	lm[BOARDSIZE][BOARDSIZE] int;
	wm[BOARDSIZE][BOARDSIZE] int;
	cross_sets[BOARDSIZE][BOARDSIZE] uint32_t;
};