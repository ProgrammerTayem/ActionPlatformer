#pragma once

#include "animation.h"
#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include <vector>

class UIStack;

enum class ObjectType{
    player, level, enemy, bullet
};

enum class ObjectTarget{
    playable, background, foreground, destructible
};

enum class PlayerState{
    idle, running, jumping
};

enum class BulletState{
    idle, moving, colliding
};

enum class enemyState{
    shambling, damaged, dead
};

enum class buttonState{
    active, inactive, hidden
};

struct PlayerData{
    float HP, HPmax;
    PlayerState state;
    Timer WeaponTimer;
    PlayerData() : HP(200.0f), HPmax(200.0), state(PlayerState::idle), WeaponTimer(0.15f) {}
};

struct BulletData{
    BulletState state;
    BulletData() : state(BulletState::moving) {}
};

struct EnemyData{
    float HP, HPmax;
    Timer dmgDuration, contactRate;
    enemyState state;
    EnemyData() : HP(200.0f), HPmax(200.0f), dmgDuration(0.5f), contactRate(0.25f), state(enemyState::shambling) {
        contactRate.prime();
    }
};
struct LevelData{};

union ObjectData{
    PlayerData player;
    EnemyData enemy;
    LevelData level;
    BulletData bullet;
};

struct GameObject{
    ObjectType type;
    ObjectData data;
    glm::vec2 pos, vel, acc;
    std::vector<Animation> animations;
    SDL_Texture *texture;
    SDL_FRect hitbox;
    Timer flashTimer;
    int curAnimation, spriteFrame;
    float dir;
    float maxSpeedX;
    bool dynamic, grounded, flashes, allowsGoingThrough;
    ObjectTarget Tar;
    
    GameObject(): data{.level = LevelData()}, flashTimer(0.05)
    {
        type = ObjectType::level;
        pos = vel = acc = glm::vec2(0);
        curAnimation = -1;
        spriteFrame = 1;
        dir = 1;
        maxSpeedX = 0;
        texture = nullptr;
        dynamic = false;
        grounded = false;
        hitbox = {0};
        flashes = false;
        Tar = ObjectTarget::playable;
    }
};

struct Button{
    glm::vec2 pos, txt_dim;
    float h, w;
    const char *s;
    SDL_Color border, text;
    glm::vec3 box;
    SDL_Texture *texture, *txtTexture;
    std::function<void(Button &)> onClick;
    std::vector<buttonState> StateChanges;
    bool pressed, hovered;
    buttonState state;
    Button(){
        pos = glm::vec2(0, 0);
        h = 25.0f, w = 150.f;
        text = SDL_Color{0, 0, 0, 255};
        border = text;
        box = glm::vec3{255.0, 255.0, 255.0};
        texture = nullptr;
        pressed = hovered = false;
        state = buttonState::hidden;
        s = NULL;
    }
};

struct Slider{
    SDL_FRect knob, track;
    bool dragging;
    float val;
    Slider(){
        knob = track = {0};
        dragging = false;
        val = 1.0f;
    }
    Slider(float x, float y, float w, float h, float knobW, float knobH, float initialVal = 1.0f, bool drg = false){
        track = {x, y, w, h};
        knob = {x + w - knobW / 2, y + h /2 - knobH / 2, knobW, knobH};
        val = initialVal;
        dragging = drg;
    }
};