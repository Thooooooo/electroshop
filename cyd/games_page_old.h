#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

extern TFT_eSPI     tft;
extern TFT_eSprite  gameCanvas;
extern bool         canvasReady;
extern TFT_eSPI*    gameTarget;
extern bool         needRedraw;
extern int          currentPage;

// Redirect all game drawing to canvas (if available) or tft
#define GDRAW (*gameTarget)

extern void beepOK();
extern void beepFail();
extern void beepTick();

int currentGame = -1; // -1 = launcher, 0-9 = active game index

static const char* GAME_NAMES[10] = {
  "Snake","Pong","Breakout","TicTacToe","SimonSays",
  "FlappyBird","2048","SpaceInvaders","MemoryFlip","Reaction"
};

// ─────────────────────────────────────────────────────────────────────────────
// BACK BUTTON HELPER
// ─────────────────────────────────────────────────────────────────────────────
static void drawGameBackBtn() {
  GDRAW.fillRect(0, 0, 62, 28, C_HEADER);
  GDRAW.drawRect(0, 0, 62, 28, C_GRAY);
  GDRAW.setTextColor(C_WHITE, C_HEADER);
  GDRAW.setTextSize(1);
  GDRAW.setCursor(4, 10);
  GDRAW.print("<BACK");
}

// ─────────────────────────────────────────────────────────────────────────────
// GAME 0: SNAKE
// ─────────────────────────────────────────────────────────────────────────────
struct SnakeState {
  int8_t sx[200], sy[200];
  int len;
  int8_t fx, fy;
  int8_t dx, dy;
  int score;
  bool gameOver;
  unsigned long lastMove;
  int speed;
  bool started;
};
static SnakeState snake;

static void snakeInit() {
  memset(&snake, 0, sizeof(snake));
  snake.len = 3;
  snake.sx[0]=15; snake.sy[0]=9;
  snake.sx[1]=14; snake.sy[1]=9;
  snake.sx[2]=13; snake.sy[2]=9;
  snake.dx=1; snake.dy=0;
  snake.fx = random(0,30); snake.fy = random(0,18);
  snake.score=0; snake.gameOver=false;
  snake.speed=300; snake.started=true;
  snake.lastMove=millis();
  GDRAW.fillScreen(C_BG);
  GDRAW.drawRect(0, 32, 480, 288, C_GRAY);
  drawGameBackBtn();
  GDRAW.setTextColor(C_WHITE, C_BG);
  GDRAW.setTextSize(1);
  GDRAW.setCursor(80, 10);
  GDRAW.print("SNAKE  Score: 0");
  for(int i=0;i<snake.len;i++)
    GDRAW.fillRect(snake.sx[i]*16, 32+snake.sy[i]*16, 15, 15, i==0 ? C_GREEN : C_DKGREEN);
  GDRAW.fillRect(snake.fx*16, 32+snake.fy*16, 15, 15, C_RED);
}

static void snakeShowGameOver() {
  beepFail();
  GDRAW.fillRect(140,120,200,80,C_HEADER);
  GDRAW.drawRect(140,120,200,80,C_RED);
  GDRAW.setTextColor(C_RED,C_HEADER); GDRAW.setTextSize(2);
  GDRAW.setCursor(160,130); GDRAW.print("GAME OVER");
  GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
  GDRAW.setCursor(155,160); GDRAW.print("Score: "); GDRAW.print(snake.score);
  GDRAW.setCursor(148,178); GDRAW.print("Tap BACK to exit");
}

static void snakeTick() {
  if(!snake.started || snake.gameOver) return;
  unsigned long now = millis();
  if(now - snake.lastMove < (unsigned long)snake.speed) return;
  snake.lastMove = now;

  GDRAW.fillRect(snake.sx[snake.len-1]*16, 32+snake.sy[snake.len-1]*16, 15, 15, C_BG);

  for(int i=snake.len-1;i>0;i--) {
    snake.sx[i]=snake.sx[i-1];
    snake.sy[i]=snake.sy[i-1];
  }
  snake.sx[0]+=snake.dx;
  snake.sy[0]+=snake.dy;

  if(snake.sx[0]<0||snake.sx[0]>=30||snake.sy[0]<0||snake.sy[0]>=18) {
    snake.gameOver=true; snakeShowGameOver(); return;
  }
  for(int i=1;i<snake.len;i++) {
    if(snake.sx[0]==snake.sx[i]&&snake.sy[0]==snake.sy[i]) {
      snake.gameOver=true; snakeShowGameOver(); return;
    }
  }

  if(snake.sx[0]==snake.fx&&snake.sy[0]==snake.fy) {
    snake.score++;
    beepTick();
    if(snake.len<200) snake.len++;
    bool valid=false;
    while(!valid) {
      snake.fx=random(0,30); snake.fy=random(0,18);
      valid=true;
      for(int i=0;i<snake.len;i++)
        if(snake.sx[i]==snake.fx&&snake.sy[i]==snake.fy){valid=false;break;}
    }
    GDRAW.fillRect(snake.fx*16, 32+snake.fy*16, 15, 15, C_RED);
    if(snake.score%5==0 && snake.speed>100) snake.speed-=20;
    GDRAW.fillRect(80,2,300,28,C_BG);
    GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
    GDRAW.setCursor(80,10); GDRAW.print("SNAKE  Score: "); GDRAW.print(snake.score);
  }

  GDRAW.fillRect(snake.sx[0]*16, 32+snake.sy[0]*16, 15, 15, C_GREEN);
  if(snake.len>1) GDRAW.fillRect(snake.sx[1]*16, 32+snake.sy[1]*16, 15, 15, C_DKGREEN);
}

static void snakeTouch(uint16_t tx, uint16_t ty) {
  if(snake.gameOver) return;
  if(ty < 120 && snake.dy==0)      { snake.dx=0; snake.dy=-1; }
  else if(ty > 200 && snake.dy==0) { snake.dx=0; snake.dy=1; }
  else if(tx < 160 && snake.dx==0) { snake.dx=-1; snake.dy=0; }
  else if(tx > 320 && snake.dx==0) { snake.dx=1;  snake.dy=0; }
}

// ─────────────────────────────────────────────────────────────────────────────
// GAME 1: PONG
// ─────────────────────────────────────────────────────────────────────────────
#define PONG_PADDLE_H 60
#define PONG_PADDLE_W 8
#define PONG_BALL_R   5
#define PONG_TOP      32
#define PONG_BOTTOM   320

struct PongState {
  float bx, by, bdx, bdy;
  int py, cy;
  int pScore, cScore;
  bool gameOver;
  bool started;
  unsigned long lastTick;
};
static PongState pong;

static void pongInit() {
  pong.bx=240; pong.by=176;
  pong.bdx=4; pong.bdy=3;
  pong.py=176; pong.cy=176;
  pong.pScore=0; pong.cScore=0;
  pong.gameOver=false; pong.started=true;
  pong.lastTick=millis();
  GDRAW.fillScreen(C_BG);
  drawGameBackBtn();
  for(int y=PONG_TOP;y<PONG_BOTTOM;y+=8) GDRAW.fillRect(239,y,2,4,C_GRAY);
  GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(2);
  GDRAW.setCursor(168,4); GDRAW.print("0 : 0");
  GDRAW.fillRect(10, pong.py-PONG_PADDLE_H/2, PONG_PADDLE_W, PONG_PADDLE_H, C_CYAN);
  GDRAW.fillRect(462, pong.cy-PONG_PADDLE_H/2, PONG_PADDLE_W, PONG_PADDLE_H, C_RED);
  GDRAW.fillCircle((int)pong.bx,(int)pong.by,PONG_BALL_R,C_WHITE);
}

static void pongTick() {
  if(!pong.started||pong.gameOver) return;
  if(millis()-pong.lastTick < 16) return;
  pong.lastTick=millis();

  GDRAW.fillCircle((int)pong.bx,(int)pong.by,PONG_BALL_R,C_BG);
  GDRAW.fillRect(10, pong.py-PONG_PADDLE_H/2, PONG_PADDLE_W, PONG_PADDLE_H, C_BG);
  GDRAW.fillRect(462, pong.cy-PONG_PADDLE_H/2, PONG_PADDLE_W, PONG_PADDLE_H, C_BG);

  pong.bx+=pong.bdx; pong.by+=pong.bdy;

  if(pong.by<=PONG_TOP+PONG_BALL_R)    { pong.by=PONG_TOP+PONG_BALL_R;    pong.bdy=-pong.bdy; }
  if(pong.by>=PONG_BOTTOM-PONG_BALL_R) { pong.by=PONG_BOTTOM-PONG_BALL_R; pong.bdy=-pong.bdy; }

  if(pong.bx<=18+PONG_BALL_R && pong.bdx<0) {
    if(pong.by>=pong.py-PONG_PADDLE_H/2-PONG_BALL_R && pong.by<=pong.py+PONG_PADDLE_H/2+PONG_BALL_R) {
      pong.bdx = -pong.bdx * 1.05f;
      if(pong.bdx > 8) pong.bdx=8;
      beepTick();
    }
  }
  if(pong.bx>=462-PONG_BALL_R && pong.bdx>0) {
    if(pong.by>=pong.cy-PONG_PADDLE_H/2-PONG_BALL_R && pong.by<=pong.cy+PONG_PADDLE_H/2+PONG_BALL_R) {
      pong.bdx=-pong.bdx;
    }
  }

  if(pong.by > pong.cy+5) pong.cy+=3;
  else if(pong.by < pong.cy-5) pong.cy-=3;
  pong.cy=constrain(pong.cy, PONG_TOP+PONG_PADDLE_H/2, PONG_BOTTOM-PONG_PADDLE_H/2);

  bool scored=false;
  if(pong.bx<0)   { pong.cScore++; pong.bx=240; pong.by=176; pong.bdx=4;  pong.bdy=3; scored=true; }
  if(pong.bx>480) { pong.pScore++; pong.bx=240; pong.by=176; pong.bdx=-4; pong.bdy=3; scored=true; }

  if(pong.pScore>=7||pong.cScore>=7) {
    pong.gameOver=true;
    GDRAW.fillRect(140,120,200,80,C_HEADER);
    GDRAW.drawRect(140,120,200,80,C_YELLOW);
    GDRAW.setTextColor(C_YELLOW,C_HEADER); GDRAW.setTextSize(2);
    GDRAW.setCursor(158,130);
    GDRAW.print(pong.pScore>=7?"YOU WIN!":"CPU WINS");
    GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
    GDRAW.setCursor(148,175); GDRAW.print("Tap BACK to exit");
    return;
  }

  if(scored) {
    GDRAW.fillRect(140,2,200,28,C_BG);
    GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(2);
    GDRAW.setCursor(168,4);
    GDRAW.print(pong.pScore); GDRAW.print(" : "); GDRAW.print(pong.cScore);
  }

  for(int y=PONG_TOP;y<PONG_BOTTOM;y+=8) GDRAW.fillRect(239,y,2,4,C_GRAY);
  GDRAW.fillCircle((int)pong.bx,(int)pong.by,PONG_BALL_R,C_WHITE);
  GDRAW.fillRect(10, pong.py-PONG_PADDLE_H/2, PONG_PADDLE_W, PONG_PADDLE_H, C_CYAN);
  GDRAW.fillRect(462, pong.cy-PONG_PADDLE_H/2, PONG_PADDLE_W, PONG_PADDLE_H, C_RED);
}

static void pongTouch(uint16_t tx, uint16_t ty) {
  if(tx < 240 && ty > PONG_TOP)
    pong.py = constrain((int)ty, PONG_TOP+PONG_PADDLE_H/2, PONG_BOTTOM-PONG_PADDLE_H/2);
}

// ─────────────────────────────────────────────────────────────────────────────
// GAME 2: BREAKOUT
// ─────────────────────────────────────────────────────────────────────────────
#define BRK_COLS   10
#define BRK_ROWS   5
#define BRK_BW     44
#define BRK_BH     16
#define BRK_BALL_R 5

struct BreakoutState {
  bool bricks[BRK_ROWS][BRK_COLS];
  float bx, by, bdx, bdy;
  float px;
  int lives, score;
  bool gameOver, won, started;
  unsigned long lastTick;
};
static BreakoutState brk;
static uint16_t brkColors[BRK_ROWS] = {C_RED, C_ORANGE, C_YELLOW, C_GREEN, C_CYAN};

static void breakoutDrawHUD() {
  GDRAW.fillRect(65,2,350,28,C_BG);
  GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(70,10);
  GDRAW.print("Lives:"); GDRAW.print(brk.lives);
  GDRAW.print("  Score:"); GDRAW.print(brk.score);
}

static void breakoutInit() {
  for(int r=0;r<BRK_ROWS;r++) for(int c=0;c<BRK_COLS;c++) brk.bricks[r][c]=true;
  brk.bx=240; brk.by=260;
  brk.bdx=3; brk.bdy=-4;
  brk.px=240;
  brk.lives=3; brk.score=0;
  brk.gameOver=false; brk.won=false; brk.started=true;
  brk.lastTick=millis();
  GDRAW.fillScreen(C_BG);
  drawGameBackBtn();
  for(int r=0;r<BRK_ROWS;r++) {
    for(int c=0;c<BRK_COLS;c++) {
      int bx=10+c*46, by=40+r*18;
      GDRAW.fillRect(bx+1,by+1,42,14,brkColors[r]);
      GDRAW.drawRect(bx,by,44,16,C_BG);
    }
  }
  GDRAW.fillRect((int)brk.px-40,295,80,12,C_CYAN);
  GDRAW.fillCircle((int)brk.bx,(int)brk.by,BRK_BALL_R,C_WHITE);
  breakoutDrawHUD();
}

static void breakoutTick() {
  if(!brk.started||brk.gameOver||brk.won) return;
  if(millis()-brk.lastTick<16) return;
  brk.lastTick=millis();

  GDRAW.fillCircle((int)brk.bx,(int)brk.by,BRK_BALL_R,C_BG);
  GDRAW.fillRect((int)brk.px-40,295,80,12,C_BG);

  brk.bx+=brk.bdx; brk.by+=brk.bdy;

  if(brk.bx<=BRK_BALL_R)        { brk.bx=BRK_BALL_R;        brk.bdx=-brk.bdx; }
  if(brk.bx>=480-BRK_BALL_R)    { brk.bx=480-BRK_BALL_R;    brk.bdx=-brk.bdx; }
  if(brk.by<=32+BRK_BALL_R)     { brk.by=32+BRK_BALL_R;     brk.bdy=-brk.bdy; }

  // Paddle collision
  if(brk.bdy>0 && brk.by>=290-BRK_BALL_R && brk.by<=302 &&
     brk.bx>=brk.px-40 && brk.bx<=brk.px+40) {
    brk.bdy=-fabsf(brk.bdy);
    brk.bdx = (brk.bx - brk.px) * 0.1f;
    if(brk.bdx>5)  brk.bdx=5;
    if(brk.bdx<-5) brk.bdx=-5;
    if(fabsf(brk.bdx)<1.0f) brk.bdx=(brk.bdx>=0)?1.0f:-1.0f;
  }

  // Ball lost
  if(brk.by>325) {
    brk.lives--;
    breakoutDrawHUD();
    if(brk.lives<=0) {
      brk.gameOver=true;
      GDRAW.fillRect(140,120,200,80,C_HEADER);
      GDRAW.drawRect(140,120,200,80,C_RED);
      GDRAW.setTextColor(C_RED,C_HEADER); GDRAW.setTextSize(2);
      GDRAW.setCursor(160,130); GDRAW.print("GAME OVER");
      GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
      GDRAW.setCursor(155,160); GDRAW.print("Score: "); GDRAW.print(brk.score);
      GDRAW.setCursor(148,178); GDRAW.print("Tap BACK to exit");
      return;
    }
    brk.bx=240; brk.by=260; brk.bdx=3; brk.bdy=-4;
  }

  // Brick collision
  int bRow = ((int)brk.by - 40) / 18;
  int bCol = ((int)brk.bx - 10) / 46;
  if(bRow>=0 && bRow<BRK_ROWS && bCol>=0 && bCol<BRK_COLS) {
    int bxp=10+bCol*46, byp=40+bRow*18;
    if(brk.by>=byp && brk.by<=byp+16 && brk.bx>=bxp && brk.bx<=bxp+44) {
      if(brk.bricks[bRow][bCol]) {
        brk.bricks[bRow][bCol]=false;
        GDRAW.fillRect(bxp,byp,44,16,C_BG);
        brk.bdy=-brk.bdy;
        brk.score+=10;
        beepTick();
        breakoutDrawHUD();
        bool allGone=true;
        for(int r=0;r<BRK_ROWS&&allGone;r++)
          for(int c=0;c<BRK_COLS&&allGone;c++)
            if(brk.bricks[r][c]) allGone=false;
        if(allGone) {
          brk.won=true;
          GDRAW.fillRect(140,120,200,80,C_HEADER);
          GDRAW.drawRect(140,120,200,80,C_GREEN);
          GDRAW.setTextColor(C_GREEN,C_HEADER); GDRAW.setTextSize(2);
          GDRAW.setCursor(178,130); GDRAW.print("YOU WIN!");
          GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
          GDRAW.setCursor(155,160); GDRAW.print("Score: "); GDRAW.print(brk.score);
          GDRAW.setCursor(148,178); GDRAW.print("Tap BACK to exit");
          return;
        }
      }
    }
  }

  GDRAW.fillCircle((int)brk.bx,(int)brk.by,BRK_BALL_R,C_WHITE);
  GDRAW.fillRect((int)brk.px-40,295,80,12,C_CYAN);
}

static void breakoutTouch(uint16_t tx, uint16_t ty) {
  brk.px = constrain((int)tx, 40, 440);
}

// ─────────────────────────────────────────────────────────────────────────────
// GAME 3: TIC-TAC-TOE
// ─────────────────────────────────────────────────────────────────────────────
#define TIC_X0  120
#define TIC_Y0  50
#define TIC_CW  80
#define TIC_CH  80

struct TicState {
  int8_t board[9];
  bool playerTurn, gameOver, started;
  int winner;
};
static TicState tic;

static int ticCheckWin(int8_t* b, int8_t player) {
  const int lines[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
  for(int i=0;i<8;i++)
    if(b[lines[i][0]]==player && b[lines[i][1]]==player && b[lines[i][2]]==player) return i;
  return -1;
}

static void ticDrawBoard() {
  GDRAW.fillRect(TIC_X0-5, TIC_Y0-5, TIC_CW*3+10, TIC_CH*3+10, C_BG);
  for(int i=1;i<3;i++) {
    GDRAW.drawFastVLine(TIC_X0+i*TIC_CW, TIC_Y0, TIC_CH*3, C_GRAY);
    GDRAW.drawFastHLine(TIC_X0, TIC_Y0+i*TIC_CH, TIC_CW*3, C_GRAY);
  }
}

static void ticDrawCell(int idx) {
  int r=idx/3, c=idx%3;
  int cx=TIC_X0+c*TIC_CW+TIC_CW/2, cy=TIC_Y0+r*TIC_CH+TIC_CH/2;
  if(tic.board[idx]==1) {
    int d=28;
    GDRAW.drawLine(cx-d,cy-d,cx+d,cy+d,C_CYAN);
    GDRAW.drawLine(cx+d,cy-d,cx-d,cy+d,C_CYAN);
    GDRAW.drawLine(cx-d+1,cy-d,cx+d+1,cy+d,C_CYAN);
    GDRAW.drawLine(cx+d+1,cy-d,cx-d+1,cy+d,C_CYAN);
  } else if(tic.board[idx]==2) {
    GDRAW.drawCircle(cx,cy,28,C_RED);
    GDRAW.drawCircle(cx,cy,27,C_RED);
  }
}

static void ticCpuMove() {
  // Try to win
  for(int i=0;i<9;i++) {
    if(tic.board[i]==0) {
      tic.board[i]=2;
      if(ticCheckWin(tic.board,2)>=0) { ticDrawCell(i); return; }
      tic.board[i]=0;
    }
  }
  // Block player
  for(int i=0;i<9;i++) {
    if(tic.board[i]==0) {
      tic.board[i]=1;
      if(ticCheckWin(tic.board,1)>=0) { tic.board[i]=2; ticDrawCell(i); return; }
      tic.board[i]=0;
    }
  }
  // Center
  if(tic.board[4]==0) { tic.board[4]=2; ticDrawCell(4); return; }
  // First available
  for(int i=0;i<9;i++) {
    if(tic.board[i]==0) { tic.board[i]=2; ticDrawCell(i); return; }
  }
}

static void ticInit() {
  memset(&tic,0,sizeof(tic));
  tic.playerTurn=true; tic.started=true;
  GDRAW.fillScreen(C_BG);
  drawGameBackBtn();
  GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(80,10); GDRAW.print("TIC-TAC-TOE  X=You  O=CPU");
  ticDrawBoard();
}

static void ticTouch(uint16_t tx, uint16_t ty) {
  if(tic.gameOver) { ticInit(); return; }
  if(!tic.playerTurn) return;
  if(tx<TIC_X0 || tx>=TIC_X0+TIC_CW*3 || ty<TIC_Y0 || ty>=TIC_Y0+TIC_CH*3) return;
  int c=(tx-TIC_X0)/TIC_CW;
  int r=(ty-TIC_Y0)/TIC_CH;
  int idx=r*3+c;
  if(tic.board[idx]!=0) return;
  tic.board[idx]=1;
  ticDrawCell(idx);

  if(ticCheckWin(tic.board,1)>=0) {
    tic.gameOver=true; tic.winner=1;
    GDRAW.fillRect(90,295,300,22,C_DKGREEN);
    GDRAW.drawRect(90,295,300,22,C_GREEN);
    GDRAW.setTextColor(C_GREEN,C_DKGREEN); GDRAW.setTextSize(2);
    GDRAW.setCursor(130,298); GDRAW.print("YOU WIN! Tap restart");
    return;
  }
  bool full=true; for(int i=0;i<9;i++) if(tic.board[i]==0){full=false;break;}
  if(full) {
    tic.gameOver=true; tic.winner=3;
    GDRAW.fillRect(90,295,300,22,C_HEADER);
    GDRAW.drawRect(90,295,300,22,C_YELLOW);
    GDRAW.setTextColor(C_YELLOW,C_HEADER); GDRAW.setTextSize(2);
    GDRAW.setCursor(150,298); GDRAW.print("DRAW! Tap restart");
    return;
  }

  tic.playerTurn=false;
  ticCpuMove();

  if(ticCheckWin(tic.board,2)>=0) {
    tic.gameOver=true; tic.winner=2;
    GDRAW.fillRect(90,295,300,22,C_DKRED);
    GDRAW.drawRect(90,295,300,22,C_RED);
    GDRAW.setTextColor(C_RED,C_DKRED); GDRAW.setTextSize(2);
    GDRAW.setCursor(130,298); GDRAW.print("CPU WINS! Tap restart");
    return;
  }
  full=true; for(int i=0;i<9;i++) if(tic.board[i]==0){full=false;break;}
  if(full) {
    tic.gameOver=true; tic.winner=3;
    GDRAW.fillRect(90,295,300,22,C_HEADER);
    GDRAW.drawRect(90,295,300,22,C_YELLOW);
    GDRAW.setTextColor(C_YELLOW,C_HEADER); GDRAW.setTextSize(2);
    GDRAW.setCursor(150,298); GDRAW.print("DRAW! Tap restart");
    return;
  }
  tic.playerTurn=true;
}

// ─────────────────────────────────────────────────────────────────────────────
// GAME 4: SIMON SAYS
// ─────────────────────────────────────────────────────────────────────────────
#define SIMON_MAX 100

struct SimonState {
  uint8_t sequence[SIMON_MAX];
  int seqLen;
  int playerPos;
  bool showing;
  int showIdx;
  unsigned long showMs;
  bool lit;
  int litButton;
  bool gameOver;
  bool started;
  bool waitingInput;
  int level;
};
static SimonState simon;

// 4 buttons in 2×2 grid each 240×144 starting at y=32
// btn 0=TL red, 1=TR green, 2=BL blue, 3=BR yellow
static const uint16_t SIMON_COLORS_DIM[4]  = {C_DKRED,  C_DKGREEN, C_DKBLUE, 0x8400};
static const uint16_t SIMON_COLORS_LIT[4]  = {C_RED,     C_GREEN,   C_BLUE,   C_YELLOW};
static const char*    SIMON_LABELS[4]       = {"1","2","3","4"};

static void simonGetBtnRect(int btn, int* bx, int* by, int* bw, int* bh) {
  *bw=238; *bh=140;
  *bx=(btn%2)*240+1;
  *by=32+(btn/2)*143+1;
}

static void simonDrawBtn(int btn, bool lit) {
  int bx,by,bw,bh;
  simonGetBtnRect(btn,&bx,&by,&bw,&bh);
  uint16_t col = lit ? SIMON_COLORS_LIT[btn] : SIMON_COLORS_DIM[btn];
  GDRAW.fillRect(bx,by,bw,bh,col);
  GDRAW.drawRect(bx-1,by-1,bw+2,bh+2,C_GRAY);
  GDRAW.setTextColor(C_WHITE,col);
  GDRAW.setTextSize(4);
  GDRAW.setCursor(bx+bw/2-12, by+bh/2-16);
  GDRAW.print(SIMON_LABELS[btn]);
}

static void simonDrawAll(int litBtn) {
  for(int i=0;i<4;i++) simonDrawBtn(i, i==litBtn);
}

static void simonInit() {
  memset(&simon,0,sizeof(simon));
  simon.seqLen=1;
  simon.sequence[0]=random(0,4);
  simon.playerPos=0;
  simon.showing=true;
  simon.showIdx=0;
  simon.showMs=millis()+600;
  simon.lit=false;
  simon.litButton=-1;
  simon.gameOver=false;
  simon.started=true;
  simon.waitingInput=false;
  simon.level=1;
  GDRAW.fillScreen(C_BG);
  drawGameBackBtn();
  GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(80,10); GDRAW.print("SIMON SAYS  Level: 1");
  // Draw divider lines
  GDRAW.drawFastVLine(240,32,288,C_GRAY);
  GDRAW.drawFastHLine(0,175,480,C_GRAY);
  simonDrawAll(-1);
}

static void simonTick() {
  if(!simon.started||simon.gameOver) return;
  unsigned long now=millis();

  if(simon.showing) {
    if(!simon.lit) {
      // Waiting before lighting next
      if(now>=simon.showMs) {
        simon.lit=true;
        simon.litButton=simon.sequence[simon.showIdx];
        simonDrawBtn(simon.litButton, true);
        simon.showMs=now+500; // lit duration
      }
    } else {
      // Currently lit, wait then unlit
      if(now>=simon.showMs) {
        simonDrawBtn(simon.litButton, false);
        simon.lit=false;
        simon.showIdx++;
        if(simon.showIdx>=simon.seqLen) {
          // Done showing
          simon.showing=false;
          simon.waitingInput=true;
          simon.playerPos=0;
          simon.litButton=-1;
        } else {
          simon.showMs=now+300; // pause between flashes
        }
      }
    }
  }
}

static void simonTouch(uint16_t tx, uint16_t ty) {
  if(simon.gameOver||simon.showing||!simon.waitingInput) return;
  // Determine which button tapped
  int btn=-1;
  if(tx<240 && ty<175)      btn=0; // TL red
  else if(tx>=240 && ty<175) btn=1; // TR green
  else if(tx<240 && ty>=175) btn=2; // BL blue
  else if(tx>=240 && ty>=175) btn=3;// BR yellow

  if(btn<0) return;

  // Flash the button
  simonDrawBtn(btn,true);
  delay(80);
  simonDrawBtn(btn,false);

  if(btn!=simon.sequence[simon.playerPos]) {
    // Wrong!
    simon.gameOver=true;
    GDRAW.fillRect(100,120,280,80,C_HEADER);
    GDRAW.drawRect(100,120,280,80,C_RED);
    GDRAW.setTextColor(C_RED,C_HEADER); GDRAW.setTextSize(2);
    GDRAW.setCursor(140,132); GDRAW.print("WRONG! Level:");
    GDRAW.print(simon.level);
    GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
    GDRAW.setCursor(118,175); GDRAW.print("Tap BACK to exit");
    return;
  }

  simon.playerPos++;
  if(simon.playerPos>=simon.seqLen) {
    // Round complete — advance
    simon.level++;
    if(simon.seqLen<SIMON_MAX) {
      simon.sequence[simon.seqLen]=random(0,4);
      simon.seqLen++;
    }
    simon.waitingInput=false;
    simon.showing=true;
    simon.showIdx=0;
    simon.lit=false;
    simon.showMs=millis()+800;
    // Update HUD
    GDRAW.fillRect(65,2,350,28,C_BG);
    GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
    GDRAW.setCursor(80,10); GDRAW.print("SIMON SAYS  Level: "); GDRAW.print(simon.level);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// GAME 5: FLAPPY BIRD
// ─────────────────────────────────────────────────────────────────────────────
#define FLAPPY_GRAVITY 0.35f
#define FLAPPY_FLAP   -5.5f
#define FLAPPY_PIPE_W  40
#define FLAPPY_GAP_H   85
#define BIRD_X         80
#define BIRD_W         20
#define BIRD_H         14
#define FLAPPY_TOP     32
#define FLAPPY_BOTTOM  320
#define FLAPPY_PIPE_SPD 2.5f

struct FlappyState {
  float birdY, birdVY;
  float pipeX[2];
  int   pipeGap[2]; // y of gap top
  int   score;
  bool  gameOver, started;
  unsigned long lastTick;
  bool  scored[2];
};
static FlappyState flappy;

static void flappyDrawPipe(float px, int gapTop, uint16_t col) {
  int x=(int)px;
  if(x>480||x+FLAPPY_PIPE_W<0) return;
  // Top pipe
  GDRAW.fillRect(x, FLAPPY_TOP, FLAPPY_PIPE_W, gapTop-FLAPPY_TOP, col);
  // Bottom pipe
  int botY=gapTop+FLAPPY_GAP_H;
  GDRAW.fillRect(x, botY, FLAPPY_PIPE_W, FLAPPY_BOTTOM-botY, col);
}

static void flappyDrawBird(float by, uint16_t col) {
  GDRAW.fillRect(BIRD_X, (int)by, BIRD_W, BIRD_H, col);
  if(col!=C_BG) {
    GDRAW.fillRect(BIRD_X+BIRD_W-4, (int)by+2, 6, 4, C_YELLOW); // beak
    GDRAW.fillRect(BIRD_X+BIRD_W-2, (int)by+1, 2, 2, C_WHITE);  // eye
  }
}

static int flappyRandGap() {
  return FLAPPY_TOP + 20 + random(0, (FLAPPY_BOTTOM-FLAPPY_TOP-40-FLAPPY_GAP_H));
}

static void flappyDrawScore() {
  GDRAW.fillRect(65,2,350,28,C_BG);
  GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(80,10); GDRAW.print("FLAPPY BIRD  Score: "); GDRAW.print(flappy.score);
}

static void flappyInit() {
  flappy.birdY=(FLAPPY_TOP+FLAPPY_BOTTOM)/2.0f;
  flappy.birdVY=0;
  flappy.pipeX[0]=380; flappy.pipeX[1]=570;
  flappy.pipeGap[0]=flappyRandGap();
  flappy.pipeGap[1]=flappyRandGap();
  flappy.score=0;
  flappy.gameOver=false; flappy.started=true;
  flappy.scored[0]=false; flappy.scored[1]=false;
  flappy.lastTick=millis();
  GDRAW.fillScreen(C_BG);
  drawGameBackBtn();
  flappyDrawScore();
  // Draw sky background line
  GDRAW.drawFastHLine(0,FLAPPY_TOP,480,C_GRAY);
  flappyDrawPipe(flappy.pipeX[0],flappy.pipeGap[0],C_DKGREEN);
  flappyDrawPipe(flappy.pipeX[1],flappy.pipeGap[1],C_DKGREEN);
  flappyDrawBird(flappy.birdY,C_YELLOW);
  GDRAW.setTextColor(C_LGRAY,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(180,150); GDRAW.print("TAP to flap!");
}

static void flappyTick() {
  if(!flappy.started||flappy.gameOver) return;
  if(millis()-flappy.lastTick<20) return;
  flappy.lastTick=millis();

  // Erase bird and pipes
  flappyDrawBird(flappy.birdY, C_BG);
  for(int p=0;p<2;p++) flappyDrawPipe(flappy.pipeX[p],flappy.pipeGap[p],C_BG);

  // Physics
  flappy.birdVY+=FLAPPY_GRAVITY;
  flappy.birdY+=flappy.birdVY;

  // Move pipes
  for(int p=0;p<2;p++) {
    flappy.pipeX[p]-=FLAPPY_PIPE_SPD;
    if(flappy.pipeX[p]+FLAPPY_PIPE_W < 0) {
      flappy.pipeX[p]=480+20;
      flappy.pipeGap[p]=flappyRandGap();
      flappy.scored[p]=false;
    }
    // Score when bird passes pipe
    if(!flappy.scored[p] && flappy.pipeX[p]+FLAPPY_PIPE_W < BIRD_X) {
      flappy.scored[p]=true;
      flappy.score++;
      flappyDrawScore();
    }
  }

  // Collision with walls
  if(flappy.birdY<FLAPPY_TOP || flappy.birdY+BIRD_H>FLAPPY_BOTTOM) {
    flappy.gameOver=true;
    GDRAW.fillRect(140,130,200,70,C_HEADER);
    GDRAW.drawRect(140,130,200,70,C_RED);
    GDRAW.setTextColor(C_RED,C_HEADER); GDRAW.setTextSize(2);
    GDRAW.setCursor(160,140); GDRAW.print("GAME OVER");
    GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
    GDRAW.setCursor(155,168); GDRAW.print("Score: "); GDRAW.print(flappy.score);
    GDRAW.setCursor(148,183); GDRAW.print("Tap BACK to exit");
    return;
  }

  // Collision with pipes
  for(int p=0;p<2;p++) {
    int px=(int)flappy.pipeX[p];
    int gapTop=flappy.pipeGap[p];
    int gapBot=gapTop+FLAPPY_GAP_H;
    int bx=BIRD_X, byr=(int)flappy.birdY;
    if(bx+BIRD_W>px && bx<px+FLAPPY_PIPE_W) {
      if(byr<gapTop || byr+BIRD_H>gapBot) {
        flappy.gameOver=true;
        GDRAW.fillRect(140,130,200,70,C_HEADER);
        GDRAW.drawRect(140,130,200,70,C_RED);
        GDRAW.setTextColor(C_RED,C_HEADER); GDRAW.setTextSize(2);
        GDRAW.setCursor(160,140); GDRAW.print("GAME OVER");
        GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
        GDRAW.setCursor(155,168); GDRAW.print("Score: "); GDRAW.print(flappy.score);
        GDRAW.setCursor(148,183); GDRAW.print("Tap BACK to exit");
        return;
      }
    }
  }

  // Redraw
  for(int p=0;p<2;p++) flappyDrawPipe(flappy.pipeX[p],flappy.pipeGap[p],C_DKGREEN);
  flappyDrawBird(flappy.birdY,C_YELLOW);
  GDRAW.drawFastHLine(0,FLAPPY_TOP,480,C_GRAY);
}

static void flappyTouch(uint16_t tx, uint16_t ty) {
  if(flappy.gameOver) return;
  flappy.birdVY=FLAPPY_FLAP;
}

// ─────────────────────────────────────────────────────────────────────────────
// GAME 6: 2048
// ─────────────────────────────────────────────────────────────────────────────
#define G2048_X0  4
#define G2048_Y0  38
#define G2048_CW  117
#define G2048_CH  68
#define G2048_GAP 3

struct G2048State {
  int  grid[4][4];
  int  score;
  bool gameOver, won, started;
  int  swipeStartX, swipeStartY;
  bool swipeActive;
};
static G2048State g2048;

static uint16_t g2048TileColor(int val) {
  switch(val) {
    case    0: return 0x2104;
    case    2: return 0x3186;
    case    4: return 0x4228;
    case    8: return C_ORANGE;
    case   16: return C_RED;
    case   32: return 0xF015;
    case   64: return C_DKRED;
    case  128: return C_YELLOW;
    case  256: return C_CYAN;
    case  512: return C_GREEN;
    case 1024: return C_MAGENTA;
    case 2048: return C_WHITE;
    default:   return C_PURPLE;
  }
}

static void g2048DrawTile(int row, int col) {
  int val=g2048.grid[row][col];
  int x=G2048_X0+col*(G2048_CW+G2048_GAP);
  int y=G2048_Y0+row*(G2048_CH+G2048_GAP);
  uint16_t bg=g2048TileColor(val);
  GDRAW.fillRect(x,y,G2048_CW,G2048_CH,bg);
  GDRAW.drawRect(x,y,G2048_CW,G2048_CH,C_GRAY);
  if(val>0) {
    GDRAW.setTextColor(C_WHITE,bg);
    GDRAW.setTextSize(val>=1000?2:val>=100?2:2);
    char buf[8]; itoa(val,buf,10);
    int tw=strlen(buf)*12;
    GDRAW.setCursor(x+(G2048_CW-tw)/2, y+G2048_CH/2-8);
    GDRAW.print(buf);
  }
}

static void g2048DrawAll() {
  GDRAW.fillRect(0,G2048_Y0-2,480,G2048_CH*4+G2048_GAP*3+4,C_BG);
  for(int r=0;r<4;r++) for(int c=0;c<4;c++) g2048DrawTile(r,c);
}

static void g2048DrawHUD() {
  GDRAW.fillRect(65,2,350,28,C_BG);
  GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(80,10); GDRAW.print("2048  Score: "); GDRAW.print(g2048.score);
}

static void g2048SpawnTile() {
  int empties[16][2]; int n=0;
  for(int r=0;r<4;r++) for(int c=0;c<4;c++)
    if(g2048.grid[r][c]==0) { empties[n][0]=r; empties[n][1]=c; n++; }
  if(n==0) return;
  int pick=random(0,n);
  g2048.grid[empties[pick][0]][empties[pick][1]]=(random(0,10)<9)?2:4;
}

static bool g2048MoveLeft() {
  bool moved=false;
  for(int r=0;r<4;r++) {
    int row[4]={0,0,0,0}; int idx=0;
    for(int c=0;c<4;c++) if(g2048.grid[r][c]) row[idx++]=g2048.grid[r][c];
    // Merge
    for(int i=0;i<3;i++) {
      if(row[i]&&row[i]==row[i+1]) {
        row[i]*=2; g2048.score+=row[i];
        for(int j=i+1;j<3;j++) row[j]=row[j+1];
        row[3]=0;
      }
    }
    for(int c=0;c<4;c++) {
      if(g2048.grid[r][c]!=row[c]) moved=true;
      g2048.grid[r][c]=row[c];
    }
  }
  return moved;
}

static bool g2048MoveRight() {
  bool moved=false;
  for(int r=0;r<4;r++) {
    int row[4]={0,0,0,0}; int idx=3;
    for(int c=3;c>=0;c--) if(g2048.grid[r][c]) row[idx--]=g2048.grid[r][c];
    for(int i=3;i>0;i--) {
      if(row[i]&&row[i]==row[i-1]) {
        row[i]*=2; g2048.score+=row[i];
        for(int j=i-1;j>0;j--) row[j]=row[j-1];
        row[0]=0;
      }
    }
    for(int c=0;c<4;c++) {
      if(g2048.grid[r][c]!=row[c]) moved=true;
      g2048.grid[r][c]=row[c];
    }
  }
  return moved;
}

static bool g2048MoveUp() {
  bool moved=false;
  for(int c=0;c<4;c++) {
    int col[4]={0,0,0,0}; int idx=0;
    for(int r=0;r<4;r++) if(g2048.grid[r][c]) col[idx++]=g2048.grid[r][c];
    for(int i=0;i<3;i++) {
      if(col[i]&&col[i]==col[i+1]) {
        col[i]*=2; g2048.score+=col[i];
        for(int j=i+1;j<3;j++) col[j]=col[j+1];
        col[3]=0;
      }
    }
    for(int r=0;r<4;r++) {
      if(g2048.grid[r][c]!=col[r]) moved=true;
      g2048.grid[r][c]=col[r];
    }
  }
  return moved;
}

static bool g2048MoveDown() {
  bool moved=false;
  for(int c=0;c<4;c++) {
    int col[4]={0,0,0,0}; int idx=3;
    for(int r=3;r>=0;r--) if(g2048.grid[r][c]) col[idx--]=g2048.grid[r][c];
    for(int i=3;i>0;i--) {
      if(col[i]&&col[i]==col[i-1]) {
        col[i]*=2; g2048.score+=col[i];
        for(int j=i-1;j>0;j--) col[j]=col[j-1];
        col[0]=0;
      }
    }
    for(int r=0;r<4;r++) {
      if(g2048.grid[r][c]!=col[r]) moved=true;
      g2048.grid[r][c]=col[r];
    }
  }
  return moved;
}

static bool g2048CheckWin() {
  for(int r=0;r<4;r++) for(int c=0;c<4;c++) if(g2048.grid[r][c]==2048) return true;
  return false;
}

static bool g2048HasMoves() {
  for(int r=0;r<4;r++) for(int c=0;c<4;c++) {
    if(g2048.grid[r][c]==0) return true;
    if(r<3&&g2048.grid[r][c]==g2048.grid[r+1][c]) return true;
    if(c<3&&g2048.grid[r][c]==g2048.grid[r][c+1]) return true;
  }
  return false;
}

static void g2048Init() {
  memset(&g2048,0,sizeof(g2048));
  g2048.started=true;
  g2048SpawnTile(); g2048SpawnTile();
  GDRAW.fillScreen(C_BG);
  drawGameBackBtn();
  g2048DrawHUD();
  GDRAW.setTextColor(C_LGRAY,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(320,10); GDRAW.print("Swipe to move");
  g2048DrawAll();
}

static void g2048Touch(uint16_t tx, uint16_t ty) {
  if(g2048.gameOver||g2048.won) return;
  if(!g2048.swipeActive) {
    g2048.swipeStartX=tx;
    g2048.swipeStartY=ty;
    g2048.swipeActive=true;
    return;
  }
  int dx=tx-g2048.swipeStartX;
  int dy=ty-g2048.swipeStartY;
  if(abs(dx)<30 && abs(dy)<30) return; // Not a swipe yet

  bool moved=false;
  if(abs(dx)>abs(dy)) {
    moved=(dx>0)?g2048MoveRight():g2048MoveLeft();
  } else {
    moved=(dy>0)?g2048MoveDown():g2048MoveUp();
  }
  g2048.swipeActive=false;

  if(moved) {
    g2048SpawnTile();
    g2048DrawAll();
    g2048DrawHUD();
    if(g2048CheckWin()&&!g2048.won) {
      g2048.won=true;
      GDRAW.fillRect(140,120,200,80,C_HEADER);
      GDRAW.drawRect(140,120,200,80,C_YELLOW);
      GDRAW.setTextColor(C_YELLOW,C_HEADER); GDRAW.setTextSize(2);
      GDRAW.setCursor(178,132); GDRAW.print("YOU WIN!");
      GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
      GDRAW.setCursor(148,170); GDRAW.print("Tap BACK to exit");
      return;
    }
    if(!g2048HasMoves()) {
      g2048.gameOver=true;
      GDRAW.fillRect(140,120,200,80,C_HEADER);
      GDRAW.drawRect(140,120,200,80,C_RED);
      GDRAW.setTextColor(C_RED,C_HEADER); GDRAW.setTextSize(2);
      GDRAW.setCursor(160,132); GDRAW.print("GAME OVER");
      GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
      GDRAW.setCursor(148,170); GDRAW.print("Tap BACK to exit");
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// GAME 7: SPACE INVADERS
// ─────────────────────────────────────────────────────────────────────────────
#define INV_COLS 8
#define INV_ROWS 3
#define INV_W    28
#define INV_H    18
#define INV_X0   20
#define INV_Y0   40
#define INV_XGAP 6
#define INV_YGAP 8
#define PLAYER_Y  295
#define PLAYER_W  32
#define PLAYER_H  12
#define BULLET_W   3
#define BULLET_H  10

struct InvaderState {
  bool alive[INV_ROWS][INV_COLS];
  int  alienOffX; // pixel offset from INV_X0
  int  alienOffY;
  int  dx;
  float bx, by;
  bool  bulletAlive;
  float abx[4], aby[4];
  bool  abullet[4];
  int   playerX;
  int   lives, score;
  bool  gameOver, won, started;
  unsigned long lastAlienTick;
  unsigned long lastBulletTick;
  int   alienSpeed;
  int   aliensLeft;
  bool  goDown;
  bool  shooting; // player wants to shoot
};
static InvaderState inv;

static void invDrawAlien(int row, int col, uint16_t col2) {
  int x=INV_X0+inv.alienOffX+col*(INV_W+INV_XGAP);
  int y=INV_Y0+inv.alienOffY+row*(INV_H+INV_YGAP);
  GDRAW.fillRect(x,y,INV_W,INV_H,col2);
  if(col2!=C_BG) {
    // Alien "shape": two antennae + body
    GDRAW.fillRect(x+2,y-2,3,3,col2);    // left antenna
    GDRAW.fillRect(x+INV_W-5,y-2,3,3,col2); // right antenna
    GDRAW.fillRect(x+4,y+2,INV_W-8,INV_H-4,C_BG); // body cutout eyes
    GDRAW.fillRect(x+6,y+4,4,4,col2);   // left eye
    GDRAW.fillRect(x+INV_W-10,y+4,4,4,col2); // right eye
  }
}

static void invDrawPlayer(uint16_t col) {
  int x=inv.playerX-PLAYER_W/2;
  GDRAW.fillRect(x,PLAYER_Y,PLAYER_W,PLAYER_H,col);
  if(col!=C_BG) {
    GDRAW.fillRect(x+PLAYER_W/2-2,PLAYER_Y-4,4,4,col); // "gun"
  }
}

static void invDrawHUD() {
  GDRAW.fillRect(65,2,350,28,C_BG);
  GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(80,10);
  GDRAW.print("INVADERS  Lives:"); GDRAW.print(inv.lives);
  GDRAW.print("  Score:"); GDRAW.print(inv.score);
}

static void invCountAliens() {
  inv.aliensLeft=0;
  for(int r=0;r<INV_ROWS;r++) for(int c=0;c<INV_COLS;c++)
    if(inv.alive[r][c]) inv.aliensLeft++;
}

static void invInit() {
  memset(&inv,0,sizeof(inv));
  for(int r=0;r<INV_ROWS;r++) for(int c=0;c<INV_COLS;c++) inv.alive[r][c]=true;
  inv.alienOffX=0; inv.alienOffY=0;
  inv.dx=2;
  inv.playerX=240;
  inv.lives=3; inv.score=0;
  inv.gameOver=false; inv.won=false; inv.started=true;
  inv.lastAlienTick=millis();
  inv.lastBulletTick=millis();
  inv.alienSpeed=600;
  inv.aliensLeft=INV_ROWS*INV_COLS;
  inv.bulletAlive=false;
  for(int i=0;i<4;i++) inv.abullet[i]=false;
  inv.goDown=false;
  GDRAW.fillScreen(C_BG);
  drawGameBackBtn();
  invDrawHUD();
  // Draw ground line
  GDRAW.drawFastHLine(0,PLAYER_Y+PLAYER_H+2,480,C_GRAY);
  // Draw aliens
  static const uint16_t alienColors[INV_ROWS]={C_GREEN,C_CYAN,C_MAGENTA};
  for(int r=0;r<INV_ROWS;r++)
    for(int c=0;c<INV_COLS;c++)
      invDrawAlien(r,c,alienColors[r]);
  invDrawPlayer(C_WHITE);
}

static void invTick() {
  if(!inv.started||inv.gameOver||inv.won) return;
  unsigned long now=millis();

  static const uint16_t alienColors[INV_ROWS]={C_GREEN,C_CYAN,C_MAGENTA};

  // Move aliens
  if(now-inv.lastAlienTick>=inv.alienSpeed) {
    inv.lastAlienTick=now;

    // Erase all aliens
    for(int r=0;r<INV_ROWS;r++)
      for(int c=0;c<INV_COLS;c++)
        if(inv.alive[r][c]) invDrawAlien(r,c,C_BG);

    if(inv.goDown) {
      inv.alienOffY+=12;
      inv.dx=-inv.dx;
      inv.goDown=false;
    } else {
      inv.alienOffX+=inv.dx*6;
    }

    // Check bounds (find leftmost/rightmost alive column)
    int minC=INV_COLS, maxC=-1;
    for(int r=0;r<INV_ROWS;r++) for(int c=0;c<INV_COLS;c++)
      if(inv.alive[r][c]) { if(c<minC)minC=c; if(c>maxC)maxC=c; }

    int leftEdge=INV_X0+inv.alienOffX+minC*(INV_W+INV_XGAP);
    int rightEdge=INV_X0+inv.alienOffX+maxC*(INV_W+INV_XGAP)+INV_W;

    if(rightEdge>=478||leftEdge<=2) inv.goDown=true;

    // Redraw aliens
    for(int r=0;r<INV_ROWS;r++)
      for(int c=0;c<INV_COLS;c++)
        if(inv.alive[r][c]) invDrawAlien(r,c,alienColors[r]);

    // Check if aliens reached player
    int maxRow=-1;
    for(int r=INV_ROWS-1;r>=0;r--) {
      for(int c=0;c<INV_COLS;c++) {
        if(inv.alive[r][c]) { maxRow=r; goto foundMaxRow; }
      }
    }
    foundMaxRow:
    if(maxRow>=0) {
      int botY=INV_Y0+inv.alienOffY+maxRow*(INV_H+INV_YGAP)+INV_H;
      if(botY>=PLAYER_Y) {
        inv.gameOver=true;
        GDRAW.fillRect(140,120,200,80,C_HEADER);
        GDRAW.drawRect(140,120,200,80,C_RED);
        GDRAW.setTextColor(C_RED,C_HEADER); GDRAW.setTextSize(2);
        GDRAW.setCursor(155,132); GDRAW.print("INVADED!");
        GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
        GDRAW.setCursor(155,162); GDRAW.print("Score: "); GDRAW.print(inv.score);
        GDRAW.setCursor(148,178); GDRAW.print("Tap BACK to exit");
        return;
      }
    }
    // Alien random shoot
    if(inv.aliensLeft>0 && random(0,100)<15) {
      // Find a random alive alien in the bottom row of each column
      int attempts=0;
      while(attempts<20) {
        int ac=random(0,INV_COLS);
        // find bottom alive in column
        for(int ar=INV_ROWS-1;ar>=0;ar--) {
          if(inv.alive[ar][ac]) {
            // Find free alien bullet slot
            for(int i=0;i<4;i++) {
              if(!inv.abullet[i]) {
                inv.abx[i]=INV_X0+inv.alienOffX+ac*(INV_W+INV_XGAP)+INV_W/2;
                inv.aby[i]=INV_Y0+inv.alienOffY+ar*(INV_H+INV_YGAP)+INV_H;
                inv.abullet[i]=true;
                goto abDone;
              }
            }
            break;
          }
        }
        abDone:
        attempts++;
        break;
      }
    }
    // Adjust speed based on aliens left
    inv.alienSpeed=max(100, 600-inv.aliensLeft*15);
  }

  // Move bullets
  if(now-inv.lastBulletTick>=16) {
    inv.lastBulletTick=now;

    // Player bullet
    if(inv.bulletAlive) {
      GDRAW.fillRect((int)inv.bx-1,(int)inv.by,BULLET_W,BULLET_H,C_BG);
      inv.by-=8;
      if(inv.by<FLAPPY_TOP) {
        inv.bulletAlive=false;
      } else {
        // Check alien hits
        bool hit=false;
        for(int r=0;r<INV_ROWS&&!hit;r++) {
          for(int c=0;c<INV_COLS&&!hit;c++) {
            if(!inv.alive[r][c]) continue;
            int ax=INV_X0+inv.alienOffX+c*(INV_W+INV_XGAP);
            int ay=INV_Y0+inv.alienOffY+r*(INV_H+INV_YGAP);
            if(inv.bx>=ax&&inv.bx<=ax+INV_W&&inv.by>=ay&&inv.by<=ay+INV_H) {
              inv.alive[r][c]=false;
              inv.aliensLeft--;
              invDrawAlien(r,c,C_BG);
              inv.bulletAlive=false;
              inv.score+=10*(INV_ROWS-r);
              invDrawHUD();
              hit=true;
              if(inv.aliensLeft==0) {
                inv.won=true;
                GDRAW.fillRect(140,120,200,80,C_HEADER);
                GDRAW.drawRect(140,120,200,80,C_GREEN);
                GDRAW.setTextColor(C_GREEN,C_HEADER); GDRAW.setTextSize(2);
                GDRAW.setCursor(168,132); GDRAW.print("YOU WIN!");
                GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
                GDRAW.setCursor(155,162); GDRAW.print("Score: "); GDRAW.print(inv.score);
                GDRAW.setCursor(148,178); GDRAW.print("Tap BACK to exit");
                return;
              }
            }
          }
        }
        if(!hit && inv.bulletAlive)
          GDRAW.fillRect((int)inv.bx-1,(int)inv.by,BULLET_W,BULLET_H,C_YELLOW);
      }
    }

    // Alien bullets
    for(int i=0;i<4;i++) {
      if(!inv.abullet[i]) continue;
      GDRAW.fillRect((int)inv.abx[i]-1,(int)inv.aby[i],BULLET_W,BULLET_H,C_BG);
      inv.aby[i]+=5;
      if(inv.aby[i]>PLAYER_Y+PLAYER_H) {
        inv.abullet[i]=false;
      } else {
        // Check player hit
        int px=inv.playerX-PLAYER_W/2;
        if(inv.abx[i]>=px&&inv.abx[i]<=px+PLAYER_W&&inv.aby[i]>=PLAYER_Y) {
          inv.abullet[i]=false;
          inv.lives--;
          invDrawHUD();
          if(inv.lives<=0) {
            inv.gameOver=true;
            GDRAW.fillRect(140,120,200,80,C_HEADER);
            GDRAW.drawRect(140,120,200,80,C_RED);
            GDRAW.setTextColor(C_RED,C_HEADER); GDRAW.setTextSize(2);
            GDRAW.setCursor(160,132); GDRAW.print("GAME OVER");
            GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
            GDRAW.setCursor(155,162); GDRAW.print("Score: "); GDRAW.print(inv.score);
            GDRAW.setCursor(148,178); GDRAW.print("Tap BACK to exit");
            return;
          }
        } else {
          GDRAW.fillRect((int)inv.abx[i]-1,(int)inv.aby[i],BULLET_W,BULLET_H,C_RED);
        }
      }
    }
  }
}

static void invTouch(uint16_t tx, uint16_t ty) {
  if(inv.gameOver||inv.won) return;
  // Erase player
  invDrawPlayer(C_BG);
  if(tx < 160) {
    inv.playerX=max(PLAYER_W/2, inv.playerX-14);
  } else if(tx > 320) {
    inv.playerX=min(480-PLAYER_W/2, inv.playerX+14);
  } else {
    // Center tap = shoot
    if(!inv.bulletAlive) {
      inv.bx=inv.playerX;
      inv.by=PLAYER_Y-BULLET_H;
      inv.bulletAlive=true;
    }
  }
  invDrawPlayer(C_WHITE);
}

// ─────────────────────────────────────────────────────────────────────────────
// GAME 8: MEMORY FLIP
// ─────────────────────────────────────────────────────────────────────────────
#define MEM_COLS 4
#define MEM_ROWS 4
#define MEM_X0   4
#define MEM_Y0   34
#define MEM_CW   114
#define MEM_CH   68
#define MEM_GAP  3

struct MemState {
  uint8_t cards[MEM_ROWS][MEM_COLS];
  bool faceUp[MEM_ROWS][MEM_COLS];
  bool matched[MEM_ROWS][MEM_COLS];
  int firstR, firstC;
  int secondR, secondC;
  bool waitingFlipBack;
  unsigned long flipBackMs;
  int pairsFound;
  bool gameOver, started;
  unsigned long startMs;
};
static MemState mem;

static const char* MEM_SYMBOLS[8]={"A","B","C","D","E","F","G","H"};
static const uint16_t MEM_SYM_COLORS[8]={C_RED,C_GREEN,C_BLUE,C_YELLOW,C_CYAN,C_ORANGE,C_MAGENTA,C_WHITE};

static void memDrawCard(int r, int c) {
  int x=MEM_X0+c*(MEM_CW+MEM_GAP);
  int y=MEM_Y0+r*(MEM_CH+MEM_GAP);
  if(mem.matched[r][c]) {
    GDRAW.fillRect(x,y,MEM_CW,MEM_CH,C_DKGREEN);
    GDRAW.drawRect(x,y,MEM_CW,MEM_CH,C_GREEN);
    return;
  }
  if(mem.faceUp[r][c]) {
    uint8_t val=mem.cards[r][c];
    uint16_t col=MEM_SYM_COLORS[val];
    GDRAW.fillRect(x,y,MEM_CW,MEM_CH,C_HEADER);
    GDRAW.drawRect(x,y,MEM_CW,MEM_CH,col);
    GDRAW.setTextColor(col,C_HEADER); GDRAW.setTextSize(3);
    GDRAW.setCursor(x+MEM_CW/2-9, y+MEM_CH/2-12);
    GDRAW.print(MEM_SYMBOLS[val]);
  } else {
    GDRAW.fillRect(x,y,MEM_CW,MEM_CH,0x1863);
    GDRAW.drawRect(x,y,MEM_CW,MEM_CH,C_GRAY);
    // Pattern on back
    GDRAW.drawRect(x+4,y+4,MEM_CW-8,MEM_CH-8,C_LGRAY);
  }
}

static void memDrawAll() {
  for(int r=0;r<MEM_ROWS;r++) for(int c=0;c<MEM_COLS;c++) memDrawCard(r,c);
}

static void memDrawHUD() {
  GDRAW.fillRect(65,2,350,28,C_BG);
  GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(80,10);
  GDRAW.print("MEMORY  Pairs: "); GDRAW.print(mem.pairsFound); GDRAW.print("/8");
}

static void memInit() {
  memset(&mem,0,sizeof(mem));
  // Fill pairs
  uint8_t vals[16];
  for(int i=0;i<8;i++) { vals[i*2]=i; vals[i*2+1]=i; }
  // Shuffle Fisher-Yates
  for(int i=15;i>0;i--) {
    int j=random(0,i+1);
    uint8_t tmp=vals[i]; vals[i]=vals[j]; vals[j]=tmp;
  }
  int idx=0;
  for(int r=0;r<MEM_ROWS;r++) for(int c=0;c<MEM_COLS;c++)
    mem.cards[r][c]=vals[idx++];
  mem.firstR=-1; mem.firstC=-1;
  mem.secondR=-1; mem.secondC=-1;
  mem.waitingFlipBack=false;
  mem.pairsFound=0;
  mem.gameOver=false; mem.started=true;
  mem.startMs=millis();
  GDRAW.fillScreen(C_BG);
  drawGameBackBtn();
  memDrawHUD();
  memDrawAll();
}

static void memTick() {
  if(!mem.started||mem.gameOver) return;
  if(mem.waitingFlipBack && millis()-mem.flipBackMs>=1000) {
    mem.faceUp[mem.firstR][mem.firstC]=false;
    mem.faceUp[mem.secondR][mem.secondC]=false;
    memDrawCard(mem.firstR,mem.firstC);
    memDrawCard(mem.secondR,mem.secondC);
    mem.firstR=-1; mem.firstC=-1;
    mem.secondR=-1; mem.secondC=-1;
    mem.waitingFlipBack=false;
  }
}

static void memTouch(uint16_t tx, uint16_t ty) {
  if(mem.gameOver||mem.waitingFlipBack) return;
  // Map touch to card
  if(ty<MEM_Y0||ty>=MEM_Y0+MEM_ROWS*(MEM_CH+MEM_GAP)) return;
  if(tx<MEM_X0||tx>=MEM_X0+MEM_COLS*(MEM_CW+MEM_GAP)) return;
  int c=(tx-MEM_X0)/(MEM_CW+MEM_GAP);
  int r=(ty-MEM_Y0)/(MEM_CH+MEM_GAP);
  if(r<0||r>=MEM_ROWS||c<0||c>=MEM_COLS) return;
  if(mem.faceUp[r][c]||mem.matched[r][c]) return;
  if(mem.firstR>=0&&r==mem.firstR&&c==mem.firstC) return;

  mem.faceUp[r][c]=true;
  memDrawCard(r,c);

  if(mem.firstR<0) {
    mem.firstR=r; mem.firstC=c;
  } else {
    mem.secondR=r; mem.secondC=c;
    if(mem.cards[mem.firstR][mem.firstC]==mem.cards[mem.secondR][mem.secondC]) {
      // Match!
      mem.matched[mem.firstR][mem.firstC]=true;
      mem.matched[mem.secondR][mem.secondC]=true;
      memDrawCard(mem.firstR,mem.firstC);
      memDrawCard(mem.secondR,mem.secondC);
      mem.firstR=-1; mem.firstC=-1;
      mem.secondR=-1; mem.secondC=-1;
      mem.pairsFound++;
      memDrawHUD();
      if(mem.pairsFound>=8) {
        mem.gameOver=true;
        unsigned long elapsed=(millis()-mem.startMs)/1000;
        GDRAW.fillRect(110,120,260,80,C_HEADER);
        GDRAW.drawRect(110,120,260,80,C_GREEN);
        GDRAW.setTextColor(C_GREEN,C_HEADER); GDRAW.setTextSize(2);
        GDRAW.setCursor(148,132); GDRAW.print("COMPLETE!");
        GDRAW.setTextColor(C_WHITE,C_HEADER); GDRAW.setTextSize(1);
        GDRAW.setCursor(130,162); GDRAW.print("Time: "); GDRAW.print(elapsed); GDRAW.print("s");
        GDRAW.setCursor(128,178); GDRAW.print("Tap BACK to exit");
      }
    } else {
      // No match - flip back after delay
      mem.waitingFlipBack=true;
      mem.flipBackMs=millis();
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// GAME 9: REACTION TEST
// ─────────────────────────────────────────────────────────────────────────────
struct ReactState {
  int round;
  unsigned long promptMs;
  unsigned long targetMs;
  int delay_ms;
  bool waitingForTarget;
  bool targetShowing;
  bool falseTap;
  bool roundDone;
  long reactions[5];
  bool gameOver, started;
};
static ReactState react;

static void reactShowReady() {
  GDRAW.fillRect(0,FLAPPY_TOP,480,FLAPPY_BOTTOM-FLAPPY_TOP,0x2104);
  GDRAW.setTextColor(C_LGRAY,0x2104); GDRAW.setTextSize(3);
  GDRAW.setCursor(140,130); GDRAW.print("GET READY...");
  GDRAW.setTextSize(1);
  GDRAW.setCursor(170,180); GDRAW.print("Round "); GDRAW.print(react.round+1); GDRAW.print(" of 5");
}

static void reactShowTarget() {
  GDRAW.fillRect(0,FLAPPY_TOP,480,FLAPPY_BOTTOM-FLAPPY_TOP,C_GREEN);
  GDRAW.setTextColor(C_WHITE,C_GREEN); GDRAW.setTextSize(4);
  GDRAW.setCursor(150,130); GDRAW.print("TAP NOW!");
}

static void reactShowFalse() {
  GDRAW.fillRect(0,FLAPPY_TOP,480,FLAPPY_BOTTOM-FLAPPY_TOP,C_RED);
  GDRAW.setTextColor(C_WHITE,C_RED); GDRAW.setTextSize(3);
  GDRAW.setCursor(130,140); GDRAW.print("FALSE START!");
  GDRAW.setTextSize(1);
  GDRAW.setCursor(170,190); GDRAW.print("Wait for GREEN!");
}

static void reactShowResults() {
  GDRAW.fillRect(0,FLAPPY_TOP,480,FLAPPY_BOTTOM-FLAPPY_TOP,C_BG);
  GDRAW.setTextColor(C_CYAN,C_BG); GDRAW.setTextSize(2);
  GDRAW.setCursor(140,40+FLAPPY_TOP); GDRAW.print("RESULTS:");
  long total=0; int valid=0;
  for(int i=0;i<5;i++) {
    GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
    GDRAW.setCursor(100,70+FLAPPY_TOP+i*28);
    GDRAW.print("Round "); GDRAW.print(i+1); GDRAW.print(": ");
    if(react.reactions[i]<0) {
      GDRAW.setTextColor(C_RED,C_BG);
      GDRAW.print("FALSE START");
    } else {
      GDRAW.setTextColor(C_GREEN,C_BG);
      GDRAW.print(react.reactions[i]); GDRAW.print(" ms");
      total+=react.reactions[i]; valid++;
    }
  }
  if(valid>0) {
    GDRAW.setTextColor(C_YELLOW,C_BG); GDRAW.setTextSize(2);
    GDRAW.setCursor(100,70+FLAPPY_TOP+5*28);
    GDRAW.print("Avg: "); GDRAW.print(total/valid); GDRAW.print("ms");
  }
  GDRAW.setTextColor(C_GRAY,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(148,FLAPPY_BOTTOM-20); GDRAW.print("Tap BACK to exit");
}

static void reactDrawHUD() {
  GDRAW.fillRect(65,2,350,28,C_BG);
  GDRAW.setTextColor(C_WHITE,C_BG); GDRAW.setTextSize(1);
  GDRAW.setCursor(80,10); GDRAW.print("REACTION TEST  Round: ");
  GDRAW.print(react.round+1); GDRAW.print("/5");
}

static void reactInit() {
  memset(&react,0,sizeof(react));
  for(int i=0;i<5;i++) react.reactions[i]=0;
  react.round=0;
  react.waitingForTarget=true;
  react.targetShowing=false;
  react.falseTap=false;
  react.roundDone=false;
  react.gameOver=false; react.started=true;
  react.delay_ms=1000+random(0,3000);
  react.promptMs=millis();
  GDRAW.fillScreen(C_BG);
  drawGameBackBtn();
  reactDrawHUD();
  reactShowReady();
}

static void reactTick() {
  if(!react.started||react.gameOver) return;
  unsigned long now=millis();

  if(react.waitingForTarget) {
    if(now-react.promptMs>=(unsigned long)react.delay_ms) {
      react.waitingForTarget=false;
      react.targetShowing=true;
      react.targetMs=now;
      reactShowTarget();
    }
  }
}

static void reactTouch(uint16_t tx, uint16_t ty) {
  if(react.gameOver) return;

  if(react.waitingForTarget && !react.falseTap) {
    // False start
    react.reactions[react.round]=-1;
    react.falseTap=true;
    reactShowFalse();
    react.roundDone=true;
    // Advance after showing false start
    react.round++;
    if(react.round>=5) {
      react.gameOver=true;
      delay(800);
      reactShowResults();
      return;
    }
    delay(800);
    react.falseTap=false;
    react.roundDone=false;
    react.waitingForTarget=true;
    react.targetShowing=false;
    react.delay_ms=1000+random(0,3000);
    react.promptMs=millis();
    reactDrawHUD();
    reactShowReady();
    return;
  }

  if(react.targetShowing) {
    long rt=(long)(millis()-react.targetMs);
    react.reactions[react.round]=rt;
    react.targetShowing=false;
    react.roundDone=true;

    // Show result briefly
    GDRAW.fillRect(0,FLAPPY_TOP,480,FLAPPY_BOTTOM-FLAPPY_TOP,C_HEADER);
    GDRAW.setTextColor(C_YELLOW,C_HEADER); GDRAW.setTextSize(3);
    GDRAW.setCursor(150,130); GDRAW.print(rt); GDRAW.print("ms");
    GDRAW.setTextSize(1);
    GDRAW.setTextColor(C_WHITE,C_HEADER);
    if(rt<200)      { GDRAW.setCursor(180,180); GDRAW.print("INCREDIBLE!"); }
    else if(rt<350) { GDRAW.setCursor(200,180); GDRAW.print("GREAT!"); }
    else if(rt<500) { GDRAW.setCursor(200,180); GDRAW.print("GOOD"); }
    else            { GDRAW.setCursor(190,180); GDRAW.print("TOO SLOW"); }

    react.round++;
    if(react.round>=5) {
      react.gameOver=true;
      delay(1200);
      reactShowResults();
      return;
    }
    delay(1000);
    react.roundDone=false;
    react.waitingForTarget=true;
    react.targetShowing=false;
    react.delay_ms=1000+random(0,3000);
    react.promptMs=millis();
    reactDrawHUD();
    reactShowReady();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// LAUNCHER
// ─────────────────────────────────────────────────────────────────────────────
void drawPageGames() {
  if(currentGame >= 0) return;
  GDRAW.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, C_BG);
  int bw = SCREEN_W/2 - 4;              // 236
  int bh = (CONTENT_H - 6) / 5 - 2;    // ~42
  static const uint16_t gameAccent[10] = {
    C_GREEN, C_CYAN, C_RED, C_YELLOW, C_MAGENTA,
    C_ORANGE, C_WHITE, C_TEAL, C_PURPLE, C_BLUE
  };
  for(int i=0;i<10;i++) {
    int col  = i % 2;
    int row  = i / 2;
    int bx   = 2 + col*(bw+4);
    int by   = CONTENT_Y + 3 + row*(bh+2);
    GDRAW.fillRect(bx, by, bw, bh, 0x0841);
    GDRAW.drawRect(bx, by, bw, bh, gameAccent[i]);
    GDRAW.setTextColor(gameAccent[i], 0x0841);
    GDRAW.setTextSize(1);
    char label[24];
    snprintf(label, sizeof(label), "%d.%s", i+1, GAME_NAMES[i]);
    int textW = strlen(label)*6;
    GDRAW.setCursor(bx + (bw-textW)/2, by + bh/2 - 4);
    GDRAW.print(label);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// START GAME DISPATCHER
// ─────────────────────────────────────────────────────────────────────────────
static void startGame(int gameIdx) {
  currentGame = gameIdx;
  // Clear canvas background before starting game (ghost removal)
  if(canvasReady) {
    gameTarget = &gameCanvas;
    gameCanvas.fillScreen(C_BG);
    gameTarget = &tft;
  }
  switch(gameIdx) {
    case 0: snakeInit();    break;
    case 1: pongInit();     break;
    case 2: breakoutInit(); break;
    case 3: ticInit();      break;
    case 4: simonInit();    break;
    case 5: flappyInit();   break;
    case 6: g2048Init();    break;
    case 7: invInit();      break;
    case 8: memInit();      break;
    case 9: reactInit();    break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// GLOBAL TOUCH HANDLER
// ─────────────────────────────────────────────────────────────────────────────
void handleGamesTouch(uint16_t tx, uint16_t ty) {
  // Back button: top-left area
  if(tx < 62 && ty < 28 && currentGame >= 0) {
    currentGame = -1;
    gameTarget  = &tft;   // restore direct drawing
    needRedraw  = true;
    return;
  }
  if(currentGame == -1) {
    int bw = SCREEN_W/2 - 4;
    int bh = (CONTENT_H - 6) / 5 - 2;
    if(ty < CONTENT_Y || ty >= CONTENT_Y + CONTENT_H) return;
    int col = (tx < SCREEN_W/2) ? 0 : 1;
    int row = (ty - CONTENT_Y - 3) / (bh + 2);
    if(row < 0 || row >= 5) return;
    int gameIdx = row*2 + col;
    if(gameIdx < 0 || gameIdx >= 10) return;
    startGame(gameIdx);
  } else {
    switch(currentGame) {
      case 0: snakeTouch(tx,ty);    break;
      case 1: pongTouch(tx,ty);     break;
      case 2: breakoutTouch(tx,ty); break;
      case 3: ticTouch(tx,ty);      break;
      case 4: simonTouch(tx,ty);    break;
      case 5: flappyTouch(tx,ty);   break;
      case 6: g2048Touch(tx,ty);    break;
      case 7: invTouch(tx,ty);      break;
      case 8: memTouch(tx,ty);      break;
      case 9: reactTouch(tx,ty);    break;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// GLOBAL TICK (call from loop())
// ─────────────────────────────────────────────────────────────────────────────
void gameTick() {
  if(currentGame < 0) return;
  // Activate canvas as drawing target before game tick
  if(canvasReady) gameTarget = &gameCanvas;
  switch(currentGame) {
    case 0: snakeTick();    break;
    case 1: pongTick();     break;
    case 2: breakoutTick(); break;
    // case 3: tic-tac-toe is event-driven only
    case 4: simonTick();    break;
    case 5: flappyTick();   break;
    // case 6: 2048 is event-driven only
    case 7: invTick();      break;
    case 8: memTick();      break;
    case 9: reactTick();    break;
  }
  // Push completed frame to display (double-buffer swap)
  if(canvasReady) {
    gameCanvas.pushSprite(0, 0);
    gameTarget = &tft;
  }
}
