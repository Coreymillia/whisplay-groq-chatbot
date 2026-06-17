#pragma once

/*
 * Tetris.h — Tetris game ported from M5Core2 to Waveshare ESP32-S3 410x502
 *
 * Self-contained game logic, rendering, and touch input.
 * Adapted from /home/coreymillia/Documents/M5Core2-ESP32-Tetris-Touch-main
 */

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "GroqWatchLog.h"

// ── Game layout constants (410 x 502 portrait) ───────────────────────────────
static constexpr int TETRIS_BLOCK_SIZE   = 23;
static constexpr int TETRIS_FIELD_W      = 10;
static constexpr int TETRIS_FIELD_H      = 20;
static constexpr int TETRIS_OFFSET_X     = 90;    // field left edge
static constexpr int TETRIS_OFFSET_Y     = 20;    // field top edge
static constexpr int TETRIS_FIELD_PX_W   = TETRIS_FIELD_W * TETRIS_BLOCK_SIZE;
static constexpr int TETRIS_FIELD_PX_H   = TETRIS_FIELD_H * TETRIS_BLOCK_SIZE;

// ── Panel positions ──────────────────────────────────────────────────────────
static constexpr int TETRIS_HOLD_X       = 8;
static constexpr int TETRIS_HOLD_Y       = 66;
static constexpr int TETRIS_HOLD_SIZE    = 68;

static constexpr int TETRIS_NEXT_X       = TETRIS_OFFSET_X + TETRIS_FIELD_PX_W + 10;
static constexpr int TETRIS_NEXT_Y       = 66;
static constexpr int TETRIS_NEXT_SIZE    = 68;

static constexpr int TETRIS_SCORE_Y      = 4;

// ── Colors ───────────────────────────────────────────────────────────────────
static constexpr uint16_t T_COLOR_BLACK   = 0x0000;
static constexpr uint16_t T_COLOR_WHITE   = 0xFFFF;
static constexpr uint16_t T_COLOR_RED     = 0xF800;
static constexpr uint16_t T_COLOR_GREEN   = 0x07E0;
static constexpr uint16_t T_COLOR_BLUE    = 0x001F;
static constexpr uint16_t T_COLOR_YELLOW  = 0xFFE0;
static constexpr uint16_t T_COLOR_ORANGE  = 0xFD20;
static constexpr uint16_t T_COLOR_PURPLE  = 0x780F;
static constexpr uint16_t T_COLOR_CYAN    = 0x07FF;
static constexpr uint16_t T_COLOR_MAGENTA = 0xF81F;

// ── Piece colors ─────────────────────────────────────────────────────────────
static const uint16_t T_PIECE_COLORS[7] = {
    T_COLOR_YELLOW, T_COLOR_CYAN,   T_COLOR_PURPLE, T_COLOR_GREEN,
    T_COLOR_RED,    T_COLOR_BLUE,   T_COLOR_ORANGE
};

// ── Tetromino shapes (7 pieces × 4 rotations × 2 axes × 4 cells) ────────────
//    axes: [0] = row offset, [1] = col offset
static const int T_PIECES[7][4][2][4] = {
    // O
    {{{0,0,1,1},{0,1,0,1}},{{0,0,1,1},{0,1,0,1}},{{0,0,1,1},{0,1,0,1}},{{0,0,1,1},{0,1,0,1}}},
    // I
    {{{0,0,0,0},{-1,0,1,2}},{{-1,0,1,2},{0,0,0,0}},{{0,0,0,0},{-1,0,1,2}},{{-1,0,1,2},{0,0,0,0}}},
    // T (clockwise)
    {{{0,0,0,1},{-1,0,1,0}},{{-1,0,1,0},{0,0,0,-1}},{{0,0,0,-1},{1,0,-1,0}},{{1,0,-1,0},{0,0,0,1}}},
    // S (clockwise)
    {{{0,0,1,1},{0,1,-1,0}},{{-1,0,0,1},{0,0,1,1}},{{0,0,1,1},{0,1,-1,0}},{{-1,0,0,1},{0,0,1,1}}},
    // Z (clockwise)
    {{{0,0,1,1},{-1,0,0,1}},{{-1,0,0,1},{1,1,0,0}},{{0,0,1,1},{-1,0,0,1}},{{-1,0,0,1},{1,1,0,0}}},
    // J (clockwise)
    {{{-1,0,0,0},{-1,-1,0,1}},{{-1,-1,0,1},{0,1,0,0}},{{0,0,0,1},{-1,0,1,1}},{{-1,0,1,1},{0,0,-1,0}}},
    // L (clockwise)
    {{{-1,0,0,0},{1,-1,0,1}},{{-1,0,1,1},{0,0,0,1}},{{0,0,0,1},{-1,0,1,-1}},{{-1,-1,0,1},{0,1,1,1}}}
};

// ── Tetris game state ────────────────────────────────────────────────────────
namespace Tetris {

static Arduino_GFX *tgfx = nullptr;

static uint8_t tField[TETRIS_FIELD_H][TETRIS_FIELD_W];
static int     tCurrentPiece = 0;
static int     tCurrentRot   = 0;
static int     tPosX = 0, tPosY = 0;
static int     tScore       = 0;
static int     tLevel       = 1;
static int     tLines       = 0;
static int     tHeldPiece   = -1;
static int     tNextPiece   = 0;
static bool    tCanHold     = true;
static bool    tGameOver    = false;
static bool    tNeedsInit   = true;
static bool    tDirty       = true;

static unsigned long tLastDropMs    = 0;
static unsigned long tLockDelayStart = 0;
static bool          tLockDelayActive = false;
static int           tDropSpeed     = 500;

static int  tLastScore  = -1;
static int  tLastLines  = -1;

// ── Touch tracking for Tetris mode ──────────────────────────────────────────
static bool    tTouchActive    = false;
static bool    tTouchHardDrop  = false;
static int     tTouchStartX    = -1;
static int     tTouchStartY    = -1;
static unsigned long tTouchStartMs = 0;

static bool    tBtnLeft         = false;
static bool    tBtnRight        = false;
static bool    tBtnRotate       = false;
static bool    tBtnSoftDrop     = false;
static bool    tBtnHardDrop     = false;
static bool    tBtnHold         = false;
static bool    tBtnLeftPrev     = false;
static bool    tBtnRightPrev    = false;
static bool    tBtnRotatePrev   = false;
static bool    tBtnHardDropPrev = false;
static bool    tBtnHoldPrev     = false;

static unsigned long tLastLeftRightMs = 0;
static unsigned long tLastMoveMs      = 0;

// ── Forward declarations ─────────────────────────────────────────────────────
static void tInit();
static void tDraw();
static void tUpdate();
static bool tTest(int y, int x, int piece, int rot);
static void tPlacePiece();
static void tClearLines();
static void tNewPiece(bool setPiece);
static int  tCalculateDropDistance();
static void tDrawGhost();
static uint16_t tShadeColor(uint16_t color, uint8_t percent);
static void tDrawMini(int pieceType, int x, int y, int scale);
static void tDrawBoardBlock(int px, int py, uint16_t color);
static void tDrawMiniBlock(int px, int py, int scale, uint16_t color);
static void tDrawHold();
static void tDrawNext();
static void tHandleInput();
static void tDrawBorder();
static void tShowGameOver();

// ═══════════════════════════════════════════════════════════════════════════════
//  Entry points (called from main.cpp)
// ═══════════════════════════════════════════════════════════════════════════════

inline void tetrisBegin(Arduino_GFX &gfx) {
    tgfx = &gfx;
    tNeedsInit = true;
    tDirty = true;

    tTouchActive = false;
    tTouchHardDrop = false;
    tTouchStartX = -1;
    tTouchStartY = -1;
    tTouchStartMs = 0;

    tBtnLeft = false;
    tBtnRight = false;
    tBtnRotate = false;
    tBtnSoftDrop = false;
    tBtnHardDrop = false;
    tBtnHold = false;
    tBtnLeftPrev = false;
    tBtnRightPrev = false;
    tBtnRotatePrev = false;
    tBtnHardDropPrev = false;
    tBtnHoldPrev = false;
    tLastLeftRightMs = 0;
    tLastMoveMs = 0;

    tgfx->fillScreen(T_COLOR_BLACK);
    tDrawBorder();
}

inline void tetrisEnd() {
    tgfx = nullptr;
}

inline bool tetrisIsGameOver() { return tGameOver && !tNeedsInit; }

inline int tetrisGetScore() { return tScore; }

// Process a raw touch event for Tetris controls.
// (x, y) are display coordinates; pressed = finger down, released = finger up.
inline void tetrisHandleTouch(uint16_t x, uint16_t y, bool pressed, bool released) {
    if (!tgfx) return;

    // Continuous movement flags must be recomputed from live touch state.
    tBtnLeft = false;
    tBtnRight = false;
    tBtnSoftDrop = false;

    const int fLeft  = TETRIS_OFFSET_X;
    const int fRight = TETRIS_OFFSET_X + TETRIS_FIELD_PX_W;
    const int fTop   = TETRIS_OFFSET_Y;
    const int fBot   = TETRIS_OFFSET_Y + TETRIS_FIELD_PX_H;

    if (pressed) {
        if (!tTouchActive) {
            tTouchActive   = true;
            tTouchHardDrop = false;
            tTouchStartX   = x;
            tTouchStartY   = y;
            tTouchStartMs  = millis();
            return;
        }

        const bool startInHoldZone =
            tTouchStartX >= TETRIS_HOLD_X && tTouchStartX <= TETRIS_HOLD_X + TETRIS_HOLD_SIZE &&
            tTouchStartY >= TETRIS_HOLD_Y && tTouchStartY <= TETRIS_HOLD_Y + TETRIS_HOLD_SIZE + 20;
        if (startInHoldZone) return;

        if (tTouchStartX < fLeft) {
            tBtnLeft = true;
        } else if (tTouchStartX > fRight) {
            tBtnRight = true;
        } else if (tTouchStartX >= fLeft && tTouchStartX <= fRight &&
                   tTouchStartY >= fTop && tTouchStartY <= fBot &&
                   millis() - tTouchStartMs > 220) {
            // Hold inside field = soft drop.
            tBtnSoftDrop = true;
        }
        return;
    }

    if (released && tTouchActive) {
        tTouchActive = false;
        unsigned long dur = millis() - tTouchStartMs;
        int dx = (int)x - tTouchStartX;
        int dy = (int)y - tTouchStartY;

        // Swipe up = hard drop
        if (dy < -40 && dur < 350 && !tTouchHardDrop) {
            tTouchHardDrop = true;
            tBtnHardDrop = true;
            return;
        }

        // Quick tap inside game field = rotate
        if (tTouchStartX >= fLeft && tTouchStartX <= fRight &&
            tTouchStartY >= fTop  && tTouchStartY <= fBot &&
            dur < 200 && abs(dx) < 18 && abs(dy) < 18) {
            tBtnRotate = true;
            return;
        }

        // Hold piece zone
        if (tTouchStartX >= TETRIS_HOLD_X && tTouchStartX <= TETRIS_HOLD_X + TETRIS_HOLD_SIZE &&
            tTouchStartY >= TETRIS_HOLD_Y && tTouchStartY <= TETRIS_HOLD_Y + TETRIS_HOLD_SIZE + 20) {
            tBtnHold = true;
            return;
        }
    }
}

// Process BOOT button for Tetris (rotate on press)
inline void tetrisHandleBoot() {
    tBtnRotate = true;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Game logic
// ═══════════════════════════════════════════════════════════════════════════════

static void tInit() {
    for (int y = 0; y < TETRIS_FIELD_H; y++)
        for (int x = 0; x < TETRIS_FIELD_W; x++)
            tField[y][x] = 0;

    tScore       = 0;
    tLevel       = 1;
    tLines       = 0;
    tDropSpeed   = 500;
    tGameOver    = false;
    tHeldPiece   = -1;
    tCanHold     = true;
    tLastScore   = -1;
    tLastLines   = -1;
    tLastDropMs  = 0;
    tLockDelayActive = false;
    tNextPiece   = random(0, 7);

    tNewPiece(false);
    tDirty = true;
    tNeedsInit = false;
}

static bool tTest(int y, int x, int piece, int rot) {
    for (int i = 0; i < 4; i++) {
        int px = x + T_PIECES[piece][rot][1][i];
        int py = y + T_PIECES[piece][rot][0][i];
        if (px < 0 || px >= TETRIS_FIELD_W || py >= TETRIS_FIELD_H) return true;
        if (py >= 0 && tField[py][px] > 0) return true;
    }
    return false;
}

static void tPlacePiece() {
    for (int i = 0; i < 4; i++) {
        int x = tPosX + T_PIECES[tCurrentPiece][tCurrentRot][1][i];
        int y = tPosY + T_PIECES[tCurrentPiece][tCurrentRot][0][i];
        if (y >= 0 && y < TETRIS_FIELD_H && x >= 0 && x < TETRIS_FIELD_W)
            tField[y][x] = tCurrentPiece + 1;
    }
}

static void tClearLines() {
    int cleared = 0;
    for (int y = TETRIS_FIELD_H - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < TETRIS_FIELD_W; x++) {
            if (tField[y][x] == 0) { full = false; break; }
        }
        if (full) {
            cleared++;
            tScore += 100;
            for (int yy = y; yy > 0; yy--)
                for (int x = 0; x < TETRIS_FIELD_W; x++)
                    tField[yy][x] = tField[yy - 1][x];
            for (int x = 0; x < TETRIS_FIELD_W; x++)
                tField[0][x] = 0;
            y++;
        }
    }
    if (cleared > 0) {
        tLines += cleared;
        int newLevel = 1 + (tLines / 10);
        if (newLevel > tLevel) {
            tLevel = newLevel;
            tDropSpeed = max(60, 500 - (tLevel - 1) * 40);
        }
    }
}

static void tNewPiece(bool setPiece) {
    if (setPiece) tCanHold = true;
    if (tNextPiece >= 0) {
        tCurrentPiece = tNextPiece;
        tNextPiece = random(0, 7);
    } else {
        tCurrentPiece = random(0, 7);
        tNextPiece = random(0, 7);
    }
    tCurrentRot = 0;
    tPosX = TETRIS_FIELD_W / 2 - 1;
    tPosY = 0;
    tLockDelayActive = false;
}

static int tCalculateDropDistance() {
    for (int d = 1; d < TETRIS_FIELD_H; d++)
        if (tTest(tPosY + d, tPosX, tCurrentPiece, tCurrentRot))
            return d - 1;
    return TETRIS_FIELD_H - tPosY - 1;
}

static void tHoldPiece() {
    if (!tCanHold) return;
    tCanHold = false;
    if (tHeldPiece < 0) {
        tHeldPiece = tCurrentPiece;
        tNewPiece(false);
    } else {
        int temp = tCurrentPiece;
        tCurrentPiece = tHeldPiece;
        tHeldPiece = temp;
        tCurrentRot = 0;
        tPosX = TETRIS_FIELD_W / 2 - 1;
        tPosY = 0;
    }
    tLockDelayActive = false;
}

static void tHandleInput() {
    const unsigned long now = millis();

    // Continuous left / right movement from hold input.
    if (now - tLastLeftRightMs > 80) {
        bool moved = false;
        if (tBtnLeft) {
            tPosX--;
            if (tTest(tPosY, tPosX, tCurrentPiece, tCurrentRot)) tPosX++;
            else { if (tLockDelayActive) tLockDelayStart = now; moved = true; }
        } else if (tBtnRight) {
            tPosX++;
            if (tTest(tPosY, tPosX, tCurrentPiece, tCurrentRot)) tPosX--;
            else { if (tLockDelayActive) tLockDelayStart = now; moved = true; }
        }
        if (moved) {
            tLastLeftRightMs = now;
            tDirty = true;
        }
    }

    // Swipe up = hard drop.
    if (tBtnHardDrop) {
        while (!tTest(tPosY + 1, tPosX, tCurrentPiece, tCurrentRot)) {
            tPosY++;
            tScore += 2;
        }
        tLockDelayActive = false;
        tLastDropMs = 0;
        tDirty = true;
        tBtnHardDrop = false;
        return;
    }

    // Hold in field = soft drop.
    if (tBtnSoftDrop && now - tLastMoveMs >= 75) {
        tPosY++;
        if (tTest(tPosY, tPosX, tCurrentPiece, tCurrentRot)) {
            tPosY--;
        } else {
            tScore++;
            tLockDelayActive = false;
            tDirty = true;
        }
        tLastMoveMs = now;
        return;
    }

    if (now - tLastMoveMs < 100) {
        tBtnRotate = false;
        tBtnHold = false;
        return;
    }

    // Tap = rotate.
    if (tBtnRotate) {
        int nr = (tCurrentRot + 1) % 4;
        if (!tTest(tPosY, tPosX, tCurrentPiece, nr)) {
            tCurrentRot = nr;
            if (tLockDelayActive) tLockDelayStart = now;
            tDirty = true;
        }
        tLastMoveMs = now;
    }
    // Hold piece button.
    else if (tBtnHold) {
        tHoldPiece();
        tDirty = true;
        tLastMoveMs = now;
    }

    tBtnRotate = false;
    tBtnHold = false;
}

static void tUpdate() {
    if (tNeedsInit) tInit();
    if (tGameOver) return;

    tHandleInput();

    const unsigned long now = millis();
    if (now - tLastDropMs > (unsigned long)tDropSpeed) {
        tPosY++;
        if (tTest(tPosY, tPosX, tCurrentPiece, tCurrentRot)) {
            tPosY--;
            if (!tLockDelayActive) {
                tLockDelayActive = true;
                tLockDelayStart = now;
            }
            if (now - tLockDelayStart >= 500) {
                tPlacePiece();
                tClearLines();
                tNewPiece(true);
                if (tTest(tPosY, tPosX, tCurrentPiece, tCurrentRot))
                    tGameOver = true;
            }
        } else {
            tLockDelayActive = false;
        }
        tLastDropMs = now;
        tDirty = true;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Rendering
// ═══════════════════════════════════════════════════════════════════════════════

static void tDrawBorder() {
    const int x0 = TETRIS_OFFSET_X - 3;
    const int y0 = TETRIS_OFFSET_Y - 3;
    const int w  = TETRIS_FIELD_PX_W + 6;
    const int h  = TETRIS_FIELD_PX_H + 6;

    tgfx->fillRect(x0, y0, w, 3, T_COLOR_BLUE);
    tgfx->fillRect(x0, y0 + h - 3, w, 3, T_COLOR_BLUE);
    tgfx->fillRect(x0, y0, 3, h, T_COLOR_BLUE);
    tgfx->fillRect(x0 + w - 3, y0, 3, h, T_COLOR_BLUE);

    tgfx->fillRect(x0 + 1, y0 + 1, w - 2, 1, T_COLOR_CYAN);
    tgfx->fillRect(x0 + 1, y0 + h - 2, w - 2, 1, T_COLOR_CYAN);
    tgfx->fillRect(x0 + 1, y0 + 1, 1, h - 2, T_COLOR_CYAN);
    tgfx->fillRect(x0 + w - 2, y0 + 1, 1, h - 2, T_COLOR_CYAN);
}

static uint16_t tShadeColor(uint16_t color, uint8_t percent) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    r = (uint8_t)((r * percent) / 100);
    g = (uint8_t)((g * percent) / 100);
    b = (uint8_t)((b * percent) / 100);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void tDrawBoardBlock(int px, int py, uint16_t color) {
    const int s = TETRIS_BLOCK_SIZE - 1;
    tgfx->fillRect(px, py, s, s, color);
    if (s > 10) {
        const uint16_t inner = tShadeColor(color, 58);
        tgfx->fillRect(px + 4, py + 4, s - 8, s - 8, inner);
    } else if (s > 6) {
        const uint16_t inner = tShadeColor(color, 58);
        tgfx->fillRect(px + 2, py + 2, s - 4, s - 4, inner);
    }
}

static void tDrawMiniBlock(int px, int py, int scale, uint16_t color) {
    const int s = scale - 1;
    tgfx->fillRect(px, py, s, s, color);
    if (s > 6) {
        const uint16_t inner = tShadeColor(color, 58);
        tgfx->fillRect(px + 2, py + 2, s - 4, s - 4, inner);
    }
}

static void tDrawMini(int pieceType, int x, int y, int scale) {
    if (pieceType < 0 || pieceType > 6) return;
    for (int i = 0; i < 4; i++) {
        int px = x + (T_PIECES[pieceType][0][1][i] * scale);
        int py = y + (T_PIECES[pieceType][0][0][i] * scale);
        tDrawMiniBlock(px, py, scale, T_PIECE_COLORS[pieceType]);
    }
}

static void tDrawGhost() {
    int drop = tCalculateDropDistance();
    if (drop <= 0) return;
    int gy = tPosY + drop;
    const uint16_t outer = tgfx->color565(140, 235, 255);
    const uint16_t fill  = tgfx->color565(28, 56, 76);
    for (int i = 0; i < 4; i++) {
        int x = tPosX + T_PIECES[tCurrentPiece][tCurrentRot][1][i];
        int y = gy    + T_PIECES[tCurrentPiece][tCurrentRot][0][i];
        if (y >= 0 && y < TETRIS_FIELD_H && x >= 0 && x < TETRIS_FIELD_W) {
            int px = TETRIS_OFFSET_X + x * TETRIS_BLOCK_SIZE;
            int py = TETRIS_OFFSET_Y + y * TETRIS_BLOCK_SIZE;
            const int s = TETRIS_BLOCK_SIZE - 1;
            if (s > 8) {
                tgfx->fillRect(px + 4, py + 4, s - 8, s - 8, fill);
                tgfx->drawRect(px + 1, py + 1, s - 2, s - 2, outer);
            } else {
                tgfx->drawRect(px, py, s, s, outer);
            }
        }
    }
}

static void tDrawHold() {
    tgfx->setTextSize(1);
    tgfx->setTextColor(T_COLOR_WHITE, T_COLOR_BLACK);
    tgfx->fillRect(TETRIS_HOLD_X, TETRIS_HOLD_Y - 12, TETRIS_HOLD_SIZE, 12, T_COLOR_BLACK);
    tgfx->setCursor(TETRIS_HOLD_X + 12, TETRIS_HOLD_Y - 10);
    tgfx->print("HOLD");

    tgfx->fillRect(TETRIS_HOLD_X, TETRIS_HOLD_Y, TETRIS_HOLD_SIZE, TETRIS_HOLD_SIZE, T_COLOR_BLACK);
    tgfx->drawRect(TETRIS_HOLD_X - 1, TETRIS_HOLD_Y - 1,
                   TETRIS_HOLD_SIZE + 2, TETRIS_HOLD_SIZE + 2, T_COLOR_WHITE);

    if (tHeldPiece >= 0)
        tDrawMini(tHeldPiece, TETRIS_HOLD_X + 14, TETRIS_HOLD_Y + 18, 8);

    tgfx->fillRect(TETRIS_HOLD_X, TETRIS_HOLD_Y + TETRIS_HOLD_SIZE + 8, 76, 16, T_COLOR_BLACK);
    tgfx->setTextColor(T_COLOR_CYAN, T_COLOR_BLACK);
    tgfx->setCursor(TETRIS_HOLD_X, TETRIS_HOLD_Y + TETRIS_HOLD_SIZE + 10);
    tgfx->printf("LINES %d", tLines);
    tLastLines = tLines;
}

static void tDrawNext() {
    tgfx->setTextSize(1);
    tgfx->setTextColor(T_COLOR_WHITE, T_COLOR_BLACK);
    tgfx->fillRect(TETRIS_NEXT_X, TETRIS_NEXT_Y - 12, TETRIS_NEXT_SIZE, 12, T_COLOR_BLACK);
    tgfx->setCursor(TETRIS_NEXT_X + 12, TETRIS_NEXT_Y - 10);
    tgfx->print("NEXT");

    tgfx->fillRect(TETRIS_NEXT_X, TETRIS_NEXT_Y, TETRIS_NEXT_SIZE, TETRIS_NEXT_SIZE, T_COLOR_BLACK);
    tgfx->drawRect(TETRIS_NEXT_X - 1, TETRIS_NEXT_Y - 1,
                   TETRIS_NEXT_SIZE + 2, TETRIS_NEXT_SIZE + 2, T_COLOR_WHITE);

    if (tNextPiece >= 0)
        tDrawMini(tNextPiece, TETRIS_NEXT_X + 14, TETRIS_NEXT_Y + 18, 8);
}

static void tShowGameOver() {
    const int ox = 40, oy = 170, ow = 330, oh = 140;
    tgfx->fillRoundRect(ox, oy, ow, oh, 12, tgfx->color565(10, 10, 30));
    tgfx->drawRoundRect(ox, oy, ow, oh, 12, T_COLOR_RED);

    tgfx->setTextSize(3);
    tgfx->setTextColor(T_COLOR_RED);
    tgfx->setCursor(ox + 52, oy + 24);
    tgfx->print("GAME OVER");

    tgfx->setTextSize(2);
    tgfx->setTextColor(T_COLOR_WHITE);
    tgfx->setCursor(ox + 74, oy + 72);
    tgfx->printf("Score: %d", tScore);

    tgfx->setTextSize(1);
    tgfx->setTextColor(T_COLOR_CYAN);
    tgfx->setCursor(ox + 56, oy + 112);
    tgfx->print("Tap to restart  |  BOOT = back");
}

static void tDraw() {
    if (!tgfx || !tDirty) return;
    tDirty = false;

    if (tNeedsInit) {
        tgfx->fillScreen(T_COLOR_BLACK);
        tDrawBorder();
        return;
    }

    // ── Score bar at top ──────────────────────────────────────────────────
    if (tScore != tLastScore || tNeedsInit) {
        tgfx->fillRect(0, 0, 410, 16, T_COLOR_BLACK);
        tgfx->setTextSize(1);
        tgfx->setTextColor(T_COLOR_WHITE, T_COLOR_BLACK);
        tgfx->setCursor(144, TETRIS_SCORE_Y);
        tgfx->printf("SCORE %d", tScore);
        tgfx->setCursor(258, TETRIS_SCORE_Y);
        tgfx->printf("LV %d", tLevel);
        tLastScore = tScore;
    }

    // ── Field blocks ──────────────────────────────────────────────────────
    for (int y = 0; y < TETRIS_FIELD_H; y++) {
        for (int x = 0; x < TETRIS_FIELD_W; x++) {
            int px = TETRIS_OFFSET_X + x * TETRIS_BLOCK_SIZE;
            int py = TETRIS_OFFSET_Y + y * TETRIS_BLOCK_SIZE;
            if (tField[y][x] > 0) {
                tDrawBoardBlock(px, py, T_PIECE_COLORS[tField[y][x] - 1]);
            } else {
                tgfx->fillRect(px, py, TETRIS_BLOCK_SIZE - 1, TETRIS_BLOCK_SIZE - 1, T_COLOR_BLACK);
            }
        }
    }

    // Keep border solid and on top of any clears.
    tDrawBorder();

    // ── Ghost piece ───────────────────────────────────────────────────────
    tDrawGhost();

    // ── Current piece ─────────────────────────────────────────────────────
    for (int i = 0; i < 4; i++) {
        int x = tPosX + T_PIECES[tCurrentPiece][tCurrentRot][1][i];
        int y = tPosY + T_PIECES[tCurrentPiece][tCurrentRot][0][i];
        if (y >= 0 && y < TETRIS_FIELD_H && x >= 0 && x < TETRIS_FIELD_W) {
            int px = TETRIS_OFFSET_X + x * TETRIS_BLOCK_SIZE;
            int py = TETRIS_OFFSET_Y + y * TETRIS_BLOCK_SIZE;
            tDrawBoardBlock(px, py, T_PIECE_COLORS[tCurrentPiece]);
        }
    }

    // ── UI panels ─────────────────────────────────────────────────────────
    tDrawHold();
    tDrawNext();

    // ── Game over overlay ─────────────────────────────────────────────────
    if (tGameOver) tShowGameOver();
}

}  // namespace Tetris
