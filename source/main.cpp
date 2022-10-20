#include "raylib.h" // Base Raylib header
#include "raymath.h" // Vector math
#include <stdint.h>
#include <stdio.h> // printf
#include <assert.h> // for `assert`

#define TILEMAP_SIZE_X 16
#define TILEMAP_SIZE_Y 12
// How wide and tall is each tile in pixels
#define TILE_PIXELS 64
// What happens when we get out of grid horizontally
#define OUTSIDE_TILE_HORIZONTAL TILE_FULL
// What happens when we get out of grid vertically
#define OUTSIDE_TILE_VERTICAL TILE_EMPTY
// How much should the box in `resolveBoxCollisionWithTilemap` bounce of off walls.
// Mainly player uses this to bounce.
#define BOUNCE_FACTOR_X 0.4f

// Number of items in a static (fixed-size) array
#define arrayNumItems(arr) (sizeof(arr) / sizeof((arr)[0]))

// Half-size of the player's box collider.
#define PLAYER_SIZE Vector2{0.3f, 0.4f}
// Gravity in units (tiles) per second
#define PLAYER_GRAVITY 32.0f
// How fast player accelerates.
#define PLAYER_SPEED 75.0f
#define PLAYER_GROUND_FRICTION_X 70.0f
#define PLAYER_JUMP_STRENGTH 18.0f

struct Player {
    Vector2 position;
    Vector2 velocity;
    float jumpHoldTime;
};

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

bool tilemapIsTileFull(const Tilemap* tilemap, int x, int y) {
    const Tile tile = tilemapGetTile(tilemap, x, y);
    if (tile == TILE_EMPTY || tile == TILE_ZERO) return false;
    return true;
}

// Display the tilemap on screen.
void drawTilemap(const Tilemap* tilemap) {
    for (int x = 0; x < TILEMAP_SIZE_X; x++) {
        for (int y = 0; y < TILEMAP_SIZE_Y; y++) {
            if (!tilemapIsTileFull(tilemap, x, y)) continue;
            DrawRectangle(x * TILE_PIXELS, y * TILE_PIXELS, TILE_PIXELS, TILE_PIXELS, ORANGE);
        }
    }
}

void drawTilemapDebug(const Tilemap* tilemap) {
    for (int x = 0; x < TILEMAP_SIZE_X; x++) {
        for (int y = 0; y < TILEMAP_SIZE_Y; y++) {
            Tile tile = tilemapGetTile(tilemap, x, y);
            DrawTextEx(GetFontDefault(), TextFormat("[%i,%i]\n%i\n\'%c\'", x, y, tile, tile),
                Vector2Add(worldToScreen(Vector2{ (float)x, (float)y }), { 1, 1 }),
                10, 1, RED);
        }
    }
}


// Note: starts at the bottom, so it looks continuous
const Tilemap screenTilemaps[] = {
    {
        // Index zero is empty
        // This index is reserved for 'invalid tilemap'
    },
    {
        "#  ########    #",
        "# ##      ##   #",
        "# # #####  ### #",
        "# # #   #  # # #",
        "# # #####  # # #",
        "# #        # # #",
        "# #        # # #",
        "# #        ### #",
        "# #  ####  #   #",
        "# #  #  #  #   #",
        "# ####  ####   #",
        "####      ######",
    },
    {
        "####      ######",
        "###        #####",
        "###  ###    ####",
        "###  ##     ####",
        "###       ######",
        "###    #########",
        "###  #    ######",
        "###        #####",
        "###         ####",
        "##  ##      ####",
        "##        ######",
        "##        ######",
    },
    // Starting screen:
    {
        "##        ######",
        "######       ###",
        "######        ##",
        "######     #####",
        "##       #######",
        "#              #",
        "#  #######     #",
        "#  #######     #",
        "#     ##       #",
        "###        # # #",
        "#### ##   ###  #",
        "################",
    },
};

// Get the screen index, where start = 0 and increases when you move up (-Y)
int getScreenHeightIndex(float height) {
    return floorf(-height / TILEMAP_SIZE_Y);
}

// Get start and end coordinates of the boxes a bounding box on the tilemap grid
void getTilesOverlappedByBox(int* outStartX, int* outStartY, int* outEndX, int* outEndY, Vector2 center, const Vector2 size) {
    *outStartX = int(floorf(center.x - size.x));
    *outStartY = int(floorf(center.y - size.y));
    *outEndX = int(floorf(center.x + size.x));
    *outEndY = int(floorf(center.y + size.y));
}


// This function takes a box and a tilemap, and tries to make sure the box
// doesn't intersect with the tilemap.
// 
// The method:
// First, we iterate all of the tiles that *could* be colliding with the box (based on the bounding volume).
// Next, we calculate the distance between near surfaces on each axis.
// Then we find an axis to 'clip' the position and velocity against.
// 
// Note: the `size` is half-extent: it's the vector from the center of the box to it's corner.
//  It's half the actual width and height of the box.
void resolveBoxCollisionWithTilemap(const Tilemap* tilemap, float tilemapHeight, Vector2* center, Vector2* velocity, const Vector2 size) {
    // Add the offset to center (simply transform into tilemap local-space)
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

            // Clip the velocity (or bounce) based on the axis
            if (isClipAxisX) {
                if (center->x > boxPos.x) {
                    // Clamp the position exactly to the surface
                    center->x = boxPos.x + sizeSum.x;
                    if (velocity->x < 0.0) {
                        velocity->x = -velocity->x * BOUNCE_FACTOR_X;
                    }
                }
                else {
                    center->x = boxPos.x - sizeSum.x;
                    if (velocity->x > 0.0) {
                        velocity->x = -velocity->x * BOUNCE_FACTOR_X;
                    }
                }
            }
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

    // Remove the local-space offset
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

// Read inputs and update player movement
void updatePlayer(Player* player, const Tilemap* tilemap, float tilemapHeight, float delta) {
    player->velocity.y += PLAYER_GRAVITY * delta;
    const bool isOnGround = isBoxCollidingWithTilemap(tilemap, tilemapHeight, { player->position.x, player->position.y + PLAYER_SIZE.y }, { 0.1, 0.05 });
    if (isOnGround) {
        player->velocity.x /= 1.0 + delta * PLAYER_GROUND_FRICTION_X;

        if (IsKeyReleased(KEY_SPACE)) {
            // Calculate strength based on how long the user held down the jump key.
            // The numbers are kind of random, you play with it yourself.
            const float jumpStrength = Clamp(player->jumpHoldTime * 3.0f, 1.1f, 2.0f) / 2.0f;

            // If the player doesn't press anything, the direction is up.
            Vector2 dir = {0.0f, -1.0f};
            const float xMoveStrength = 0.75f - (jumpStrength * 0.35f);
            if (IsKeyDown(KEY_RIGHT)) dir.x += xMoveStrength;
            if (IsKeyDown(KEY_LEFT)) dir.x -= xMoveStrength;
            // Make sure the vector is unit vector (length = 1.0).
            dir = Vector2Normalize(dir);

            // Multiply the vector length by the strength factor.
            dir = Vector2Scale(dir, jumpStrength * PLAYER_JUMP_STRENGTH);
            // Now apply the jump vector to the actual velocity
            player->velocity = dir;
        }

        if (IsKeyDown(KEY_SPACE)) {
            player->jumpHoldTime += delta;
        }
        else {
            player->jumpHoldTime = 0.0f;
            if (IsKeyDown(KEY_RIGHT)) player->velocity.x += PLAYER_SPEED * delta;
            if (IsKeyDown(KEY_LEFT)) player->velocity.x -= PLAYER_SPEED * delta;
        }
    }
    else {
        player->jumpHoldTime = 0.0f;
    }

    player->position = Vector2Add(player->position, Vector2Scale(player->velocity, delta));
}



// Entry point of the program
// --------------------------
int main(const char** argv, int argc) {
    // Initialization
    // --------------

    const int initialScreenWidth = TILEMAP_SIZE_X * TILE_PIXELS;
    const int initialScreenHeight = TILEMAP_SIZE_Y * TILE_PIXELS;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(initialScreenWidth, initialScreenHeight, "raylib [core] example - keyboard input");
    SetTargetFPS(60); // Set our game to run at 60 frames-per-second when possible
    bool isDebugEnabled = false;

    Player player = {};
    player.position = { (float)initialScreenWidth / (2 * TILE_PIXELS), (float)initialScreenHeight / (2 * TILE_PIXELS) };

    // Main game loop
    // --------------

    // `WindowShouldClose` detects window close
    while (!WindowShouldClose()) {
        const float delta = Clamp(GetFrameTime(), 0.0001f, 0.1f);

        int screenIndex = arrayNumItems(screenTilemaps) - getScreenHeightIndex(player.position.y) - 2;
        if (screenIndex < 0 || screenIndex > arrayNumItems(screenTilemaps)) screenIndex = 0;

        const Tilemap* tilemap = &screenTilemaps[screenIndex % arrayNumItems(screenTilemaps)];
        const int heightIndex = getScreenHeightIndex(player.position.y);
        const float screenOffsetY = -(float)(heightIndex + 1) * TILEMAP_SIZE_Y;

        // Update
        {
            if (IsKeyPressed(KEY_I)) isDebugEnabled = !isDebugEnabled;
            updatePlayer(&player, tilemap, screenOffsetY, delta);
            resolveBoxCollisionWithTilemap(tilemap, screenOffsetY, &player.position, &player.velocity, PLAYER_SIZE);
        }

        // Draw
        {
            BeginDrawing();

            ClearBackground({ 30, 30, 40, 255 });
            drawTilemap(tilemap);

            if (isDebugEnabled) {
                drawTilemapDebug(tilemap);

                int startX = 0;
                int startY = 0;
                int endX = 0;
                int endY = 0;
                getTilesOverlappedByBox(&startX, &startY, &endX, &endY, player.position, PLAYER_SIZE);

                for (int x = startX; x <= endX; x++) {
                    for (int y = startY; y <= endY; y++) {
                        DrawRectangle(x * TILE_PIXELS, y * TILE_PIXELS, TILE_PIXELS, TILE_PIXELS, Fade(RED, 0.5));
                    }
                }
            }

            // Draw player, but relative to current screen
            DrawCircleV(worldToScreen({ player.position.x, player.position.y - screenOffsetY }), TILE_PIXELS * PLAYER_SIZE.y, WHITE);

            if (isDebugEnabled) {
                DrawFPS(1, 1);
                DrawText(TextFormat("player.position = [%f, %f]", player.position.x, player.position.y), 1, 22, 20, GREEN);
                DrawText(TextFormat("player.jumpHoldTime = %f", player.jumpHoldTime), 1, 88, 20, GREEN);
                DrawText(TextFormat("screenOffset = %f", screenOffsetY), 1, 44, 20, GREEN);
                DrawText(TextFormat("screenIndex = %i", screenIndex), 1, 66, 20, GREEN);
            }

            EndDrawing();
        }
    }

    // Shutdown
    // --------

    CloseWindow(); // Close window and OpenGL context

    return 0;
}