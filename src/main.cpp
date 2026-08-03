#include <raylib.h>
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

// 3D Spatial Friend Anchor & Rig representation
struct Friend3DAnchor {
    std::string name;
    std::string tag;
    Vector3 position;
    Color sigColor;
    Color hairColor;
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

// 4. SPEECH BUBBLES (High-Contrast Glassmorphic Screen-Space Card Overlay)
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
        pos.y - calculatedHeight - 40.0f,
        (float)calculatedWidth,
        (float)calculatedHeight
    };

    if (cardRect.x < 30.0f) cardRect.x = 30.0f;
    if (cardRect.x + cardRect.width > 1890.0f) cardRect.x = 1890.0f - cardRect.width;
    if (cardRect.y < 30.0f) cardRect.y = 30.0f;

    // High contrast drop shadow
    DrawRectangleRounded(Rectangle{ cardRect.x + 8, cardRect.y + 8, cardRect.width, cardRect.height }, 0.2f, 8, Color{ 0, 0, 0, 160 });

    // High contrast dark-glass card backdrop
    DrawRectangleRounded(cardRect, 0.2f, 8, Color{ 12, 16, 28, 245 });

    // Glowing border outline
    DrawRectangleRoundedLines(cardRect, 0.2f, 8, 3.0f, ColorAlpha(sigColor, 0.95f));

    // Header badge
    std::string headerStr = speakerName + " says:";
    DrawText(headerStr.c_str(), (int)(cardRect.x + padding), (int)(cardRect.y + padding), titleSize, sigColor);

    // Render dialogue text
    float lineY = cardRect.y + padding + titleSize + 10;
    for (const auto& line : lines) {
        DrawText(line.c_str(), (int)(cardRect.x + padding), (int)lineY, fontSize, Color{ 255, 255, 255, 255 });
        lineY += fontSize + 6;
    }

    // Pointer arrow
    Vector2 p1 = { pos.x - 14.0f, cardRect.y + cardRect.height };
    Vector2 p2 = { pos.x + 14.0f, cardRect.y + cardRect.height };
    Vector2 p3 = { pos.x, pos.y - 30.0f };
    DrawTriangle(p1, p3, p2, Color{ 12, 16, 28, 245 });
}

// Background Network & State Thread
void BackgroundStateLoop() {
    const std::string jsonPath = "state.json";

    std::vector<DashboardState> script = {
        { "03:58 PM", "72°F Sunset Glow", "Frank", "Sam", "happy", "sharing_photo", "Check out this sunset view from the mountain ridge yesterday!", "assets/photo1.jpg", Color{ 245, 125, 75, 180 } },
        { "03:58 PM", "72°F Sunset Glow", "Milo", "Sky", "smug", "high_five", "Told you our textured 3D spatial lounge would look like a real room!", "assets/photo1.jpg", Color{ 64, 156, 255, 180 } },
        { "03:59 PM", "71°F Golden Hour", "Sam", "Frank", "excited", "back_pat", "Awesome capture Frank! The 3D hardwood floor and ambient light glow look incredible!", "assets/photo1.jpg", Color{ 46, 204, 113, 180 } },
        { "03:59 PM", "71°F Golden Hour", "Sky", "Milo", "panicked", "waving", "Wait, did anyone check the 3D coffee table and skeletal gesture animations?", "assets/photo1.jpg", Color{ 255, 191, 0, 180 } }
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
                    newState.timeStr = j.value("timeStr", "03:58 PM");
                    newState.weatherStr = j.value("weatherStr", "72°F Sunset Glow");
                    newState.activeSpeaker = j.value("activeSpeaker", "Frank");
                    newState.targetFriend = j.value("targetFriend", "Sam");
                    newState.emotion = j.value("emotion", "happy");
                    newState.action = j.value("action", "sharing_photo");
                    newState.dialogueText = j.value("dialogueText", "Hello photorealistic 3D spatial lounge!");
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

// Render 3D Character Model / Rig with Dynamic Forward Kinematics Skeletal Gestures
void DrawCharacter3DModel(const Friend3DAnchor& friendAnchor, bool isActive, bool isTarget, const DashboardState& state, float globalTime, Vector3 tvPos) {
    Color skinColor = Color{ 235, 195, 165, 255 };

    // Micro breathing vertical float
    float breath = sinf(globalTime * 2.2f + friendAnchor.position.x) * 0.05f;
    Vector3 rootPos = { friendAnchor.position.x, friendAnchor.position.y + breath, friendAnchor.position.z };

    // Torso Base & Outfit
    Vector3 torsoBottom = { rootPos.x, rootPos.y - 0.4f, rootPos.z };
    Vector3 torsoTop    = { rootPos.x, rootPos.y + 0.7f, rootPos.z };
    DrawCapsule(torsoBottom, torsoTop, 0.42f, 12, 12, friendAnchor.sigColor);

    // Active / Target Floor Aura Ring in 3D
    if (isActive) {
        float auraPulse = 0.85f + sinf(globalTime * 5.0f) * 0.12f;
        DrawCircle3D(Vector3{ rootPos.x, rootPos.y - 1.1f, rootPos.z }, auraPulse, Vector3{ 1.0f, 0.0f, 0.0f }, 90.0f, ColorAlpha(friendAnchor.sigColor, 0.7f));
    } else if (isTarget) {
        DrawCircle3D(Vector3{ rootPos.x, rootPos.y - 1.1f, rootPos.z }, 0.80f, Vector3{ 1.0f, 0.0f, 0.0f }, 90.0f, ColorAlpha(WHITE, 0.5f));
    }

    // Head Joint Position & Dynamic Gaze Rotation
    Vector3 headPos = { rootPos.x, rootPos.y + 1.25f, rootPos.z };
    
    Vector3 gazeTargetPos = tvPos;
    if (state.activeSpeaker != friendAnchor.name) {
        gazeTargetPos = tvPos; // Gaze toward 3D TV screen by default
    }

    Vector3 gazeDir = { gazeTargetPos.x - headPos.x, gazeTargetPos.y - headPos.y, gazeTargetPos.z - headPos.z };
    float dist = sqrtf(gazeDir.x * gazeDir.x + gazeDir.y * gazeDir.y + gazeDir.z * gazeDir.z);
    if (dist > 0.001f) {
        gazeDir.x /= dist; gazeDir.y /= dist; gazeDir.z /= dist;
    }

    // Head Mesh & Hair
    DrawSphere(headPos, 0.38f, skinColor);
    DrawSphere(Vector3{ headPos.x, headPos.y + 0.12f, headPos.z }, 0.40f, friendAnchor.hairColor);

    // Eyes / Pupil Gaze Indicators
    Vector3 leftEye  = { headPos.x - 0.12f + gazeDir.x * 0.28f, headPos.y + 0.04f + gazeDir.y * 0.15f, headPos.z + 0.30f + gazeDir.z * 0.28f };
    Vector3 rightEye = { headPos.x + 0.12f + gazeDir.x * 0.28f, headPos.y + 0.04f + gazeDir.y * 0.15f, headPos.z + 0.30f + gazeDir.z * 0.28f };
    DrawSphere(leftEye, 0.06f, WHITE);
    DrawSphere(rightEye, 0.06f, WHITE);
    DrawSphere(Vector3{ leftEye.x + gazeDir.x * 0.02f, leftEye.y, leftEye.z + gazeDir.z * 0.02f }, 0.035f, friendAnchor.sigColor);
    DrawSphere(Vector3{ rightEye.x + gazeDir.x * 0.02f, rightEye.y, rightEye.z + gazeDir.z * 0.02f }, 0.035f, friendAnchor.sigColor);

    // -------------------------------------------------------------
    // FORWARD KINEMATICS SKELETAL ARMS & GESTURES
    // -------------------------------------------------------------
    Vector3 rShoulder = { rootPos.x + 0.52f, rootPos.y + 0.55f, rootPos.z };
    Vector3 lShoulder = { rootPos.x - 0.52f, rootPos.y + 0.55f, rootPos.z };

    Vector3 rHandPos = { rShoulder.x + 0.15f, rShoulder.y - 0.60f, rShoulder.z + 0.1f };
    Vector3 lHandPos = { lShoulder.x - 0.15f, lShoulder.y - 0.60f, lShoulder.z + 0.1f };

    if (isActive) {
        if (state.action == "sharing_photo") {
            rHandPos = { rShoulder.x + 0.10f, rShoulder.y + 0.10f, rShoulder.z - 1.20f };
        }
        else if (state.action == "high_five") {
            rHandPos = { rShoulder.x + 0.25f, rShoulder.y + 0.85f, rShoulder.z - 0.20f };
        }
        else if (state.action == "back_pat") {
            rHandPos = { rShoulder.x + 0.40f, rShoulder.y + 0.20f, rShoulder.z - 0.80f };
        }
        else if (state.action == "waving") {
            float sway = sinf(globalTime * 6.0f) * 0.35f;
            rHandPos = { rShoulder.x + 0.20f + sway, rShoulder.y + 0.90f, rShoulder.z - 0.10f };
        }
    }

    // Right & Left Arm Segments
    Vector3 rElbow = { (rShoulder.x + rHandPos.x) * 0.5f, (rShoulder.y + rHandPos.y) * 0.5f - 0.1f, (rShoulder.z + rHandPos.z) * 0.5f };
    DrawCylinderEx(rShoulder, rElbow, 0.12f, 0.10f, 10, friendAnchor.sigColor);
    DrawCylinderEx(rElbow, rHandPos, 0.10f, 0.08f, 10, friendAnchor.sigColor);
    DrawSphere(rHandPos, 0.10f, skinColor);

    Vector3 lElbow = { (lShoulder.x + lHandPos.x) * 0.5f, (lShoulder.y + lHandPos.y) * 0.5f - 0.05f, (lShoulder.z + lHandPos.z) * 0.5f };
    DrawCylinderEx(lShoulder, lElbow, 0.12f, 0.10f, 10, friendAnchor.sigColor);
    DrawCylinderEx(lElbow, lHandPos, 0.10f, 0.08f, 10, friendAnchor.sigColor);
    DrawSphere(lHandPos, 0.10f, skinColor);
}

int main() {
    const int CANVAS_WIDTH = 1920;
    const int CANVAS_HEIGHT = 1080;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Ambient Lounge - Photorealistic 3D Spatial Canvas");
    SetTargetFPS(60);

    // Virtual 1080p Viewport Target
    RenderTexture2D target = LoadRenderTexture(CANVAS_WIDTH, CANVAS_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    // Fixed 3D Perspective Camera Setup
    Camera3D camera = { 0 };
    camera.position = Vector3{ 0.0f, 2.5f, 10.5f };
    camera.target   = Vector3{ 0.0f, 0.2f, 0.0f };
    camera.up       = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Initial state setup
    {
        std::lock_guard<std::mutex> lock(g_StateMutex);
        g_State.timeStr = "03:58 PM";
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

    // Load 3D Room Textures
    Texture2D floorTex = std::filesystem::exists("assets/3d/floor_diffuse.png") ? LoadTexture("assets/3d/floor_diffuse.png") : GeneratePhotoFallbackTexture(1024, 1024);
    Texture2D wallTex  = std::filesystem::exists("assets/3d/wall_diffuse.png") ? LoadTexture("assets/3d/wall_diffuse.png") : GeneratePhotoFallbackTexture(1024, 1024);
    Texture2D activePhotoTex = GeneratePhotoFallbackTexture(1920, 1080);

    // Build 3D Floor Plane Model & Material Texture
    Mesh floorMesh = GenMeshPlane(24.0f, 24.0f, 1, 1);
    Model floorModel = LoadModelFromMesh(floorMesh);
    SetMaterialTexture(&floorModel.materials[0], MATERIAL_MAP_DIFFUSE, floorTex);

    // Build 3D Back Wall Model & Material Texture
    Mesh wallMesh = GenMeshPlane(24.0f, 12.0f, 1, 1);
    Model wallModel = LoadModelFromMesh(wallMesh);
    SetMaterialTexture(&wallModel.materials[0], MATERIAL_MAP_DIFFUSE, wallTex);

    std::vector<Friend3DAnchor> anchors3D = {
        { "Frank", "📸", Vector3{ 0.0f, 1.6f, -2.4f }, GetSignatureColor("Frank"), Color{ 50, 40, 35, 255 } }, // TOP
        { "Sam",   "🚆", Vector3{ 0.0f, -1.6f, 2.4f }, GetSignatureColor("Sam"),   Color{ 70, 50, 30, 255 } }, // BOTTOM
        { "Sky",   "☀️", Vector3{ -4.6f, 0.0f, 0.0f }, GetSignatureColor("Sky"),   Color{ 200, 160, 60, 255 } },// LEFT
        { "Milo",  "⚡", Vector3{ 4.6f, 0.0f, 0.0f }, GetSignatureColor("Milo"),  Color{ 30, 35, 50, 255 } }  // RIGHT
    };

    Vector3 centerPhotoPlanePos = { 0.0f, 1.2f, -4.8f };
    Vector3 coffeeTablePos     = { 0.0f, -1.2f, 0.0f };

    float globalTime = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        globalTime += dt;

        // Snapshot state safely
        DashboardState localState;
        {
            std::lock_guard<std::mutex> lock(g_StateMutex);
            localState = g_State;
        }

        // -------------------------------------------------------------
        // RENDER 3D SPATIAL SCENE & 2D HUD OVERLAY
        // -------------------------------------------------------------
        BeginTextureMode(target);
            ClearBackground(Color{ 10, 14, 24, 255 });

            // Begin 3D Scene Rendering
            BeginMode3D(camera);

                // 1. 3D HARDWOOD/SLATE FLOOR PLANE
                DrawModel(floorModel, Vector3{ 0.0f, -1.8f, 0.0f }, 1.0f, WHITE);

                // 2. 3D LOUNGE BACK WALL (Rotated vertical at Z = -5.0)
                DrawModelEx(wallModel, Vector3{ 0.0f, 3.2f, -5.0f }, Vector3{ 1.0f, 0.0f, 0.0f }, 90.0f, Vector3{ 1.0f, 1.0f, 1.0f }, WHITE);

                // 3. 3D WOODEN COFFEE TABLE IN CENTER LOUNGE
                DrawCube(coffeeTablePos, 3.5f, 0.4f, 2.0f, Color{ 65, 42, 28, 255 });
                DrawCubeWires(coffeeTablePos, 3.55f, 0.42f, 2.05f, Color{ 110, 80, 50, 255 });
                // Table Legs
                DrawCylinder(Vector3{ -1.5f, -1.5f, -0.8f }, 0.08f, 0.08f, 0.6f, 8, Color{ 35, 25, 20, 255 });
                DrawCylinder(Vector3{  1.5f, -1.5f, -0.8f }, 0.08f, 0.08f, 0.6f, 8, Color{ 35, 25, 20, 255 });
                DrawCylinder(Vector3{ -1.5f, -1.5f,  0.8f }, 0.08f, 0.08f, 0.6f, 8, Color{ 35, 25, 20, 255 });
                DrawCylinder(Vector3{  1.5f, -1.5f,  0.8f }, 0.08f, 0.08f, 0.6f, 8, Color{ 35, 25, 20, 255 });

                // 4. AMBIENT POINT LIGHT GLOW BEHIND 3D TV
                float pulseGlow = (sinf(globalTime * 3.0f) + 1.0f) * 0.5f;
                DrawSphere(Vector3{ centerPhotoPlanePos.x, centerPhotoPlanePos.y, centerPhotoPlanePos.z - 0.2f },
                           3.8f + pulseGlow * 0.2f, ColorAlpha(localState.glowTint, 0.32f));

                // 5. CENTER 3D TV DISPLAY (Textured Quad & Bezel Frame)
                // Metallic Bezel Frame
                DrawCube(centerPhotoPlanePos, 6.3f, 3.675f, 0.12f, Color{ 16, 22, 34, 255 });
                DrawCubeWires(centerPhotoPlanePos, 6.35f, 3.725f, 0.14f, Color{ 140, 160, 190, 255 });

                // Textured 3D Photo Plane Quad facing camera
                DrawPlane(Vector3{ centerPhotoPlanePos.x, centerPhotoPlanePos.y, centerPhotoPlanePos.z + 0.07f },
                          Vector2{ 6.0f, 3.375f }, WHITE);

                // 6. 4 TEXTURED 3D CHARACTER MODELS
                for (const auto& friendAnchor : anchors3D) {
                    bool isActive = (friendAnchor.name == localState.activeSpeaker);
                    bool isTarget = (friendAnchor.name == localState.targetFriend);

                    DrawCharacter3DModel(friendAnchor, isActive, isTarget, localState, globalTime, centerPhotoPlanePos);
                }

                // 7. 3D SPATIAL ENERGY BEAM BETWEEN ACTIVE SPEAKER & TARGET
                Vector3 senderPos3D = { 0, 0, 0 };
                Vector3 targetPos3D = centerPhotoPlanePos;
                Color activeColor = GetSignatureColor(localState.activeSpeaker);

                for (const auto& a : anchors3D) {
                    if (a.name == localState.activeSpeaker) senderPos3D = a.position;
                    if (a.name == localState.targetFriend)  targetPos3D = a.position;
                }
                if (localState.targetFriend == "Center") targetPos3D = centerPhotoPlanePos;

                if (senderPos3D.x != 0 || senderPos3D.y != 0 || senderPos3D.z != 0) {
                    DrawLine3D(senderPos3D, targetPos3D, ColorAlpha(activeColor, 0.75f));
                    float progress = fmodf(globalTime * 0.6f, 1.0f);
                    Vector3 beadPos3D = {
                        senderPos3D.x + (targetPos3D.x - senderPos3D.x) * progress,
                        senderPos3D.y + (targetPos3D.y - senderPos3D.y) * progress,
                        senderPos3D.z + (targetPos3D.z - senderPos3D.z) * progress
                    };
                    DrawSphere(beadPos3D, 0.15f, activeColor);
                }

            EndMode3D();

            // ---------------------------------------------------------
            // 2D GLASSMORPHIC HUD SPEECH CARDS (GetWorldToScreen Tracking)
            // ---------------------------------------------------------
            
            // Top Center Clock Badge
            Rectangle clockRect = { CANVAS_WIDTH / 2.0f - 150.0f, 25.0f, 300.0f, 44.0f };
            DrawRectangleRounded(clockRect, 0.45f, 6, Color{ 12, 16, 28, 235 });
            DrawRectangleRoundedLines(clockRect, 0.45f, 6, 1.5f, Color{ 255, 255, 255, 60 });
            std::string clockText = localState.timeStr + "  |  " + localState.weatherStr;
            int clockTextW = MeasureText(clockText.c_str(), 18);
            DrawText(clockText.c_str(), (int)(CANVAS_WIDTH / 2.0f - clockTextW / 2.0f), (int)(clockRect.y + 12.0f), 18, Color{ 245, 248, 255, 255 });

            // Action Badge at bottom center
            std::string actionBadge = "[" + localState.emotion + "] " + localState.action;
            Rectangle actionRect = { CANVAS_WIDTH / 2.0f - 160.0f, CANVAS_HEIGHT - 75.0f, 320.0f, 36.0f };
            DrawRectangleRounded(actionRect, 0.45f, 6, Color{ 12, 16, 28, 235 });
            DrawRectangleRoundedLines(actionRect, 0.45f, 6, 1.5f, ColorAlpha(activeColor, 0.9f));
            int actionTextW = MeasureText(actionBadge.c_str(), 16);
            DrawText(actionBadge.c_str(), (int)(CANVAS_WIDTH / 2.0f - actionTextW / 2.0f), (int)(actionRect.y + 9.0f), 16, Color{ 245, 248, 255, 255 });

            // Project 3D character head coordinates to 2D screen space for speech cards
            for (const auto& a : anchors3D) {
                Vector3 headWorldPos = { a.position.x, a.position.y + 1.6f, a.position.z };
                Vector2 screenPos = GetWorldToScreen(headWorldPos, camera);

                if (a.name == localState.activeSpeaker && !localState.dialogueText.empty()) {
                    DrawHighContrastSpeechCard(screenPos, a.name, localState.dialogueText, activeColor);
                }

                // High-contrast 2D Character Tag
                std::string tagText = a.name + " " + a.tag;
                int tagW = MeasureText(tagText.c_str(), 16);
                Vector3 tagWorldPos = { a.position.x, a.position.y - 1.2f, a.position.z };
                Vector2 tagScreenPos = GetWorldToScreen(tagWorldPos, camera);
                Rectangle tagRect = { tagScreenPos.x - (tagW / 2.0f) - 10, tagScreenPos.y, (float)tagW + 20, 26 };
                DrawRectangleRounded(tagRect, 0.45f, 4, Color{ 12, 16, 28, 220 });
                DrawRectangleRoundedLines(tagRect, 0.45f, 4, 1.2f, ColorAlpha(a.sigColor, 0.8f));
                DrawText(tagText.c_str(), (int)(tagScreenPos.x - tagW / 2.0f), (int)(tagRect.y + 4), 16, WHITE);
            }

            // Pi 4 Performance Status Footer
            DrawText("Photorealistic 3D Spatial Room | 3D Hardwood Floor, 3D Wall & Coffee Table", 25, CANVAS_HEIGHT - 35, 16, Color{ 160, 180, 210, 180 });

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
    UnloadModel(floorModel);
    UnloadModel(wallModel);
    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
