#include "raylib.h"
#include "raymath.h"
#include <stdint.h>
#include <stdio.h>

#define TILEMAP_SIZE_X 16
#define TILEMAP_SIZE_Y 12
#define TILE_PIXELS 64
#define OUTSIDE_TILE_HORIZONTAL TILE_FULL
#define OUTSIDE_TILE_VERTICAL TILE_EMPTY

// Number of items in a static (fixed-size) array
#define arrayNumItems(arr) (sizeof(arr) / sizeof((arr)[0]))

enum Tile { TILE_EMPTY = ' ', TILE_ZERO = '\0', TILE_FULL = '#' };

// Tilemap is a grid of tiles (`Tile` enums, stored as unsigned bytes).
// The '+ 1' is there for string null-termination, because
// we're defining the tilemaps with strings.
typedef uint8_t Tilemap[TILEMAP_SIZE_Y][TILEMAP_SIZE_X + 1];

Tile tilemapGetTile(const Tilemap* tilemap, int x, int y) {
    if (x < 0 || x >= TILEMAP_SIZE_X) return OUTSIDE_TILE_HORIZONTAL;
    if (y < 0 || y >= TILEMAP_SIZE_Y) return OUTSIDE_TILE_VERTICAL;
    return (Tile)(*tilemap)[y][x];
}

// Converts a center (vector) from world-space to screen-space.
// In world-space one unit is one tile in size, so coordinate [1, 1] means tile at this coordinate.
// On the other hand, in screen-space, one unit is a pixel. [1, 1] would just mean the pixel
// close to the upper left corner of the widnow.
Vector2 worldToScreen(const Vector2 worldSpacePos) {
    return Vector2Scale(worldSpacePos, TILE_PIXELS);
}

// Display the tilemap on screen.
void drawTilemap(const Tilemap* tilemap) {
    for (int x = 0; x < TILEMAP_SIZE_X; x++) {
        for (int y = 0; y < TILEMAP_SIZE_Y; y++) {
            if (!tilemapIsTileFull(tilemap, x, y)) continue;
            DrawRectangle(x * TILE_PIXELS, y * TILE_PIXELS, TILE_PIXELS, TILE_PIXELS, YELLOW);
        }
    }
}

void drawTilemapDebug(const Tilemap* tilemap) {
    for (int x = 0; x < TILEMAP_SIZE_X; x++) {
        for (int y = 0; y < TILEMAP_SIZE_Y; y++) {
            Tile tile = tilemapGetTile(tilemap, x, y);
            DrawTextEx(GetFontDefault(), TextFormat("[%i,%i]\n%c", x, y, tile),
                Vector2Add(worldToScreen(Vector2{ (float)x, (float)y }), { 1, 1 }),
                10, 1, BLACK);
        }
    }
}


const Tilemap screenTilemaps[] = {
    {
        // Index zero is empty
        // This index is reserved for 'invalid tilemap'
    },
    {
        "0123456789012345",
        "#",
        "#",
        "#",
        "#",
        "#",
        "#",
        "#",
        "#",
        "#",
        "#",
        "#",
    },
    {
        "gggg",
        "#",
        "#",
        "#",
        "#",
        "#",
        "#",
        "#",
        "#",
        "# #        # #",
        "#    ##   ###",
        "################",
    },
};

int getScreenHeightIndex(float height) {
    return floorf(-height / TILEMAP_SIZE_Y);
}

void getTilesOverlappedByBox(int* outStartX, int* outStartY, int* outEndX, int* outEndY, Vector2 center, const Vector2 size) {
    *outStartX = int(floorf(center.x - size.x));
    *outStartY = int(floorf(center.y - size.y));
    *outEndX = int(floorf(center.x + size.x));
    *outEndY = int(floorf(center.y + size.y));
}

bool tilemapIsTileFull(const Tilemap* tilemap, int x, int y) {
    const Tile tile = tilemapGetTile(tilemap, x, y);
    if (tile == TILE_EMPTY || tile == TILE_ZERO) return false;
    return true;
}

// This function takes a box and a tilemap, and tries to make sure the box
// doesn't intersect with the tilemap.
// 
// The method:
// First, we iterate all of the tiles that *could* be colliding with the box.
// Next, we calculate the distance between surfaces on each axis.
// Then we find an axis to 'clip' the position and velocity against.
// 
// Note: the `size` is half-extent: it's the vector from the center of the box to it's corner.
//  It's half the actual width and height of the box.
void resolveBoxCollisionWithTilemap(const Tilemap* tilemap, float tilemapHeight, Vector2* center, Vector2* velocity, const Vector2 size) {
    center->y -= tilemapHeight;

    int startX = 0;
    int startY = 0;
    int endX = 0;
    int endY = 0;
    // Get neighbor tile ranges
    getTilesOverlappedByBox(&startX, &startY, &endX, &endY, *center, size);

    // Iterate over close tiles
    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            // Skip if non-empty
            if (!tilemapIsTileFull(tilemap, x, y)) continue;
            // Center of the tile box
            const Vector2 boxPos = { 0.5f + (float)x, 0.5f + (float)y };
            const Vector2 sizeSum = { size.x + 0.5f, size.y + 0.5 };
            const Vector2 surfDist = {
                fabsf(center->x - boxPos.x) - sizeSum.x,
                fabsf(center->y - boxPos.y) - sizeSum.y,
            };

            // The two boxes aren't colliding, because
            // the distance between the surfaces is larger than
            // zero on one of the axes.
            if (surfDist.x > 0 || surfDist.y > 0) continue;

            // Now check the closer neighboring tiles on each axis.
            // If the tile is empty (and current tile is full), that means
            // there exists an edge between the two tiles.
            // Our box should collide against such an edge.
            // On the other hand, if there is no edge, the box is inside the tiles
            // and collision cannot be resolved.
            const bool isXEmpty = !tilemapIsTileFull(tilemap, x + (center->x > boxPos.x ? 1 : -1), y);
            // Warning: positive Y is down in this setup!
            const bool isYEmpty = !tilemapIsTileFull(tilemap, x, y + (center->y > boxPos.y ? 1 : -1));

            // If both neighbors are empty, there aren't any edges to collide against.
            if (!isXEmpty && !isYEmpty) continue;

            // Clip axis is the axis of an edge which we don't want our box to intersect.
            bool isClipAxisX = isXEmpty;
            // In case there are two edges, just get the axis which has the least amount of penetration.
            if (isXEmpty && isYEmpty) {
                isClipAxisX = surfDist.x > surfDist.y;
            }

            // Note: this code shouldn't be duplicated, but we can't access the
            // raylib's vector as an array :(
            if (isClipAxisX) {
                if (center->x > boxPos.x) {
                    // Clamp the position exactly to the surface
                    center->x = boxPos.x + sizeSum.x;
                    // Clip the velocity
                    velocity->x = fmaxf(velocity->x, 0.0f);
                }
                else {
                    center->x = boxPos.x - sizeSum.x;
                    // Clip the velocity
                    velocity->x = fminf(velocity->x, 0.0f);
                }
            }
            // Exact copy of the branch above, but 'y' instead of 'x'
            else {
                if (center->y > boxPos.y) {
                    center->y = boxPos.y + sizeSum.y;
                    velocity->y = fmaxf(velocity->y, 0.0f);
                }
                else {
                    center->y = boxPos.y - sizeSum.y;
                    velocity->y = fminf(velocity->y, 0.0f);
                }
            }
        } // y
    } // x

    center->y += tilemapHeight;
}

// Checks whether the box is intersecting any tile in the tilemap.
// param `tilemap`: tilemap to check
// param `tilemapHeight`: offset of the tilemap along the Y axis
// param `center`: coordinate of the center of the box
// param `size`: half-extent of the box - half the box sides
bool isBoxCollidingWithTilemap(const Tilemap* tilemap, float tilemapHeight, Vector2 center, const Vector2 size) {
    center.y -= tilemapHeight;

    int startX = 0;
    int startY = 0;
    int endX = 0;
    int endY = 0;
    // Get neighbor tile ranges
    getTilesOverlappedByBox(&startX, &startY, &endX, &endY, center, size);

    // Iterate over close tiles
    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            // Skip if non-empty
            if (!tilemapIsTileFull(tilemap, x, y)) continue;
            // Center of the tile box
            const Vector2 boxPos = { 0.5f + (float)x, 0.5f + (float)y };
            const Vector2 sizeSum = { size.x + 0.5f, size.y + 0.5 };
            const Vector2 surfDist = {
                fabsf(center.x - boxPos.x) - sizeSum.x,
                fabsf(center.y - boxPos.y) - sizeSum.y,
            };

            // The two boxes aren't colliding, because
            // the distance between the surfaces is larger than
            // zero on one of the axes.
            if (surfDist.x > 0 || surfDist.y > 0) continue;
            return true;
        } // y
    } // x
    return false;
}

struct Player {
};

void updatePlayer() {
}

int main() {
    // Initialization
    const int initialScreenWidth = TILEMAP_SIZE_X * TILE_PIXELS;
    const int initialScreenHeight = TILEMAP_SIZE_Y * TILE_PIXELS;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(initialScreenWidth, initialScreenHeight, "raylib [core] example - keyboard input");
    Vector2 playerPosition = { (float)initialScreenWidth / (2 * TILE_PIXELS), (float)initialScreenHeight / (2 * TILE_PIXELS) };
    Vector2 playerVelocity = {};
    const Vector2 playerSize = { 0.1, 0.1 };
    SetTargetFPS(60); // Set our game to run at 60 frames-per-second when possible

    bool isDebugEnabled = false;

    // Main game loop

    // `WindowShouldClose` detects window close
    while (!WindowShouldClose()) {
        const float delta = GetFrameTime();

        int screenIndex = arrayNumItems(screenTilemaps) - getScreenHeightIndex(playerPosition.y) - 2;
        if (screenIndex < 0 || screenIndex > arrayNumItems(screenTilemaps)) screenIndex = 0;

        const Tilemap* tilemap = &screenTilemaps[screenIndex % arrayNumItems(screenTilemaps)];
        const int heightIndex = getScreenHeightIndex(playerPosition.y);
        const float screenOffsetY = -(float)(heightIndex + 1) * TILEMAP_SIZE_Y;

        // Update
        const float playerGravity = 10.0f;
        playerVelocity.y += playerGravity * delta;
        if (IsKeyDown(KEY_RIGHT)) playerVelocity.x += 60.0f * delta;
        if (IsKeyDown(KEY_LEFT)) playerVelocity.x -= 60.0f * delta;
        if (IsKeyDown(KEY_UP)) playerVelocity.y -= 60.0f * delta;
        if (IsKeyDown(KEY_DOWN)) playerVelocity.y += 60.0f * delta;
        playerPosition = Vector2Add(playerPosition, Vector2Scale(playerVelocity, delta));

        if (IsKeyPressed(KEY_I)) isDebugEnabled = !isDebugEnabled;

        resolveBoxCollisionWithTilemap(tilemap, screenOffsetY, &playerPosition, &playerVelocity, playerSize);

        // Draw

        BeginDrawing();
        ClearBackground(BLACK);
        drawTilemap(tilemap);
        if (isDebugEnabled) drawTilemapDebug(tilemap);
        DrawText("move the ball with arrow keys", 10, 10, 20, GRAY);
        // Draw player, but relative to current screen
        DrawCircleV(worldToScreen({ playerPosition.x, playerPosition.y + screenOffsetY }), TILE_PIXELS * playerSize.y, WHITE);

        if (isDebugEnabled) {
            int startX = 0;
            int startY = 0;
            int endX = 0;
            int endY = 0;
            getTilesOverlappedByBox(&startX, &startY, &endX, &endY, playerPosition, playerSize);

            for (int x = startX; x <= endX; x++) {
                for (int y = startY; y <= endY; y++) {
                    DrawRectangle(x * TILE_PIXELS, y * TILE_PIXELS, TILE_PIXELS, TILE_PIXELS, Fade(RED, 0.5));
                }
            }
        }

        if (isDebugEnabled) {
            DrawFPS(1, 1);
            DrawText(TextFormat("playerPosition = [%f, %f]", playerPosition.x, playerPosition.y), 1, 22, 20, GREEN);
            DrawText(TextFormat("screenOffset = %f", screenOffsetY), 1, 44, 20, GREEN);
            DrawText(TextFormat("screenIndex = %i", screenIndex), 1, 66, 20, GREEN);
        }

        EndDrawing();
    }

    // De-Initialization

    CloseWindow(); // Close window and OpenGL context

    return 0;
}