#include <raylib.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 2. State Machine Struct
struct DashboardState {
    std::string timeStr;
    std::string weatherStr;
    std::string activeSpeaker;
    std::string targetFriend;
    std::string emotion;
    std::string action;
    std::string dialogueText;
    Texture2D currentPhoto;
};

// Spatial Friend Anchor representation
struct FriendAnchor {
    std::string name;
    Vector2 position;
    Color color;
};

// Particle structure for ambient spatial background
struct AmbientParticle {
    Vector2 pos;
    Vector2 vel;
    float alpha;
    float size;
    float pulseSpeed;
};

// Helper function to create fallback procedural texture if photo missing
Texture2D CreateFallbackPhotoTexture() {
    Image img = GenImageGradientLinear(560, 380, 45, Color{ 45, 25, 85, 255 }, Color{ 235, 110, 128, 255 });
    
    // Add aesthetic spatial grid pattern
    for (int y = 0; y < img.height; y += 20) {
        for (int x = 0; x < img.width; x++) {
            ImageDrawPixel(&img, x, y, Color{ 255, 255, 255, 30 });
        }
    }
    for (int x = 0; x < img.width; x += 20) {
        for (int y = 0; y < img.height; y++) {
            ImageDrawPixel(&img, x, y, Color{ 255, 255, 255, 30 });
        }
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

// 4. Helper function DrawSpeechBubble
void DrawSpeechBubble(Vector2 pos, std::string text, Color color) {
    int fontSize = 22;
    int padding = 20;
    int maxWidth = 380;
    
    // Word wrap calculation
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
    int calculatedHeight = (lines.size() * (fontSize + 6)) + (padding * 2);

    // Center bubble around pos with offset
    Rectangle bubbleRect = {
        pos.x - (calculatedWidth / 2.0f),
        pos.y - (calculatedHeight / 2.0f) - 60.0f,
        (float)calculatedWidth,
        (float)calculatedHeight
    };

    // Soft drop shadow
    Rectangle shadowRect = { bubbleRect.x + 6, bubbleRect.y + 6, bubbleRect.width, bubbleRect.height };
    DrawRectangleRounded(shadowRect, 0.25f, 8, Color{ 0, 0, 0, 80 });

    // Glassmorphic translucent backdrop
    DrawRectangleRounded(bubbleRect, 0.25f, 8, Color{ 18, 22, 36, 215 });

    // Glowing border outline
    Color borderColor = color;
    borderColor.a = 180;
    DrawRectangleRoundedLines(bubbleRect, 0.25f, 8, 2.5f, borderColor);

    // Subtle pointer arrow connecting bubble to anchor position
    Vector2 p1 = { pos.x - 12, bubbleRect.y + bubbleRect.height };
    Vector2 p2 = { pos.x + 12, bubbleRect.y + bubbleRect.height };
    Vector2 p3 = { pos.x, pos.y - 40.0f };
    DrawTriangle(p1, p3, p2, Color{ 18, 22, 36, 215 });

    // Render dialogue text
    float lineY = bubbleRect.y + padding;
    for (const auto& line : lines) {
        float textX = bubbleRect.x + (bubbleRect.width - MeasureText(line.c_str(), fontSize)) / 2.0f;
        DrawText(line.c_str(), (int)textX, (int)lineY, fontSize, Color{ 245, 247, 250, 255 });
        lineY += fontSize + 6;
    }
}

// Function to safely load state from JSON
bool LoadStateFromJson(const std::string& filepath, DashboardState& state, std::string& currentPhotoPath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;
        
        json j;
        file >> j;
        
        state.timeStr = j.value("timeStr", "12:00 PM");
        state.weatherStr = j.value("weatherStr", "70°F Clear");
        state.activeSpeaker = j.value("activeSpeaker", "Frank");
        state.targetFriend = j.value("targetFriend", "Sam");
        state.emotion = j.value("emotion", "neutral");
        state.action = j.value("action", "speaking");
        state.dialogueText = j.value("dialogueText", "Hello world!");

        std::string newPhotoPath = j.value("photoPath", "");
        if (newPhotoPath != currentPhotoPath && !newPhotoPath.empty()) {
            if (FileExists(newPhotoPath.c_str())) {
                UnloadTexture(state.currentPhoto);
                state.currentPhoto = LoadTexture(newPhotoPath.c_str());
                currentPhotoPath = newPhotoPath;
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "JSON Load Exception: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    // Target Canvas Resolution: 1920x1080
    const int GAME_WIDTH = 1920;
    const int GAME_HEIGHT = 1080;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Spatial TV Dashboard - Ambient Lounge");
    SetTargetFPS(60);

    // Virtual Viewport render target
    RenderTexture2D target = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    // 2. Initialize state machine struct
    DashboardState state;
    state.timeStr = "03:28 PM";
    state.weatherStr = "72°F Sunset Glow";
    state.activeSpeaker = "Frank";
    state.targetFriend = "Sam";
    state.emotion = "excited";
    state.action = "sharing photo";
    state.dialogueText = "Hey everyone! Check out this sunset shot from the hike yesterday! Isn't it amazing?";
    state.currentPhoto = CreateFallbackPhotoTexture();

    std::string loadedPhotoPath = "";

    // 3. Define 4 fixed spatial anchor positions for friend group surrounding center photo
    std::vector<FriendAnchor> friendAnchors = {
        { "Frank", Vector2{ 960, 160 },  Color{ 255, 107, 107, 255 } }, // TOP
        { "Sam",   Vector2{ 960, 920 },  Color{ 78, 205, 196, 255 } },  // BOTTOM
        { "Sky",   Vector2{ 260, 540 },  Color{ 69, 183, 209, 255 } },  // LEFT
        { "Milo",  Vector2{ 1660, 540 }, Color{ 255, 160, 122, 255 } }  // RIGHT
    };

    // Center photo anchor
    Vector2 centerAnchor = { 960, 540 };

    // Ambient background particles
    std::vector<AmbientParticle> particles;
    for (int i = 0; i < 70; ++i) {
        particles.push_back({
            Vector2{ (float)GetRandomValue(0, GAME_WIDTH), (float)GetRandomValue(0, GAME_HEIGHT) },
            Vector2{ (float)GetRandomValue(-20, 20) / 100.0f, (float)GetRandomValue(-30, -5) / 100.0f },
            (float)GetRandomValue(20, 80) / 100.0f,
            (float)GetRandomValue(2, 6),
            (float)GetRandomValue(1, 4) / 10.0f
        });
    }

    // JSON reload timing
    float jsonPollTimer = 0.0f;
    const std::string jsonPath = "state.json";
    LoadStateFromJson(jsonPath, state, loadedPhotoPath);

    float globalTime = 0.0f;

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        globalTime += deltaTime;
        jsonPollTimer += deltaTime;

        // Poll state.json every 0.5 seconds
        if (jsonPollTimer >= 0.5f) {
            jsonPollTimer = 0.0f;
            LoadStateFromJson(jsonPath, state, loadedPhotoPath);
        }

        // Update ambient particles
        for (auto& p : particles) {
            p.pos.x += p.vel.x;
            p.pos.y += p.vel.y;
            if (p.pos.y < 0) p.pos.y = GAME_HEIGHT;
            if (p.pos.x < 0) p.pos.x = GAME_WIDTH;
            if (p.pos.x > GAME_WIDTH) p.pos.x = 0;
            p.alpha = 0.3f + 0.2f * sinf(globalTime * p.pulseSpeed);
        }

        // Render to virtual viewport 1920x1080
        BeginTextureMode(target);
            // Deep spatial dark gradient background
            ClearBackground(Color{ 10, 14, 26, 255 });

            // Ambient background glow circles
            DrawCircleGradient(960, 540, 700, Color{ 25, 35, 65, 180 }, Color{ 10, 14, 26, 0 });
            DrawCircleGradient(960, 160, 300, Color{ 50, 30, 80, 80 }, Color{ 10, 14, 26, 0 });
            DrawCircleGradient(960, 920, 300, Color{ 20, 60, 70, 80 }, Color{ 10, 14, 26, 0 });

            // Render floating starfield particles
            for (const auto& p : particles) {
                DrawCircleV(p.pos, p.size, ColorAlpha(Color{ 180, 200, 255, 255 }, p.alpha));
            }

            // Top Header Bar (Time & Weather glassmorphic panel)
            Rectangle headerRect = { 760, 30, 400, 50 };
            DrawRectangleRounded(headerRect, 0.4f, 6, Color{ 18, 24, 42, 200 });
            DrawRectangleRoundedLines(headerRect, 0.4f, 6, 1.5f, Color{ 255, 255, 255, 40 });
            
            std::string headerText = state.timeStr + "  |  " + state.weatherStr;
            int headerWidth = MeasureText(headerText.c_str(), 18);
            DrawText(headerText.c_str(), 960 - headerWidth / 2, 45, 18, Color{ 220, 230, 245, 255 });

            // Find Active Speaker & Target Friend Anchors
            Vector2 speakerPos = { 0, 0 };
            Vector2 targetPos = { 0, 0 };
            Color activeColor = WHITE;

            for (const auto& friendAnchor : friendAnchors) {
                if (friendAnchor.name == state.activeSpeaker) {
                    speakerPos = friendAnchor.position;
                    activeColor = friendAnchor.color;
                }
                if (friendAnchor.name == state.targetFriend) {
                    targetPos = friendAnchor.position;
                }
            }

            // Draw connecting spatial energy beam between Active Speaker and Target/Center
            if (speakerPos.x != 0 && targetPos.x != 0 && speakerPos.x != targetPos.x) {
                // Pulsing arc line
                float pulse = (sinf(globalTime * 6.0f) + 1.0f) * 0.5f;
                DrawLineEx(speakerPos, targetPos, 3.0f + pulse * 3.0f, ColorAlpha(activeColor, 0.4f + pulse * 0.3f));
                
                // Animated energy bead
                float progress = fmodf(globalTime * 0.5f, 1.0f);
                Vector2 beadPos = {
                    speakerPos.x + (targetPos.x - speakerPos.x) * progress,
                    speakerPos.y + (targetPos.y - speakerPos.y) * progress
                };
                DrawCircleV(beadPos, 8.0f, activeColor);
                DrawCircleV(beadPos, 14.0f, ColorAlpha(activeColor, 0.4f));
            }

            // Render Center Spatial Photo Frame
            float photoWidth = 560.0f;
            float photoHeight = 380.0f;
            Rectangle photoBounds = { centerAnchor.x - photoWidth/2.0f, centerAnchor.y - photoHeight/2.0f, photoWidth, photoHeight };

            // Photo frame drop shadow & glow
            DrawRectangleRounded(Rectangle{ photoBounds.x + 8, photoBounds.y + 8, photoBounds.width, photoBounds.height }, 0.08f, 8, Color{ 0, 0, 0, 120 });
            DrawRectangleRounded(photoBounds, 0.08f, 8, Color{ 20, 25, 40, 255 });

            // Draw Photo Texture
            if (state.currentPhoto.id > 0) {
                Rectangle src = { 0, 0, (float)state.currentPhoto.width, (float)state.currentPhoto.height };
                Rectangle dest = { photoBounds.x + 10, photoBounds.y + 10, photoBounds.width - 20, photoBounds.height - 20 };
                DrawTexturePro(state.currentPhoto, src, dest, Vector2{0, 0}, 0.0f, WHITE);
            }

            // Glassmorphic Photo Frame Border & Action Badge
            DrawRectangleRoundedLines(photoBounds, 0.08f, 8, 3.0f, ColorAlpha(activeColor, 0.7f));

            // Emotion & Action Badge at bottom of photo
            std::string actionBadge = "[" + state.emotion + "] " + state.action;
            Rectangle badgeRect = { centerAnchor.x - 140, photoBounds.y + photoBounds.height - 35, 280, 30 };
            DrawRectangleRounded(badgeRect, 0.5f, 4, Color{ 15, 20, 35, 220 });
            DrawRectangleRoundedLines(badgeRect, 0.5f, 4, 1.5f, ColorAlpha(activeColor, 0.6f));
            int badgeTextW = MeasureText(actionBadge.c_str(), 14);
            DrawText(actionBadge.c_str(), (int)(centerAnchor.x - badgeTextW / 2.0f), (int)(badgeRect.y + 7), 14, Color{ 240, 245, 255, 255 });

            // Render 4 Spatial Friend Anchors
            for (const auto& friendAnchor : friendAnchors) {
                bool isActive = (friendAnchor.name == state.activeSpeaker);
                bool isTarget = (friendAnchor.name == state.targetFriend);

                // Avatar Outer Ring & Glow
                float baseRadius = 42.0f;
                if (isActive) {
                    float pulseRadius = baseRadius + sinf(globalTime * 5.0f) * 5.0f;
                    DrawCircleV(friendAnchor.position, pulseRadius + 8.0f, ColorAlpha(friendAnchor.color, 0.35f));
                    DrawCircleV(friendAnchor.position, pulseRadius, friendAnchor.color);
                } else if (isTarget) {
                    DrawCircleV(friendAnchor.position, baseRadius + 4.0f, ColorAlpha(WHITE, 0.5f));
                    DrawCircleV(friendAnchor.position, baseRadius, friendAnchor.color);
                } else {
                    DrawCircleV(friendAnchor.position, baseRadius, ColorAlpha(friendAnchor.color, 0.7f));
                }

                // Avatar Inner Fill & Initials
                DrawCircleV(friendAnchor.position, 34.0f, Color{ 22, 28, 45, 255 });
                std::string initial = friendAnchor.name.substr(0, 1);
                int initW = MeasureText(initial.c_str(), 26);
                DrawText(initial.c_str(), (int)(friendAnchor.position.x - initW / 2.0f), (int)(friendAnchor.position.y - 13.0f), 26, friendAnchor.color);

                // Name Tag Below Avatar
                int nameW = MeasureText(friendAnchor.name.c_str(), 18);
                Rectangle nameTagRect = { friendAnchor.position.x - (nameW / 2.0f) - 10, friendAnchor.position.y + 48, (float)nameW + 20, 26 };
                DrawRectangleRounded(nameTagRect, 0.4f, 4, Color{ 18, 24, 40, 200 });
                DrawText(friendAnchor.name.c_str(), (int)(friendAnchor.position.x - nameW / 2.0f), (int)(nameTagRect.y + 4), 18, WHITE);

                // 4. Render Speech Bubble if this friend anchor is the Active Speaker
                if (isActive && !state.dialogueText.empty()) {
                    DrawSpeechBubble(friendAnchor.position, state.dialogueText, friendAnchor.color);
                }
            }

            // Controls Hint overlay at bottom right
            DrawText("Live polling state.json | Press ESC to exit", 20, GAME_HEIGHT - 35, 16, Color{ 140, 160, 190, 180 });

        EndTextureMode();

        // Render virtual target to current window preserving 1920x1080 aspect ratio
        BeginDrawing();
            ClearBackground(BLACK);

            float windowW = (float)GetScreenWidth();
            float windowH = (float)GetScreenHeight();

            float scale = fminf(windowW / (float)GAME_WIDTH, windowH / (float)GAME_HEIGHT);
            
            Rectangle srcRect = { 0, 0, (float)GAME_WIDTH, (float)-GAME_HEIGHT }; // Y flipped for render target
            Rectangle destRect = {
                (windowW - ((float)GAME_WIDTH * scale)) * 0.5f,
                (windowH - ((float)GAME_HEIGHT * scale)) * 0.5f,
                (float)GAME_WIDTH * scale,
                (float)GAME_HEIGHT * scale
            };

            DrawTexturePro(target.texture, srcRect, destRect, Vector2{ 0, 0 }, 0.0f, WHITE);
        EndDrawing();
    }

    // Cleanup resources
    UnloadTexture(state.currentPhoto);
    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
