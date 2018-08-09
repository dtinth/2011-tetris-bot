#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <vector>
#include <time.h>

#define SPRINT 0
#define VERSUS 1

using namespace std;

typedef int TetriminoMatrix[4][4];

struct TetriminoRotation {
	TetriminoMatrix data;
	int size;
	void load(int s, TetriminoMatrix d) {
		size = s;
		for (int i = 0; i < s; i ++) {
			for (int j = 0; j < s; j ++) {
				data[i][j] = d[i][j];
			}
		}
	}
};

struct TetriminoTemplate {
	TetriminoRotation rotations[4];
	bool noRotate;
	void load(int size, TetriminoMatrix data, bool nr = false) {
		noRotate = nr;
		for (int k = 0; k < 4; k ++) {
			int nextData[4][4];
			for (int i = 0; i < size; i ++) {
				for (int j = 0; j < size; j ++) {
					int takeI = j;
					int takeJ = size - 1 - i;
					nextData[i][j] = data[takeI][takeJ];
				}
			}
			rotations[k].load(size, nextData);
			for (int i = 0; i < size; i ++) {
				for (int j = 0; j < size; j ++) {
					data[i][j] = nextData[i][j];
				}
			}
		}
	}
};

TetriminoTemplate tetriminos[7];

typedef TetriminoTemplate *Tetrimino;

void buildTetriminoTemplates() {
	tetriminos[0].load(4, (int[4][4]){ 0,0,0,0, 0,1,1,0, 0,1,1,0, 0,0,0,0 }, true);
	tetriminos[1].load(3, (int[4][4]){ 0,1,0,0, 1,1,1,0, 0,0,0,0, 0,0,0,0 });
	tetriminos[2].load(3, (int[4][4]){ 1,0,0,0, 1,1,1,0, 0,0,0,0, 0,0,0,0 });
	tetriminos[3].load(3, (int[4][4]){ 0,1,1,0, 1,1,0,0, 0,0,0,0, 0,0,0,0 });
	tetriminos[4].load(3, (int[4][4]){ 0,0,1,0, 1,1,1,0, 0,0,0,0, 0,0,0,0 });
	tetriminos[5].load(3, (int[4][4]){ 1,1,0,0, 0,1,1,0, 0,0,0,0, 0,0,0,0 });
	tetriminos[6].load(4, (int[4][4]){ 0,0,0,0, 1,1,1,1, 0,0,0,0, 0,0,0,0 });
}

#define WIDTH 10
#define HEIGHT 20

enum CellType {
	BLANK_CELL    = 0,
	INACTIVE_MINO = 1,
	ACTIVE_MINO   = 2,
	WALL          = 3,
	BOMB          = 4,
	OPAQUE_SPACE  = 5
};

typedef CellType TetrisMatrix[HEIGHT][WIDTH];

#define inrange(x, a, b) ((a) <= (x) && (x) < (b))

typedef void (*Key)();

struct State {
	
	TetrisMatrix cells;
	Tetrimino active;
	Tetrimino next;
	Tetrimino hold;

	vector<Key> keys;
	int cleared;
	int rotated;
	int height;
	int combo;
	int baseCombo;
	int heights[WIDTH];
	int gap;
	int cliffev;
	int linesSent;
	bool tetris;
	bool tetrising;
	
	int unwellness;
	int bomb;
	
	CellType get(int i, int j) {
		if (!inrange(i, 0, HEIGHT)) return OPAQUE_SPACE;
		if (!inrange(j, 0, WIDTH))	return OPAQUE_SPACE;
		return cells[i][j];
	}
	
	void init() {
		combo = 0;
		tetrising = true;
		linesSent = 0;
	}
	
	void resume(State &inState) {
		combo = inState.combo;
		linesSent = inState.linesSent;
		tetrising = inState.tetrising;
	}
	
	bool nextStateByDropping(const TetriminoRotation &block, int position, int rotate, State &next) {
		
		int max[WIDTH];
		for (int j = 0; j < WIDTH; j ++) {
			max[j] = -1;
		}
		for (int i = 0; i < block.size; i ++) {
			for (int j = 0; j < block.size; j ++) {
				if (block.data[i][j]) {
					if (!inrange(position + j, 0, WIDTH)) {
						return false;
					}
					max[j + position] = i;
				}
			}
		}
		bool canDrop = false;
		int dropPosition = -1;
		int bombed = 0;
		int bombPositionStart, bombPositionEnd;
		
		for (int i = 0; i < HEIGHT; i ++) {
			for (int j = 0; j < WIDTH; j ++) {
				if (max[j] != -1) {
					CellType c = get(i + max[j] + 1, j);
					if (c == INACTIVE_MINO || c == WALL || c == BOMB || c == OPAQUE_SPACE) {
						canDrop = true;
						dropPosition = i;
						for (int k = i + max[j] + 1; get(k, j) == BOMB; k ++) {
							bombed ++;
							if (bombed == 1) {
								bombPositionStart = k;
							}
							bombPositionEnd = k;
						}
						break;
					}
				}
			}
			if (canDrop) {
				break;
			}
		}
		if (!canDrop) {
			return false;
		}
		next.keys = keys;
		next.rotated = rotate;
		
		TetrisMatrix tempCells;
		memcpy(tempCells, cells, sizeof(TetrisMatrix));
		
		for (int i = 0; i < block.size; i ++) {
			for (int j = 0; j < block.size; j ++) {
				if (block.data[i][j]) {
					tempCells[dropPosition + i][position + j] = INACTIVE_MINO;
				}
			}
		}
		
		memset(next.cells, 0, sizeof(TetrisMatrix));
		next.cleared = bombed;
		int k = HEIGHT - 1;
		for (int i = HEIGHT - 1; i >= 0; i --) {
			bool clear = true;
			if (bombed && bombPositionStart <= i && i <= bombPositionEnd) {
				clear = true;
			} else {
				for (int j = 0; j < WIDTH; j ++) {
					if (tempCells[i][j] != INACTIVE_MINO) {
						clear = false;
						break;
					}
				}
			}
			if (clear) {
				next.cleared ++;
			} else {
				for (int j = 0; j < WIDTH; j ++) {
					next.cells[k][j] = tempCells[i][j];
				}
				k --;
			}
		}
		for (; k >= 0; k --) {
			for (int j = 0; j < WIDTH; j ++) {
				next.cells[k][j] = BLANK_CELL;
			}
		}
		
		next.resume(*this);
		
		if (next.cleared) {
			next.combo ++;
		} else {
			next.combo = 0;
		}
		
		next.calculate();
		
		if (tetrising && height <= 7 && next.height > 7) {
			next.tetrising = false;
		}
		if (!tetrising && height >= 4 && next.height < 4) {
			next.tetrising = true;
		}
		
		return true;
		
	}
	
	inline void calc_height() {
		
		height = 0;
		for (int j = 0; j < WIDTH; j ++) {
			heights[j] = 0;
			for (int i = 0; i < HEIGHT; i ++) {
				if (cells[i][j] == INACTIVE_MINO || cells[i][j] == WALL || cells[i][j] == BOMB) {
					heights[j] = 20 - i;
					break;
				}
			}
			if (heights[j] > height) {
				height = heights[j];
			}
		}
		
	}
	
	int floorLevel;
	
	inline void calc_floor() {
		
		floorLevel = HEIGHT - 1;
		for (int i = HEIGHT - 1; i >= 0; i --) {
			for (int j = 0; j < WIDTH; j ++) {
				if (cells[i][j] == BLANK_CELL || cells[i][j] == INACTIVE_MINO) {
					floorLevel = i;
					return;
				}
			}
		}
		
	}
	
	inline void calc_unwellness() {
		
		unwellness = 0;
		for (int i = 20 - height; i < HEIGHT; i ++) {
			for (int j = 0; j < WIDTH; j ++) {
				if (cells[i][j] == BLANK_CELL) {
					unwellness ++;
				}
			}
		}
		
	}
	
	inline void calc_cliff() {
		
		cliffev = 0;
		for (int j = 1; j < WIDTH - 1; j ++) {
			int cliff = 0;
			if (heights[j - 1] > heights[j]) {
				cliff += heights[j - 1] - heights[j];
			}
			if (heights[j + 1] > heights[j]) {
				cliff += heights[j + 1] - heights[j];
			}
			cliff = max(0, cliff - 2);
			cliffev += cliff * cliff * cliff;
		}
		
	}
	
	inline void calc_gap() {
		
		gap = 0;
		for (int j = 0; j < WIDTH; j ++) {
			bool found = false;
			for (int i = 0; i < HEIGHT; i ++) {
				if (cells[i][j] == INACTIVE_MINO) {
					found = true;
				} else if (found && cells[i][j] == BLANK_CELL) {
					gap ++;
				}
			}
		}
		
	}
	
	inline void calc_lines() {
#if SPRINT
		linesSent += cleared;
#endif
#if VERSUS
		if (cleared > 0) {
			linesSent += cleared - 1;
		}
		if (cleared == 4) {
			linesSent ++;
		}
		if (combo > 1) {
			linesSent ++;
		}
		if (height == 0) {
			linesSent += 10;
		}
#endif
	}
	
	inline void calculate() {
		
		calc_floor();
		calc_height();
		calc_unwellness();
		calc_cliff();
		calc_gap();
		calc_lines();
		
	}
	
	inline int evaluate() {
		int ev = 0;
		
		// less keystrokes, better!
		ev += 3 * rotated;
		ev += 2 * keys.size();
		
		// less height, better!
		ev += 10 * max(0, height - 2) * max(0, height - 3);
		
		// less gap, better!
		ev += 20 * unwellness * unwellness * unwellness * height;
		ev += 32 * gap * gap * gap;
		ev += 40 * cliffev;
		
		// more combo, better!
		if (cleared > 0 || combo > 1) {
			ev += -32 * cleared * cleared * cleared * combo;
		}
		
		// rewards for combo
		ev += -100000 * (combo * combo - 1);
		
		// rewards for tetris
		ev += cleared == 4 ? -3000000 : 0;
		
		// rewards for line sent
		ev += linesSent * -200000;
		
#if VERSUS
		// try to make tetris
		if (tetrising) {
			for (int i = 19 - height; i <= floorLevel; i ++) {
				for (int j = 0; j < WIDTH; j ++) {
					if (get(i, j) != (j == WIDTH - 1 ? BLANK_CELL : INACTIVE_MINO)) {
						ev += 10000;
					}
				}
			}
		}
#endif
		
		// rewards for perfect clear
		if (height == 0) {
			return INT_MIN;
		}
		
		// damn! too high!
		if (height >= 9) {
			ev += 2000000;
		}
		
		return ev;
	}
	
};

Tetrimino getTetriminoFromStdin() {
	
	int id; scanf("%d", &id);
	if (id == -1) return NULL;
	return &tetriminos[id];
	
}

struct Controller {
	
	static void left()   { puts("#left"); }
	static void right()  { puts("#right"); }
	static void hold()   { puts("#hold"); }
	static void drop()   { puts("#drop"); }
	static void rotate() { puts("#rotate"); }
	
};

class TetrisBot {
	
	int numStates;
	int minEvaluation;
	State minState;
	State lastState;
	bool canSwap;
	
	void evaluateState(State &state) {
		int evaluation = state.evaluate();
		/*
		for (int i = 0; i < state.keys.size(); i ++) {
			printf("%p ", state.keys[i]);
		}
		puts("");
		printf("eval %d\n", evaluation);
		*/
		if (evaluation < minEvaluation) {
			minEvaluation = evaluation;
			minState = state;
		}
	}
	
	void processBlock(State &state, Tetrimino base, Tetrimino next, Tetrimino hold, bool swap, int baseCombo) {
		int rotate = 1;
		for (int k = 0; k < (base->noRotate ? 1 : 4); k ++) {
			for (int j = -3; j < 20; j ++) {
				int rotateTimes = (4 - rotate) % 4;
				if (base->noRotate) {
                    rotateTimes = 0;
                }
				State newState;
				numStates ++;
				if (state.nextStateByDropping(base->rotations[k], j, rotateTimes, newState)) {
					if (swap) {
						newState.keys.push_back(Controller::hold);
					}
					void (*moveAction)();
					int moveCount, rotateCount;
					rotateCount = newState.rotated;
					if (j < 3) {
						moveAction = Controller::left;
						moveCount = 3 - j;
					} else {
						moveAction = Controller::right;
						moveCount = j - 3;
					}
					while (rotateCount > 0 || moveCount > 0) {
						if (rotateCount > 0) {
							newState.keys.push_back(Controller::rotate);
							rotateCount --;
						}
						if (moveCount > 0) {
							newState.keys.push_back(moveAction);
							moveCount --;
						}
					}
					newState.keys.push_back(Controller::drop);
					int newBaseCombo = baseCombo;
					if (baseCombo == -1) {
						newBaseCombo = newState.combo;
					}
					newState.baseCombo = newBaseCombo;
					evaluateState(newState);
					if (next != NULL) {
						processBlock(newState, next, NULL, hold, false, newBaseCombo);
						processBlock(newState, hold, NULL, next, true,  newBaseCombo);
					} else if (hold != NULL) {
						if (rand() % 3 == 0) {
							processBlock(newState, hold, NULL, NULL, true, newBaseCombo);
						}
					}
				}
			}
			rotate = (rotate + 1) % 4;
		}
	}
	
	void botMove(State &state) {
		state.resume(lastState);
		minEvaluation = INT_MAX;
		numStates = 0;
		clock_t start = clock();
		processBlock(state, state.active, state.next, state.hold, false, -1);
		if (canSwap) {
			processBlock(state, state.hold, state.next, state.active, true, -1);
		}
		clock_t finish = clock();
		if (minEvaluation < INT_MAX) {
			printf("states:               %9d\n", numStates);
			printf("best score:           %9d\n", minEvaluation);
			printf("cumulative lines sent:%9d\n", minState.linesSent);
			printf("building tetris:      %9s\n", minState.tetrising ? "yes" : "no");
			printf("time taken (sec):     %9f\n", (finish - start) / (float)CLOCKS_PER_SEC);
			for (int i = 0; i < minState.keys.size(); i ++) {
				minState.keys[i]();
			}
			lastState = minState;
		}
	}
	
public:
	void processState(State &state) {
		if (state.active && state.next && state.hold) {
			botMove(state);
			canSwap = true;
		}
		if (!state.hold) {
			canSwap = false;
		}
		puts("#finish");
		fflush(stdout);
	}
	
	TetrisBot() {
		minEvaluation = INT_MAX;
		lastState.init();
	}
	
};

TetrisBot bot;

void readAndProcessState() {
	State state;
	for (int i = 0; i < HEIGHT; i ++) {
		for (int j = 0; j < WIDTH; j ++) {
			scanf("%d", &state.cells[i][j]);
		}
	}
	state.active = getTetriminoFromStdin();
	state.next = getTetriminoFromStdin();
	state.hold = getTetriminoFromStdin();
	bot.processState(state);
}

int main() {
	srand((unsigned int)time(NULL));
	buildTetriminoTemplates();
	while (!feof(stdin)) {
		readAndProcessState();
	}
}









