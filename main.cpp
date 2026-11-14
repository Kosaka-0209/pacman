#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <queue>
#include <utility>
using namespace std;

const int TILE_SIZE = 40; // 1マスのピクセルサイズ
const int MAZE_WIDTH = 20; // 横のマス数
const int MAZE_HEIGHT = 15; // 縦のマス数

const int SCREEN_WIDTH = TILE_SIZE * MAZE_WIDTH;
const int SCREEN_HEIGHT = TILE_SIZE * MAZE_HEIGHT;

// 迷路データ: '1'=壁, '0'=通路(ドット), ' '=何もない
const int NUM_MAZES = 2;
char mazePatterns[NUM_MAZES][MAZE_HEIGHT][MAZE_WIDTH + 1] = {
{
"11111111111111111111",
"10000000000000000001",
"10111011111110101101",
"10100000000000101101",
"10101111011110101101",
"10001000010000000001",
"11101011010110111111",
"10001011010110100001",
"10101011010110101101",
"10101000000000101101",
"10101011111110101101",
"10001000000000100001",
"10111110111111111101",
"10000000000000000001",
"11111111111111111111"
},
//ステージ２
{
"11111111111111111111",
"10000000000000000001",
"10111111011011111101",
"10100000000000000101",
"10101111111111110101",
"10001000000000010001",
"11101011111111010111",
"10000010000001000001",
"10111010111101011101",
"10000010000001000001",
"10111111100111111101",
"10100000000000000101",
"10101111111111110101",
"10000000000000000001",
"11111111111111111111",
}
};
//実際に使う迷路
char maze[MAZE_HEIGHT][MAZE_WIDTH + 1];
char originalMaze[MAZE_HEIGHT][MAZE_WIDTH + 1];
//パックマンの位置
int pacRow = 1;
int pacCol = 1;
double pacDirection = 0;
//パックマンアニメーション
SDL_Texture* pacmanOpenTexture = nullptr;
SDL_Texture* pacmanClosedTexture = nullptr;
bool mouthOpen = true;
Uint32 lastAnimTime = 0;
const int ANIM_SPEED = 150;
//幽霊の位置
const int NUM_GHOSTS = 3;//幽霊の数
int ghostRow[NUM_GHOSTS] = { 13,1,13 };
int ghostCol[NUM_GHOSTS] = { 18,18,1 };
Uint32 lastGhostMove = 0; //最後に幽霊が動いた時刻
int ghostMoveDelay = 400; //動く間隔(ミリ) 
//ゲーム
bool gameOver = false;
bool gameClear = false;
bool gameOverSoundPlayed = false;
bool gameClearSoundPlayed = false;
int stage = 1;
//スコア
int score = 0;
//残機
int lives = 3;
//指定したマスが壁かどうか判定
bool isWall(int row, int col) {
	if (row < 0 || row >= MAZE_HEIGHT || col < 0 || col >= MAZE_WIDTH) {
		return true;
	}
	return maze[row][col] == '1';
}

void loadMaze(int stageNumber) {
	int mazeIndex = (stageNumber - 1) % NUM_MAZES;
	for (int row = 0; row < MAZE_HEIGHT; row++) {
		for (int col = 0; col < MAZE_WIDTH; col++) {
			maze[row][col] = mazePatterns[mazeIndex][row][col];
			originalMaze[row][col] = mazePatterns[mazeIndex][row][col];
		}
	}
}

//迷路に残っているドットの数を数える
int countDots() {
	int count = 0;
	for (int row = 0; row < MAZE_HEIGHT;row++) {
		for (int col = 0; col < MAZE_WIDTH;col++) {
			if (maze[row][col] == '0') {
				count++;
			}
		}
	}
	return count;
}

void resetGame() {
	stage = 1;
	loadMaze(stage);
	pacRow = 1;
	pacCol = 1;
	int initialGhostRow[NUM_GHOSTS] = { 13,1,13 };
	int initialGhostCol[NUM_GHOSTS] = { 18,18,1 };
	for (int i = 0; i < NUM_GHOSTS;i++) {
		ghostRow[i] = initialGhostRow[i];
		ghostCol[i] = initialGhostCol[i];
	}
	score = 0;
	lives = 3;
	ghostMoveDelay = 400;
	gameOver = false;
	gameClear = false;
	gameOverSoundPlayed = false;
	gameClearSoundPlayed = false;
}

void moveGhost(int ghostIndex) {
	//探索用
	pair<int, int> parent[MAZE_HEIGHT][MAZE_WIDTH];
	bool visited[MAZE_HEIGHT][MAZE_WIDTH] = { false };

	int startRow = ghostRow[ghostIndex];
	int startCol = ghostCol[ghostIndex];

	queue<pair<int, int>> q;
	q.push({ startRow,startCol });
	visited[startRow][startCol] = true;

	//4方向:上,下,左,右
	int dRow[4] = { -1,1,0,0 };
	int dCol[4] = { 0,0,-1,1 };

	bool found = false;

	while (!q.empty() && !found) {
		pair<int, int> current = q.front();
		q.pop();

		for (int i = 0; i < 4; i++) {
			int newRow = current.first + dRow[i];
			int newCol = current.second + dCol[i];

			if (!isWall(newRow, newCol) && !visited[newRow][newCol]) {
				visited[newRow][newCol] = true;
				parent[newRow][newCol] = current;
				q.push({ newRow, newCol });

				if (newRow == pacRow && newCol == pacCol) {
					found = true;
					break;
				}
			}
		}
	}
	if (!found) return; // 経路が見つからなければ動かない

	// パックマンの位置から逆にたどって、幽霊の「次の一歩」を割り出す
	pair<int, int> step = { pacRow, pacCol };
	while (parent[step.first][step.second] != make_pair(startRow, startCol)) {
		step = parent[step.first][step.second];
	}

	ghostRow[ghostIndex] = step.first;
	ghostCol[ghostIndex] = step.second;
}

void renderText(SDL_Renderer* renderer, TTF_Font* font, const string& text, int centerX, int centerY, SDL_Color color) {
	SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

	SDL_Rect destRect;
	destRect.w = surface->w;
	destRect.h = surface->h;
	destRect.x = centerX - destRect.w / 2;
	destRect.y = centerY - destRect.h / 2;

	SDL_RenderCopy(renderer, texture, nullptr, &destRect);

	SDL_FreeSurface(surface);
	SDL_DestroyTexture(texture);
}

int main(int argc, char* argv[]) {
	loadMaze(stage);
	// SDL初期化
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		cout << "SDL初期化失敗: " << SDL_GetError() << endl;
		return 1;
	}

	if (TTF_Init() == -1) {
		cout << "SDL_ttf初期化失敗: " << endl;
		return 1;
	} 

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
		cout << "SDL_Mix初期化失敗: " << endl;
		return 1;
	}

	//png読み込みを有効化
	if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
		cout << "SDL_image初期化失敗: " << endl;
		return 1;
	}

	// ウィンドウ作成
	SDL_Window* window = SDL_CreateWindow(
		"Pac-Man",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		SCREEN_WIDTH, SCREEN_HEIGHT,
		SDL_WINDOW_SHOWN
	);

	if (window == nullptr) {
		cout << "ウィンドウ作成失敗: " << SDL_GetError() << endl;
		return 1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_Texture* pacmanOpenTexture = IMG_LoadTexture(renderer, "pacman_open.png");
	SDL_Texture* pacmanClosedTexture = IMG_LoadTexture(renderer, "pacman_closed.png");
	SDL_Texture* ghostTexture[NUM_GHOSTS];
	ghostTexture[0] = IMG_LoadTexture(renderer, "ghost_red.png");
	ghostTexture[1] = IMG_LoadTexture(renderer, "ghost_pink.png");
	ghostTexture[2] = IMG_LoadTexture(renderer, "ghost_cyan.png");

	TTF_Font* font = TTF_OpenFont("c:\\Windows\\Fonts\\arialbd.ttf", 48);

	Mix_Chunk* dotSound = Mix_LoadWAV("dot_eat.wav");
	Mix_Chunk* gameOverSound = Mix_LoadWAV("game_over.wav");
	Mix_Chunk* gameClearSound = Mix_LoadWAV("game_clear.wav");
	Mix_Chunk* gameStartSound = Mix_LoadWAV("game_start.wav");

	bool running = true;
	SDL_Event e;

	Mix_PlayChannel(-1, gameStartSound, 0);
	while (running) {
		// 入力処理
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				running = false;
			}
			//リスタート処理
			if (e.key.keysym.sym == SDLK_SPACE && (gameOver || gameClear)) {
				resetGame();
				SDL_SetWindowTitle(window, "Pac-Man - Score: 0  Lives: 3");
			}
			//キーが押された瞬間の処理
			if (e.type == SDL_KEYDOWN && e.key.repeat == 0 && !gameOver && !gameClear) {
				int newRow = pacRow;
				int newCol = pacCol;

				switch (e.key.keysym.sym) {
				case SDLK_UP:    newRow--; pacDirection = 270; break;
				case SDLK_DOWN:  newRow++; pacDirection = 90;  break;
				case SDLK_LEFT:  newCol--; pacDirection = 180; break;
				case SDLK_RIGHT: newCol++; pacDirection = 0;   break;
				}
				//壁でなければ移動
				if (!isWall(newRow, newCol)) {
					pacRow = newRow;
					pacCol = newCol;

					//ドットを食べる処理
					if (maze[pacRow][pacCol] == '0') {
						maze[pacRow][pacCol] = ' ';
						score++;
						Mix_PlayChannel(-1, dotSound, 0);
						//タイトルバーを更新
						string title = "Pac-Man - Stage: " + to_string(stage) + " Score: " + to_string(score) +  " Lives: " + to_string(lives);
						SDL_SetWindowTitle(window, title.c_str());

						//全部食べたらクリア
						if (countDots() == 0) {
							Mix_PlayChannel(-1, gameClearSound, 0);
							stage++;
				            //幽霊のスピードアップ
							if (ghostMoveDelay > 150) {
								ghostMoveDelay = 300;
							}
							loadMaze(stage);
							//パックマンと幽霊の位置リセット
							pacRow = 1;
							pacCol = 1;
							int initialGhostRow[NUM_GHOSTS] = { 13,1,13 };
							int initialGhostCol[NUM_GHOSTS] = { 18,18,1 };
							for (int i = 0; i < NUM_GHOSTS;i++) {
								ghostRow[i] = initialGhostRow[i];
								ghostCol[i] = initialGhostCol[i];
							}
							//タイトル更新
							string clearTitle = "Pac-Man - CLEAR! Score: " + to_string(score) + "Lives: " + to_string(lives);
							SDL_SetWindowTitle(window, clearTitle.c_str());
						}
					}
				}
			}
		}

		//幽霊を一定間隔で動かす
		Uint32 currentTime = SDL_GetTicks();
		if (!gameOver && !gameClear && currentTime - lastGhostMove > ghostMoveDelay) {
			for (int i = 0; i < NUM_GHOSTS; i++) {
				moveGhost(i);
			}
			lastGhostMove = currentTime;
		}

		//アニメーション更新
		if (currentTime - lastAnimTime > ANIM_SPEED) {
			mouthOpen = !mouthOpen;
			lastAnimTime = currentTime;
		}

		//当たり判定
		for (int i = 0; i < NUM_GHOSTS;i++) {
			if (pacRow == ghostRow[i] && pacCol == ghostCol[i]) {
				lives--;
				if (lives <= 0) {
					gameOver = true;
					if (!gameOverSoundPlayed) {
						Mix_PlayChannel(-1, gameOverSound, 0);
						gameOverSoundPlayed = true;
					}
					SDL_SetWindowTitle(window, "Pac-Man - GAME OVER");
				}else {
					//残機があれば位置だけリセット
					pacRow = 1;
					pacCol = 1;

					int initialGhostRow[NUM_GHOSTS] = { 13,1,13 };
					int initialGhostCol[NUM_GHOSTS] = { 18,18,1 };
					for (int j = 0; j < NUM_GHOSTS; j++) {
						ghostRow[j] = initialGhostRow[j];
						ghostCol[j] = initialGhostCol[j];
					}

					string title = "Pac-Man - Score: " + to_string(score)  +" Lives: " + to_string(lives);
					SDL_SetWindowTitle(window, title.c_str());
				}
				break;
			}
		}

		// 背景を黒でクリア
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		// 迷路を描画
		for (int row = 0; row < MAZE_HEIGHT; row++) {
			for (int col = 0; col < MAZE_WIDTH; col++) {
				SDL_Rect tile = {
				col * TILE_SIZE,
				row * TILE_SIZE,
				TILE_SIZE,
				TILE_SIZE
				};

				if (maze[row][col] == '1') {
					// 壁は青
					SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
					SDL_RenderFillRect(renderer, &tile);
				}
				else if (maze[row][col] == '0') {
					// ドットは小さい黄色の四角で表現
					SDL_Rect dot = {
					col * TILE_SIZE + TILE_SIZE / 2 - 2,
					row * TILE_SIZE + TILE_SIZE / 2 - 2,
					4, 4
					};
					SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
					SDL_RenderFillRect(renderer, &dot);
				}
			}
		}

		//パックマンを描画
		SDL_Rect pacman = {
			pacCol * TILE_SIZE,
			pacRow * TILE_SIZE,
			TILE_SIZE,
			TILE_SIZE
		};
		SDL_Texture* currentTexture = mouthOpen ? pacmanOpenTexture : pacmanClosedTexture;
		SDL_RenderCopyEx(renderer, currentTexture, nullptr, &pacman,pacDirection,nullptr,SDL_FLIP_NONE);

		//幽霊を描画
		for (int i = 0; i < NUM_GHOSTS;i++) {
			SDL_Rect ghost = {
				ghostCol[i] * TILE_SIZE + 4,
				ghostRow[i] * TILE_SIZE + 4,
				TILE_SIZE - 8,
				TILE_SIZE - 8
			};
			SDL_RenderCopy(renderer, ghostTexture[i], nullptr, &ghost);
		}
		if (gameOver) {
			SDL_Color red = { 255,0,0,255 };
			renderText(renderer, font, "GAME OVER", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, red);
		}
		else if (gameClear) {
			SDL_Color yellow = { 255,255,0,255 };
			renderText(renderer, font, "CLEAR", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, yellow);
		}
		SDL_RenderPresent(renderer);
	}
	Mix_FreeChunk(dotSound);
	Mix_FreeChunk(gameOverSound);
	Mix_FreeChunk(gameClearSound);
	Mix_FreeChunk(gameStartSound);
	Mix_CloseAudio();
	TTF_CloseFont(font);
	TTF_Quit();
	SDL_DestroyTexture(pacmanOpenTexture);
	SDL_DestroyTexture(pacmanClosedTexture);
	for (int i = 0; i < NUM_GHOSTS;i++) {
		SDL_DestroyTexture(ghostTexture[i]);
	}
	IMG_Quit();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
