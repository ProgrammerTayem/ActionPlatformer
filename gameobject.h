#pragma once

#include "animation.h"
#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include <vector>

enum class ObjectType{
    player, level, enemy, bullet
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

struct PlayerData{
    float HP, HPmax;
    PlayerState state;
    Timer WeaponTimer;
    PlayerData() : HP(250.0f), HPmax(250.0), state(PlayerState::idle), WeaponTimer(0.1f) {}
};

struct BulletData{
    BulletState state;
    BulletData() : state(BulletState::moving) {}
};

struct EnemyData{
    float HP, HPmax;
    Timer dmgDuration, hitRate;
    enemyState state;
    EnemyData() : HP(100.0f), HPmax(100.0f), dmgDuration(0.5f), state(enemyState::shambling), hitRate(0.7f) {
        hitRate.step(0.7f);
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
    bool dynamic, grounded, flashes;
    
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
    }
};