#include <raylib.h>
#include <rlgl.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// State machine struct protected by std::mutex
struct DashboardState {
    std::string timeStr;
    std::string weatherStr;
    std::string activeSpeaker; // "Frank", "Sam", "Sky", "Milo"
    std::string targetFriend;  // "Frank", "Sam", "Sky", "Milo", or "Center"
    std::string emotion;       // "happy", "panicked", "smug", "excited"
    std::string action;        // "back_pat", "high_five", "sharing_photo", "waving"
    std::string dialogueText;
    std::string photoPath;
    Color glowTint;
};

// Character Sprite Sheet & Spatial Position Entry
struct CharacterSpriteEntry {
    std::string name;
    std::string tag;
    std::string spriteSheetPath;
    Texture2D sheetTexture;
    Vector3 position3D;
    Color sigColor;
    bool flipX; // Horizontal flip for eye contact mirroring
};

// Global thread-safe state and mutex
DashboardState g_State;
std::mutex g_StateMutex;
std::atomic<bool> g_Running{ true };

// Helper signature colors
Color GetSignatureColor(const std::string& name) {
    if (name == "Frank") return Color{ 255, 105, 180, 255 }; // Pink
    if (name == "Milo")  return Color{ 64, 156, 255, 255 };  // Blue
    if (name == "Sam")   return Color{ 46, 204, 113, 255 };  // Emerald
    if (name == "Sky")   return Color{ 255, 191, 0, 255 };   // Amber
    return Color{ 200, 210, 230, 255 };
}

// Procedural Fallback Photo Texture
Texture2D GeneratePhotoFallbackTexture(int width, int height) {
    Image img = GenImageGradientLinear(width, height, 45, Color{ 40, 20, 75, 255 }, Color{ 230, 100, 130, 255 });
    for (int y = 0; y < height; y += 25) {
        for (int x = 0; x < width; x++) {
            ImageDrawPixel(&img, x, y, Color{ 255, 255, 255, 25 });
        }
    }
    for (int x = 0; x < width; x += 25) {
        for (int y = 0; y < height; y++) {
            ImageDrawPixel(&img, x, y, Color{ 255, 255, 255, 25 });
        }
    }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

// SCREEN-SPACE HUD OVERLAYS (Glassmorphic Speech Cards)
void DrawHighContrastSpeechCard(Vector2 pos, std::string speakerName, std::string text, Color sigColor) {
    int fontSize = 22;
    int titleSize = 18;
    int padding = 22;
    int maxWidth = 420;

    std::vector<std::string> lines;
    std::string currentLine = "";
    std::string word = "";

    for (size_t i = 0; i <= text.length(); ++i) {
        if (i < text.length() && text[i] != ' ' && text[i] != '\n') {
            word += text[i];
        } else {
            std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
            if (MeasureText(testLine.c_str(), fontSize) > maxWidth - (padding * 2)) {
                if (!currentLine.empty()) lines.push_back(currentLine);
                currentLine = word;
            } else {
                currentLine = testLine;
            }
            word = "";
        }
    }
    if (!currentLine.empty()) lines.push_back(currentLine);
    if (lines.empty()) lines.push_back("");

    int calculatedWidth = maxWidth;
    int calculatedHeight = 32 + (lines.size() * (fontSize + 6)) + (padding * 2);

    Rectangle cardRect = {
        pos.x - (calculatedWidth / 2.0f),
        pos.y - calculatedHeight - 45.0f,
        (float)calculatedWidth,
        (float)calculatedHeight
    };

    if (cardRect.x < 30.0f) cardRect.x = 30.0f;
    if (cardRect.x + cardRect.width > 1890.0f) cardRect.x = 1890.0f - cardRect.width;
    if (cardRect.y < 30.0f) cardRect.y = 30.0f;

    // Drop shadow
    DrawRectangleRounded(Rectangle{ cardRect.x + 8, cardRect.y + 8, cardRect.width, cardRect.height }, 0.2f, 8, Color{ 0, 0, 0, 160 });

    // Dark glass card backdrop
    DrawRectangleRounded(cardRect, 0.2f, 8, Color{ 12, 16, 28, 245 });

    // Glowing border outline
    DrawRectangleRoundedLines(cardRect, 0.2f, 8, 3.0f, ColorAlpha(sigColor, 0.95f));

    // Header badge
    std::string headerStr = speakerName + " says:";
    DrawText(headerStr.c_str(), (int)(cardRect.x + padding), (int)(cardRect.y + padding), titleSize, sigColor);

    // Dialogue text
    float lineY = cardRect.y + padding + titleSize + 10;
    for (const auto& line : lines) {
        DrawText(line.c_str(), (int)(cardRect.x + padding), (int)lineY, fontSize, Color{ 255, 255, 255, 255 });
        lineY += fontSize + 6;
    }

    // Pointer arrow down to billboard head
    Vector2 p1 = { pos.x - 14.0f, cardRect.y + cardRect.height };
    Vector2 p2 = { pos.x + 14.0f, cardRect.y + cardRect.height };
    Vector2 p3 = { pos.x, pos.y - 30.0f };
    DrawTriangle(p1, p3, p2, Color{ 12, 16, 28, 245 });
}

// Background State Thread
void BackgroundStateLoop() {
    const std::string jsonPath = "state.json";

    std::vector<DashboardState> script = {
        { "07:47 PM", "72°F Sunset Glow", "Frank", "Sam", "happy", "sharing_photo", "Check out this sunset view from the mountain ridge yesterday!", "assets/photo1.jpg", Color{ 245, 125, 75, 180 } },
        { "07:47 PM", "72°F Sunset Glow", "Milo", "Sky", "smug", "high_five", "Frank is horizontally mirrored facing left toward the group!", "assets/photo1.jpg", Color{ 64, 156, 255, 180 } },
        { "07:48 PM", "71°F Golden Hour", "Sam", "Frank", "excited", "back_pat", "Sky and Sam elevated +0.2f cleanly above coffee table at Z = -2.2f!", "assets/photo1.jpg", Color{ 46, 204, 113, 180 } },
        { "07:48 PM", "71°F Golden Hour", "Sky", "Milo", "panicked", "waving", "Zero wireframes! Active photo texture bound directly to 16:9 wall TV frame!", "assets/photo1.jpg", Color{ 255, 191, 0, 180 } }
    };

    size_t scriptIndex = 0;
    auto lastAutoAdvance = std::chrono::steady_clock::now();

    while (g_Running) {
        bool loadedFromJson = false;
        
        if (std::filesystem::exists(jsonPath)) {
            try {
                std::ifstream file(jsonPath);
                if (file.is_open()) {
                    json j;
                    file >> j;
                    DashboardState newState;
                    newState.timeStr = j.value("timeStr", "07:47 PM");
                    newState.weatherStr = j.value("weatherStr", "72°F Sunset Glow");
                    newState.activeSpeaker = j.value("activeSpeaker", "Frank");
                    newState.targetFriend = j.value("targetFriend", "Sam");
                    newState.emotion = j.value("emotion", "happy");
                    newState.action = j.value("action", "sharing_photo");
                    newState.dialogueText = j.value("dialogueText", "Hello spatial lounge!");
                    newState.photoPath = j.value("photoPath", "");
                    newState.glowTint = GetSignatureColor(newState.activeSpeaker);
                    newState.glowTint.a = 180;

                    {
                        std::lock_guard<std::mutex> lock(g_StateMutex);
                        g_State = newState;
                    }
                    loadedFromJson = true;
                }
            } catch (...) {}
        }

        auto now = std::chrono::steady_clock::now();
        if (!loadedFromJson && std::chrono::duration_cast<std::chrono::seconds>(now - lastAutoAdvance).count() >= 20) {
            lastAutoAdvance = now;
            scriptIndex = (scriptIndex + 1) % script.size();
            {
                std::lock_guard<std::mutex> lock(g_StateMutex);
                g_State = script[scriptIndex];
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

int main() {
    const int CANVAS_WIDTH = 1920;
    const int CANVAS_HEIGHT = 1080;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Ambient Lounge - Spatial Viewport");
    SetTargetFPS(60);

    // Virtual 1080p Viewport Target
    RenderTexture2D target = LoadRenderTexture(CANVAS_WIDTH, CANVAS_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    // Fixed 3D Perspective Camera Setup
    Camera3D camera = { 0 };
    camera.position = Vector3{ 0.0f, 3.0f, 8.5f };
    camera.target   = Vector3{ 0.0f, 1.2f, -3.5f };
    camera.up       = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Initial state setup
    {
        std::lock_guard<std::mutex> lock(g_StateMutex);
        g_State.timeStr = "07:47 PM";
        g_State.weatherStr = "72°F Sunset Glow";
        g_State.activeSpeaker = "Frank";
        g_State.targetFriend = "Sam";
        g_State.emotion = "happy";
        g_State.action = "sharing_photo";
        g_State.dialogueText = "Check out this sunset view from the mountain ridge yesterday!";
        g_State.photoPath = "";
        g_State.glowTint = Color{ 245, 125, 75, 180 };
    }

    // Launch background thread
    std::thread bgThread(BackgroundStateLoop);

    // Load Room Environment Textures
    Texture2D floorTex = std::filesystem::exists("assets/3d/floor_diffuse.png") ? LoadTexture("assets/3d/floor_diffuse.png") : GeneratePhotoFallbackTexture(1024, 1024);
    Texture2D wallTex  = std::filesystem::exists("assets/3d/wall_diffuse.png") ? LoadTexture("assets/3d/wall_diffuse.png") : GeneratePhotoFallbackTexture(1024, 1024);
    
    // 2. BIND ACTIVE PHOTO TO TV FRAME: Load photo texture from disk/RAM
    Texture2D activePhotoTex = std::filesystem::exists("assets/photo1.jpg") ? LoadTexture("assets/photo1.jpg") : GeneratePhotoFallbackTexture(1920, 1080);

    // Build 3D Floor Plane Model (Y = 0.0f)
    Mesh floorMesh = GenMeshPlane(24.0f, 24.0f, 1, 1);
    Model floorModel = LoadModelFromMesh(floorMesh);
    SetMaterialTexture(&floorModel.materials[0], MATERIAL_MAP_DIFFUSE, floorTex);

    // Build 3D Back Wall Model
    Mesh wallMesh = GenMeshPlane(24.0f, 12.0f, 1, 1);
    Model wallModel = LoadModelFromMesh(wallMesh);
    SetMaterialTexture(&wallModel.materials[0], MATERIAL_MAP_DIFFUSE, wallTex);

    // 2. BIND ACTIVE PHOTO TO TV FRAME: Bind activePhotoTex directly to 16:9 wall frame model material
    Mesh photoMesh = GenMeshPlane(6.0f, 3.375f, 1, 1);
    Model photoModel = LoadModelFromMesh(photoMesh);
    SetMaterialTexture(&photoModel.materials[0], MATERIAL_MAP_DIFFUSE, activePhotoTex);

    // SPRITE SHEET ASSET LOADING (3x3 grid transparent PNG sheets)
    Texture2D frankSheet = LoadTexture("assets/sprites/frank_spritesheet.png");
    Texture2D miloSheet  = LoadTexture("assets/sprites/milo_spritesheet.png");
    Texture2D samSheet   = LoadTexture("assets/sprites/sam_spritesheet.png");
    Texture2D skySheet   = LoadTexture("assets/sprites/sky_spritesheet.png");

    // 3. ADJUST COFFEE TABLE Z-DEPTH & CHARACTER ELEVATION:
    // Coffee Table shifted forward to Z = -2.2f
    Vector3 coffeeTablePos = { 0.0f, 0.25f, -2.2f };
    Vector3 centerPhotoPlanePos = { 0.0f, 3.5f, -7.8f };

    // Elevated Sky & Sam Y positions by +0.2f (1.45f) so torsos sit clearly above coffee table back edge
    // 4. HORIZONTAL SPRITE FLIP FOR FRANK (RIGHT SIDE): flipX = true negates sourceRec.width
    std::vector<CharacterSpriteEntry> characterSprites = {
        { "Milo",  "⚡", "assets/sprites/milo_spritesheet.png",  miloSheet,  Vector3{ -3.5f, 1.25f, -3.0f }, GetSignatureColor("Milo"),  false }, // Far Left (Facing Right)
        { "Sky",   "☀️", "assets/sprites/sky_spritesheet.png",   skySheet,   Vector3{ -1.8f, 1.45f, -4.2f }, GetSignatureColor("Sky"),   false }, // Mid Left (Elevated Y = 1.45f)
        { "Sam",   "🚆", "assets/sprites/sam_spritesheet.png",   samSheet,   Vector3{  1.8f, 1.45f, -4.2f }, GetSignatureColor("Sam"),   true  }, // Mid Right (Elevated Y = 1.45f, Flipped Left)
        { "Frank", "📸", "assets/sprites/frank_spritesheet.png", frankSheet, Vector3{  3.5f, 1.25f, -3.0f }, GetSignatureColor("Frank"), true  }  // Far Right (Flipped Left -> sourceRec.width = -frameWidth)
    };

    // ANIMATION FRAME CONTROLLER
    float animTimer = 0.0f;
    int currentFrame = 0;
    float globalTime = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        globalTime += dt;

        // Tick animation frame index every 0.15 seconds
        animTimer += dt;
        if (animTimer >= 0.15f) {
            animTimer = 0.0f;
            currentFrame = (currentFrame + 1) % 9; // 3x3 grid = 9 frames
        }

        // Snapshot state safely
        DashboardState localState;
        {
            std::lock_guard<std::mutex> lock(g_StateMutex);
            localState = g_State;
        }

        // -------------------------------------------------------------
        // RENDER 3D SPATIAL CANVAS WITH ANIMATED 2D BILLBOARD SPRITES
        // -------------------------------------------------------------
        BeginTextureMode(target);
            ClearBackground(Color{ 10, 14, 24, 255 });

            // Begin 3D Scene Rendering
            BeginMode3D(camera);

                // 1. DELETE WIREFRAME DRAW CALLS: Render only solid 3D models (NO DrawBoundingBox, NO DrawCubeWires, NO DrawLine3D)
                DrawModel(floorModel, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
                DrawModelEx(wallModel, Vector3{ 0.0f, 5.0f, -8.0f }, Vector3{ 1.0f, 0.0f, 0.0f }, 90.0f, Vector3{ 1.0f, 1.0f, 1.0f }, WHITE);

                // 3D Wooden Coffee Table at Z = -2.2f (NO WIREFRAMES)
                DrawCube(coffeeTablePos, 3.5f, 0.4f, 2.0f, Color{ 65, 42, 28, 255 });
                DrawCylinder(Vector3{ -1.5f, 0.0f, -3.0f }, 0.08f, 0.08f, 0.3f, 8, Color{ 35, 25, 20, 255 });
                DrawCylinder(Vector3{  1.5f, 0.0f, -3.0f }, 0.08f, 0.08f, 0.3f, 8, Color{ 35, 25, 20, 255 });
                DrawCylinder(Vector3{ -1.5f, 0.0f, -1.4f }, 0.08f, 0.08f, 0.3f, 8, Color{ 35, 25, 20, 255 });
                DrawCylinder(Vector3{  1.5f, 0.0f, -1.4f }, 0.08f, 0.08f, 0.3f, 8, Color{ 35, 25, 20, 255 });

                // 2. BIND ACTIVE PHOTO TO TV FRAME: Render 16:9 TV Display model with activePhotoTex (NO WIREFRAMES)
                DrawCube(centerPhotoPlanePos, 6.3f, 3.675f, 0.12f, Color{ 16, 22, 34, 255 });
                DrawModelEx(photoModel, Vector3{ centerPhotoPlanePos.x, centerPhotoPlanePos.y, centerPhotoPlanePos.z + 0.07f },
                            Vector3{ 1.0f, 0.0f, 0.0f }, 90.0f, Vector3{ 1.0f, 1.0f, 1.0f }, WHITE);

                // Disable depth mask for transparent billboard pass to fix depth clipping
                rlDisableDepthMask();

                // 2D BILLBOARD PASS WITH HORIZONTAL FLIPPING FOR EYE CONTACT
                Vector2 billboardSize = { 2.0f, 2.5f };

                for (const auto& c : characterSprites) {
                    // Micro breathing float motion
                    float breath = sinf(globalTime * 2.2f + c.position3D.x) * 0.04f;
                    Vector3 billboardPos = { c.position3D.x, c.position3D.y + breath, c.position3D.z };

                    // Calculate source cropping rectangle (3x3 grid)
                    float frameWidth  = (float)c.sheetTexture.width / 3.0f;
                    float frameHeight = (float)c.sheetTexture.height / 3.0f;
                    
                    // 4. HORIZONTAL SPRITE FLIP FOR FRANK (RIGHT SIDE): Negate source rectangle width if flipX is true
                    float signedWidth = c.flipX ? -frameWidth : frameWidth;
                    float srcX = c.flipX ? ((currentFrame % 3) + 1) * frameWidth : (currentFrame % 3) * frameWidth;

                    Rectangle sourceRec = {
                        srcX,
                        (float)(currentFrame / 3) * frameHeight,
                        signedWidth,
                        frameHeight
                    };

                    // Draw Billboard via DrawBillboardRec
                    DrawBillboardRec(camera, c.sheetTexture, sourceRec, billboardPos, billboardSize, WHITE);
                }

                // Restore depth mask writing
                rlEnableDepthMask();

            EndMode3D();

            // ---------------------------------------------------------
            // SCREEN-SPACE HUD OVERLAYS (GetWorldToScreen Tracking)
            // ---------------------------------------------------------
            
            // Top Center Clock Badge
            Rectangle clockRect = { CANVAS_WIDTH / 2.0f - 150.0f, 25.0f, 300.0f, 44.0f };
            DrawRectangleRounded(clockRect, 0.45f, 6, Color{ 12, 16, 28, 235 });
            DrawRectangleRoundedLines(clockRect, 0.45f, 6, 1.5f, Color{ 255, 255, 255, 60 });
            std::string clockText = localState.timeStr + "  |  " + localState.weatherStr;
            int clockTextW = MeasureText(clockText.c_str(), 18);
            DrawText(clockText.c_str(), (int)(CANVAS_WIDTH / 2.0f - clockTextW / 2.0f), (int)(clockRect.y + 12.0f), 18, Color{ 245, 248, 255, 255 });

            // Action Badge at bottom center
            Color activeColor = GetSignatureColor(localState.activeSpeaker);
            std::string actionBadge = "[" + localState.emotion + "] " + localState.action;
            Rectangle actionRect = { CANVAS_WIDTH / 2.0f - 160.0f, CANVAS_HEIGHT - 75.0f, 320.0f, 36.0f };
            DrawRectangleRounded(actionRect, 0.45f, 6, Color{ 12, 16, 28, 235 });
            DrawRectangleRoundedLines(actionRect, 0.45f, 6, 1.5f, ColorAlpha(activeColor, 0.9f));
            int actionTextW = MeasureText(actionBadge.c_str(), 16);
            DrawText(actionBadge.c_str(), (int)(CANVAS_WIDTH / 2.0f - actionTextW / 2.0f), (int)(actionRect.y + 9.0f), 16, Color{ 245, 248, 255, 255 });

            // Position glassmorphic dialogue cards directly above 3D billboard head coordinates (NO WIREFRAMES)
            for (const auto& c : characterSprites) {
                Vector3 headWorldPos = { c.position3D.x, c.position3D.y + 1.25f, c.position3D.z };
                Vector2 screenPos = GetWorldToScreen(headWorldPos, camera);

                if (c.name == localState.activeSpeaker && !localState.dialogueText.empty()) {
                    DrawHighContrastSpeechCard(screenPos, c.name, localState.dialogueText, activeColor);
                }

                // 2D Character Tag (NO WIREFRAMES)
                std::string tagText = c.name + " " + c.tag;
                int tagW = MeasureText(tagText.c_str(), 16);
                Vector3 tagWorldPos = { c.position3D.x, c.position3D.y - 1.4f, c.position3D.z };
                Vector2 tagScreenPos = GetWorldToScreen(tagWorldPos, camera);
                Rectangle tagRect = { tagScreenPos.x - (tagW / 2.0f) - 10, tagScreenPos.y, (float)tagW + 20, 26 };
                DrawRectangleRounded(tagRect, 0.45f, 4, Color{ 12, 16, 28, 220 });
                DrawRectangleRoundedLines(tagRect, 0.45f, 4, 1.2f, ColorAlpha(c.sigColor, 0.8f));
                DrawText(tagText.c_str(), (int)(tagScreenPos.x - tagW / 2.0f), (int)(tagRect.y + 4), 16, WHITE);
            }

        EndTextureMode();

        // -------------------------------------------------------------
        // RENDER VIRTUAL VIEWPORT TO WINDOW WITH ASPECT RATIO FIT
        // -------------------------------------------------------------
        BeginDrawing();
            ClearBackground(BLACK);

            float windowW = (float)GetScreenWidth();
            float windowH = (float)GetScreenHeight();
            float scale = fminf(windowW / (float)CANVAS_WIDTH, windowH / (float)CANVAS_HEIGHT);

            Rectangle srcRect = { 0, 0, (float)CANVAS_WIDTH, (float)-CANVAS_HEIGHT }; // Y-flipped for render target
            Rectangle destRect = {
                (windowW - ((float)CANVAS_WIDTH * scale)) * 0.5f,
                (windowH - ((float)CANVAS_HEIGHT * scale)) * 0.5f,
                (float)CANVAS_WIDTH * scale,
                (float)CANVAS_HEIGHT * scale
            };

            DrawTexturePro(target.texture, srcRect, destRect, Vector2{ 0, 0 }, 0.0f, WHITE);
        EndDrawing();
    }

    // Stop background thread safely
    g_Running = false;
    if (bgThread.joinable()) {
        bgThread.join();
    }

    // Cleanup resources
    UnloadTexture(floorTex);
    UnloadTexture(wallTex);
    UnloadTexture(activePhotoTex);
    UnloadTexture(frankSheet);
    UnloadTexture(miloSheet);
    UnloadTexture(samSheet);
    UnloadTexture(skySheet);
    UnloadModel(floorModel);
    UnloadModel(wallModel);
    UnloadModel(photoModel);
    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
