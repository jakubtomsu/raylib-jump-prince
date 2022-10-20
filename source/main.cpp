#include "raylib.h"

int main() {
    // Initialization
	
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
        EndDrawing();
    }

    // De-Initialization
	
    CloseWindow(); // Close window and OpenGL context

    return 0;
}