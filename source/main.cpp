#include "raylib.h"
#include <stdint.h>

#define TILEMAP_SIZE_X 64
#define TILEMAP_SIZE_Y 48
#define TILE_PIXELS 16
#define OUTSIDE_TILE_HORIZONTAL TILE_FULL
#define OUTSIDE_TILE_VERTICAL TILE_EMPTY

enum Tile {
    TILE_EMPTY = ' ',
    TILE_ZERO = '\0',
    TILE_FULL = '#'
};

typedef uint8_t Tilemap[TILEMAP_SIZE_X][TILEMAP_SIZE_Y];

bool tileIsEmpty(Tile tile) {
    switch (tile) {
    case TILE_EMPTY:
        return true;
    case TILE_ZERO:
        return true;
    }
    return false;
}

Tile tilemapGetTile(Tilemap* tilemap, int x, int y) {
    if (x < 0 || x >= TILEMAP_SIZE_X) return OUTSIDE_TILE_HORIZONTAL;
    if (y < 0 || y >= TILEMAP_SIZE_Y) return OUTSIDE_TILE_VERTICAL;
    return (Tile)(*tilemap)[y][x];
}

Tilemap levelTilemaps[] = {
    {

    },
};

int main() {
    // Initialization
    Tilemap tilemap = {
        "",
        "ffg   asd"
    };
	
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - keyboard input");
    Vector2 ballPosition = { (float)screenWidth/2, (float)screenHeight/2 };
    SetTargetFPS(60); // Set our game to run at 60 frames-per-second when possible


    // Main game loop
	
	// `WindowShouldClose` detects window close
    while (!WindowShouldClose()) {
        // Update

        if (IsKeyDown(KEY_RIGHT)) ballPosition.x += 2.0f;
        if (IsKeyDown(KEY_LEFT)) ballPosition.x -= 2.0f;
        if (IsKeyDown(KEY_UP)) ballPosition.y -= 2.0f;
        if (IsKeyDown(KEY_DOWN)) ballPosition.y += 2.0f;

        // Draw
		
        BeginDrawing();
		ClearBackground(RED);
        DrawText("move the ball with arrow keys", 10, 10, 20, GRAY);
        DrawCircleV(ballPosition, 50, WHITE);

        for (int x = 0; x < TILEMAP_SIZE_X; x++) {
            for (int y = 0; y < TILEMAP_SIZE_Y; y++) {
                Tile tile = (Tile)tilemap[y][x];
                if (tile != TILE_EMPTY && tile != TILE_ZERO) {
                    DrawRectangle(x * TILE_PIXELS, y * TILE_PIXELS, TILE_PIXELS, TILE_PIXELS, YELLOW);
                }
            }
        }

        EndDrawing();
    }

    // De-Initialization
	
    CloseWindow(); // Close window and OpenGL context

    return 0;
}