#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <vector>
#include <string>
#include <array>
#include <format>
#include "gameobject.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
//#include <glm/glm.hpp>

struct SDLState{
    SDL_Window *window;
    SDL_Renderer *renderer;
    int w, h, logW, logH;
    const bool *keys;
    ma_engine engine;
    
    SDLState() : keys(SDL_GetKeyboardState(nullptr)) {}
};

enum class currentInterface{
    MENU, GAME
};
currentInterface T = currentInterface::MENU;

struct Resource{
    const int PLAYER_IDLE_ANIMATION = 0;
    const int PLAYER_RUNNING_ANIMATION = 1;
    const int PLAYER_SLIDING_ANIMATION = 2;
    const int PLAYER_SHOOTNG_ANIMATION = 3;
    const int PLAYER_SLIDE_SHOOTING_ANIMATION = 4;

    const int BULLET_HIT_ANIMATION = 0;
    const int BULLET_MOVING_ANIMATION = 1;

    const int ENEMY_ANIMATION = 0;
    const int ENEMY_DAMAGED_ANIMATION = 1;
    const int ENEMY_DYING_ANIMATION = 2;
    std::vector<Animation> animationsPlayer, animationsBullet, animationsEnemy;
    std::vector<SDL_Texture*> textures;
    SDL_Texture* idleTex, *runTex, *groundTex, *panelTex, *enemyTex, *grassTex, *brickTex, *slideTex, *bckgrnd1Tex, *bckgrnd2Tex, 
                *bckgrnd3Tex, *bckgrnd4Tex, *bulletTex, *bulletHitTex, *shootTex, *runShootTex, *slideShootTex, *enemyHitTex,
                *enemyDieTex;

    SDL_Texture* getTex(const std::string &path, SDL_Renderer *renderer){
        SDL_Texture *tex = IMG_LoadTexture(renderer, path.c_str());
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        textures.push_back(tex);
        return tex;
    }

    void load(SDLState &state){
        animationsPlayer.resize(5);
        animationsPlayer[PLAYER_IDLE_ANIMATION] = Animation(8, 1.6f);
        animationsPlayer[PLAYER_RUNNING_ANIMATION] = Animation(4, 0.5f);
        animationsPlayer[PLAYER_SLIDING_ANIMATION] = Animation(1, 1.0f);
        animationsPlayer[PLAYER_SHOOTNG_ANIMATION] = Animation(4, 0.5f);
        animationsPlayer[PLAYER_SLIDE_SHOOTING_ANIMATION] = Animation(4, 0.5f);
        animationsBullet.resize(2);
        animationsBullet[BULLET_MOVING_ANIMATION] = Animation(4, 0.05f);
        animationsBullet[BULLET_HIT_ANIMATION] = Animation(4, 0.15f);
        animationsEnemy.resize(3);
        animationsEnemy[ENEMY_ANIMATION] = Animation(8, 1.0f);
        animationsEnemy[ENEMY_DAMAGED_ANIMATION] = Animation(8, 1.0f);
        animationsEnemy[ENEMY_DYING_ANIMATION] = Animation(18, 2.0f);
        idleTex = getTex("resources/idle.png", state.renderer);
        runTex = getTex("resources/run.png", state.renderer);
        slideTex = getTex("resources/slide.png", state.renderer);
        panelTex = getTex("resources/tiles/panel.png", state.renderer);
        groundTex = getTex("resources/tiles/ground.png", state.renderer);
        //enemyTex = getTex("resources/enemy.png", state.renderer);
        grassTex = getTex("resources/tiles/grass.png", state.renderer);
        brickTex = getTex("resources/tiles/brick.png", state.renderer);
        bckgrnd1Tex = getTex("resources/bckgrnd/bg_layer1.png", state.renderer);
        bckgrnd2Tex = getTex("resources/bckgrnd/bg_layer2.png", state.renderer);
        bckgrnd3Tex = getTex("resources/bckgrnd/bg_layer3.png", state.renderer);
        bckgrnd4Tex = getTex("resources/bckgrnd/bg_layer4.png", state.renderer);
        bulletTex = getTex("resources/bullet.png", state.renderer);
        bulletHitTex = getTex("resources/bullet_hit.png", state.renderer);
        shootTex = getTex("resources/shoot.png", state.renderer);
        runShootTex = getTex("resources/shoot_run.png", state.renderer);
        slideShootTex = getTex("resources/slide_shoot.png", state.renderer);
        enemyTex = getTex("resources/enemy.png", state.renderer);
        enemyHitTex = getTex("resources/enemy_hit.png", state.renderer);
        enemyDieTex = getTex("resources/enemy_die.png", state.renderer);
    }

    void unload(){
        for(SDL_Texture* tex : textures){
            SDL_DestroyTexture(tex);
        }
    }
};

const int LAYER_LEVEL_IDX = 0;
const int LAYER_CHARACTER_IDX = 1;

struct GameState{
    std::array<std::vector<GameObject>, 2>layers;
    std::vector<GameObject> BackgroundTile;
    std::vector<GameObject> ForegroundTile;
    std::vector<GameObject> Bullets;
    SDL_FRect MapViewport;
    int playerIdx;
    float bg2scroll, bg3scroll, bg4scroll;
    bool debugMode;
    GameState(const SDLState &state) : playerIdx(-1) {
        MapViewport = SDL_FRect{
            .x = 0,
            .y = 0,
            .w = static_cast<float>(state.logW),
            .h = static_cast<float>(state.logH)
        };
        bg2scroll = bg3scroll = bg4scroll = 0.0f;
        debugMode = false;
    }
    GameObject &getPlayer(){
        return layers[LAYER_CHARACTER_IDX][playerIdx];
    }
};

const int MAX_ROWS = 5;
const int MAX_COLS = 50;
const int TILE_SIZE = 32;
const int HP_BAR_WIDTH = 150;
const int HP_BAR_HEIGHT = 15;

void cleanup(SDLState &state);
bool init(SDLState &state);
void DrawObj(const SDLState &state, GameState &gs, GameObject &obj, float width, float height, float timeDelta);
void update(const SDLState &state, GameState &gs,GameObject &obj, Resource &res, float timeDelta, ma_engine engine);
void CollisionDetection(const SDLState &state, GameState &gs, GameObject &a, GameObject &b, float timeDelta, Resource &res);
void CollisionResponse(const SDLState &state, Resource &res, GameState &gs, GameObject &a, GameObject &b, const SDL_FRect &recA, const SDL_FRect &recB, const SDL_FRect &intersect, float timeDelta, ma_engine engine);
void createTiles(const SDLState &state, GameState &gs, Resource &res);
void HandleKey(const SDLState &state, GameState &gs, GameObject &obj, SDL_Scancode key, bool pressed);
void DrawParallaxBackground(SDL_Renderer *renderer, SDL_Texture *tex, float xVel, float &scrollPos, float scrollFact, float timeDelta);

int main(int argc, char* argv[]){
    float mx, my;
    SDLState state;
    SDL_FRect playButton;
    Resource res;
    state.w = 1600;
    state.h = 900;
    state.logW = 640;
    state.logH = 320;
    if(init(state) == false) return 1;
    ma_sound music;
    ma_sound_init_from_file(&state.engine, "resources/sound/Juhani Junkala.mp3", MA_SOUND_FLAG_LOOPING, NULL, NULL, &music);
    ma_sound_set_volume(&music, 0.3f);
    ma_sound_start(&music);
    GameState gs(state);
    res.load(state);
    restart:
    if(T == currentInterface::GAME){
        createTiles(state, gs, res);
    }
    else{
        playButton = {static_cast<float>(state.logW/2-75), static_cast<float>(state.logH/2-15), 150, 30};
    }

    uint64_t timeP = SDL_GetTicks();

    bool running = true;
    while(running){
        uint64_t timeC = SDL_GetTicks();
        float timeDelta = (timeC - timeP) / 1000.0f;

        SDL_Event event{0};
        while(SDL_PollEvent(&event)){
            if(T == currentInterface::MENU){
                switch(event.type){
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    {
                        SDL_ConvertEventToRenderCoordinates(state.renderer, &event);
                        mx = event.button.x;
                        my = event.button.y;

                        if (mx >= playButton.x && mx <= playButton.x + playButton.w && my >= playButton.y && my <= playButton.y + playButton.h) {
                            T = currentInterface::GAME;
                            //cleanup(state);
                            goto restart;
                        }
                    }
                    break;
                    case SDL_EVENT_QUIT:
                        running = false;
                        break;
                    default:
                        break;
                }
            }
            else{
                switch(event.type){
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    state.w = event.window.data1;
                    state.h = event.window.data2;
                case SDL_EVENT_KEY_DOWN:
                     HandleKey(state, gs, gs.getPlayer(), event.key.scancode, true);
                     break;
                case SDL_EVENT_KEY_UP:
                     HandleKey(state, gs, gs.getPlayer(), event.key.scancode, false);
                     if(event.key.scancode == SDL_SCANCODE_F10) gs.debugMode = !gs.debugMode;
                     break;
                default:
                    break;
                }
            }
        }

        if(T == currentInterface::MENU){
            SDL_RenderTexture(state.renderer, res.bckgrnd1Tex, nullptr, nullptr);
            SDL_RenderTexture(state.renderer, res.bckgrnd2Tex, nullptr, nullptr);
            SDL_RenderTexture(state.renderer, res.bckgrnd3Tex, nullptr, nullptr);
            SDL_RenderTexture(state.renderer, res.bckgrnd4Tex, nullptr, nullptr);
            SDL_FRect brdr = {playButton.x-1, playButton.y-1, playButton.w+2, playButton.h+2};
            SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(state.renderer, &playButton);
            SDL_SetRenderDrawColor(state.renderer, 255, 50, 50, 255);
            SDL_RenderRect(state.renderer, &brdr);
            SDL_RenderDebugText(state.renderer, playButton.x+playButton.w/2-15, playButton.y+playButton.h/2-4, "Play");
            //char mouse[20];
            //SDL_snprintf(mouse, 20, "X: %f Y: %f", mx, my);
           // SDL_RenderDebugText(state.renderer, 5, 5, mouse);
            SDL_RenderPresent(state.renderer);
        }

        if(T == currentInterface::GAME){
            for(auto &layer : gs.layers){
                for(GameObject &obj : layer){
                    update(state, gs, obj, res, timeDelta, state.engine);
                }
            }

            for(GameObject &bullet : gs.Bullets){
                update(state, gs, bullet, res, timeDelta, state.engine);
            }

            gs.MapViewport.x = gs.getPlayer().pos.x + TILE_SIZE / 2 - state.logW / 2;

            SDL_SetRenderDrawColor(state.renderer, 20, 10, 30, 255);
            SDL_RenderClear(state.renderer);

            SDL_RenderTexture(state.renderer, res.bckgrnd1Tex, nullptr, nullptr);
            DrawParallaxBackground(state.renderer, res.bckgrnd4Tex, gs.getPlayer().vel.x, gs.bg4scroll, 0.075f, timeDelta);
            DrawParallaxBackground(state.renderer, res.bckgrnd3Tex, gs.getPlayer().vel.x, gs.bg3scroll, 0.15f, timeDelta);
            DrawParallaxBackground(state.renderer, res.bckgrnd2Tex, gs.getPlayer().vel.x, gs.bg2scroll, 0.3f, timeDelta);
            if(gs.debugMode){
                SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 255);
                char stateText[64];
                int idle_bullets = 0;
                float Bx = 0.0, MVx = 0.0;
                if(gs.Bullets.size()){
                    if(gs.Bullets[0].data.bullet.state == BulletState::idle){
                        idle_bullets++;
                        Bx  = gs.Bullets[0].pos.x;
                        MVx = gs.MapViewport.x;
                    }
                }
                SDL_snprintf(stateText, sizeof(stateText), "S: %d B: %d Grnd: %d IB: %d Bx: %f MVx: %f", static_cast<int>(gs.getPlayer().data.player.state), gs.Bullets.size(), gs.getPlayer().grounded, idle_bullets, Bx, MVx);
                

                SDL_RenderDebugText(state.renderer, 5, 5, stateText);
            }

            for(auto &obj : gs.BackgroundTile){
                SDL_FRect to{
                    .x = obj.pos.x - gs.MapViewport.x,
                    .y = obj.pos.y,
                    .w = static_cast<float>(obj.texture->w),
                    .h = static_cast<float>(obj.texture->h)
                };
                SDL_RenderTexture(state.renderer, obj.texture, nullptr, &to);
            }

            for(auto &layer : gs.layers){
                for(GameObject &obj : layer){
                    DrawObj(state, gs, obj, TILE_SIZE, TILE_SIZE, timeDelta);
                }
            }

            for(GameObject &gb : gs.Bullets){
                if(gb.data.bullet.state != BulletState::idle) DrawObj(state, gs, gb, gb.hitbox.w, gb.hitbox.h, timeDelta);
            }

            for(auto &obj : gs.ForegroundTile){
                SDL_FRect to{
                    .x = obj.pos.x - gs.MapViewport.x,
                    .y = obj.pos.y,
                    .w = static_cast<float>(obj.texture->w),
                    .h = static_cast<float>(obj.texture->h)
                };
                SDL_RenderTexture(state.renderer, obj.texture, nullptr, &to);
            }

            float percHP = gs.getPlayer().data.player.HP / gs.getPlayer().data.player.HPmax;
            percHP = glm::clamp(percHP, 0.0f, 1.0f);

            char hpText[64];
            SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 255);
            SDL_snprintf(hpText, sizeof(hpText), "HP: %.0f / %.0f", gs.getPlayer().data.player.HP, gs.getPlayer().data.player.HPmax);
            SDL_RenderDebugText(state.renderer, state.logW-200, 15, hpText);
            SDL_FRect bg = {static_cast<float>(state.logW - 200), 25.0f, HP_BAR_WIDTH, HP_BAR_HEIGHT}, 
            fg = {static_cast<float>(state.logW - 200), 25.0f, percHP*150, HP_BAR_HEIGHT},
            brdr = {bg.x-1, bg.y-1, bg.w+2, bg.h+2};
            SDL_SetRenderDrawColor(state.renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(state.renderer, &bg);
            SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
            SDL_RenderRect(state.renderer, &brdr);
            SDL_SetRenderDrawColor(state.renderer, 255 * (1-percHP), 255 * percHP, 0, 255);
            SDL_RenderFillRect(state.renderer, &fg);
            if(gs.getPlayer().data.player.state == PlayerState::jumping && gs.getPlayer().grounded){
                gs.getPlayer().data.player.state = PlayerState::idle;
            }
            SDL_RenderPresent(state.renderer);
        }
        timeP = timeC;

    }
    res.unload();
    cleanup(state);
    return 0;
}

void cleanup(SDLState &state){
    SDL_DestroyWindow(state.window);
    SDL_DestroyRenderer(state.renderer);
    ma_engine_uninit(&state.engine);
    SDL_Quit();
}

bool init(SDLState &state){
    bool success = true;
    if(!SDL_Init(SDL_INIT_VIDEO)){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error Initializing SDL3", nullptr);
        success = false;
    }
    state.window = SDL_CreateWindow("Game", state.w, state.h, SDL_WINDOW_FULLSCREEN);
    if(!state.window){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error Creating Window", nullptr);
        cleanup(state);
        success = false;
    }
    state.renderer = SDL_CreateRenderer(state.window, nullptr);
    if(!state.renderer){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error Creating Renderer", nullptr);
        cleanup(state);
        success = false;
    }
    if(ma_engine_init(NULL, &state.engine) != MA_SUCCESS){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initializing audio", nullptr);
        cleanup(state);
        success = false;
    }
    SDL_SetRenderVSync(state.renderer, 1);
    SDL_SetRenderLogicalPresentation(state.renderer, state.logW, state.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    return success;
}

void DrawObj(const SDLState &state, GameState &gs, GameObject &obj, float width, float height, float timeDelta){
    float srcX = (obj.curAnimation != -1) ? obj.animations[obj.curAnimation].curFrame() * width : (obj.spriteFrame - 1) * width;
    SDL_FRect from{
        .x = srcX, .y = 0, .w = width, .h = height
    };
    SDL_FRect to{
        .x = obj.pos.x - gs.MapViewport.x, .y = obj.pos.y, .w = width, .h = height
    };
    SDL_FlipMode flipH = (obj.dir == -1) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    if(!obj.flashes){
        SDL_RenderTextureRotated(state.renderer, obj.texture, &from, &to, 0.0f, nullptr, flipH);
    }
    else{
        SDL_SetTextureColorModFloat(obj.texture, 2.5f, 1.0f, 1.0f);
        SDL_RenderTextureRotated(state.renderer, obj.texture, &from, &to, 0.0f, nullptr, flipH);
        SDL_SetTextureColorModFloat(obj.texture, 1.0f, 1.0f, 1.0f);
        if(obj.flashTimer.step(timeDelta)){
            obj.flashes = false;
        }

    }
        if(gs.debugMode){
        SDL_FRect rectA{
        .x = obj.pos.x + obj.hitbox.x - gs.MapViewport.x,
        .y = obj.pos.y + obj.hitbox.y,
        .w = obj.hitbox.w,
        .h = obj.hitbox.h
        };
        SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 150);
        SDL_RenderFillRect(state.renderer, &rectA);
        //SDL_RenderFillRect(state.renderer, &gs.MapViewport);
        SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);
    }
}

void update(const SDLState &state, GameState &gs,GameObject &obj, Resource &res, float timeDelta, ma_engine engine){
    if(obj.curAnimation != -1) obj.animations[obj.curAnimation].step(timeDelta);
    if(obj.dynamic && !obj.grounded) obj.vel += glm::vec2(0, 400) * timeDelta; // gravity
    float curDir = 0;
    if(obj.type == ObjectType::player){
        if(state.keys[SDL_SCANCODE_A]){
            curDir += -1;
        }
        if(state.keys[SDL_SCANCODE_D]){
            curDir += 1;
        }
        if(state.keys[SDL_SCANCODE_ESCAPE]) exit(EXIT_SUCCESS);
        Timer &weaponTimer = obj.data.player.WeaponTimer;
        weaponTimer.step(timeDelta);
        const auto handleShooting = [&state, &gs, &res, &obj, &weaponTimer, &engine](SDL_Texture *tex, SDL_Texture *shootTex, int AnimIndex, int ShootAnimIndex){
            if(state.keys[SDL_SCANCODE_RCTRL]){
                obj.texture = shootTex;
                obj.curAnimation = ShootAnimIndex;
                if(weaponTimer.isTmOut()){
                    weaponTimer.reset();
                    GameObject bullet;
                    bullet.data.bullet = BulletData();
                    const int yVar = 50;
                    const float yVel = SDL_rand(yVar) - yVar/2.0f;
                    bullet.vel = glm::vec2((obj.vel.x + 600.0f) * obj.dir , yVel);
                    bullet.type = ObjectType::bullet;
                    bullet.dir = gs.getPlayer().dir;
                    bullet.texture = res.bulletTex;
                    bullet.maxSpeedX = 1000.0f;
                    bullet.curAnimation = res.BULLET_MOVING_ANIMATION;
                    bullet.hitbox = SDL_FRect{
                        .x = 0,
                        .y = 0,
                        .w = static_cast<float>(res.bulletTex->h),
                        .h = static_cast<float>(res.bulletTex->h),
                    };
                    bullet.animations = res.animationsBullet;
                    // Using LERP, adjust bullet position
                    const int left = 4, right = 32 - 4 - 4;
                    const float t = (obj.dir + 1)/2.00f;
                    const int xOffset = left + right * t;
                    bullet.pos = glm::vec2{
                        obj.pos.x + xOffset,
                        obj.pos.y + TILE_SIZE / 2 + 1
                    };
                    bool foundIdle = false;
                    for(int i = 0; i < gs.Bullets.size() && !foundIdle; i++){
                        if(gs.Bullets[i].data.bullet.state == BulletState::idle){
                            foundIdle = true;
                            gs.Bullets[i] = bullet;
                        }
                    }
                    if(!foundIdle) gs.Bullets.push_back(bullet);
                    ma_engine_play_sound(&engine, "resources/sound/shoot.wav", NULL);
                }
            }
            else{
                obj.texture = tex;
                obj.curAnimation = AnimIndex;
            }
        };
        switch(obj.data.player.state){
            case PlayerState::idle:
            {
                if(curDir){
                    obj.data.player.state = PlayerState::running;
                }
                else{
                    if(obj.vel.x){
                        const float dec_factor = (obj.vel.x > 0) ? -1.2f : 1.2f;
                        float amt = dec_factor * obj.acc.x * timeDelta;
                        if(std::abs(amt) > std::abs(obj.vel.x)){
                            obj.vel.x = 0;
                        }
                        else{
                            obj.vel.x += amt;
                        }
                    }
                }
                handleShooting(res.idleTex, res.shootTex, res.PLAYER_IDLE_ANIMATION, res.PLAYER_SHOOTNG_ANIMATION);
                break;
            }
            case PlayerState::running:
            {
                if(!curDir){
                    obj.data.player.state = PlayerState::idle;
                }
                if(obj.vel.x * obj.dir < 0 && obj.grounded){
                    handleShooting(res.slideTex, res.slideShootTex, res.PLAYER_SLIDING_ANIMATION, res.PLAYER_SLIDE_SHOOTING_ANIMATION);
                }
                
                else{
                    handleShooting(res.runTex, res.runShootTex, res.PLAYER_RUNNING_ANIMATION, res.PLAYER_RUNNING_ANIMATION);
                }
                break;
            }
            case PlayerState::jumping:
            {
                handleShooting(res.runTex, res.runShootTex, res.PLAYER_RUNNING_ANIMATION, res.PLAYER_RUNNING_ANIMATION);
                break;
            }
        }
    }
    else if(obj.type == ObjectType::bullet){
        switch(obj.data.bullet.state){
            case BulletState::moving:
            {
                if(obj.pos.x - gs.MapViewport.x < 0 || obj.pos.x - gs.MapViewport.x > state.logW || 
                    obj.pos.y - gs.MapViewport.y < 0 || obj.pos.y - gs.MapViewport.y > state.logH){
                    obj.data.bullet.state = BulletState::idle;
                }
            }
            break;
            case BulletState::colliding:
            {
                if(obj.animations[obj.curAnimation].done()) obj.data.bullet.state = BulletState::idle;
            }
            break;
        }
    }
    else if(obj.type == ObjectType::enemy){
        switch(obj.data.enemy.state){
            case enemyState::shambling:
            {
                glm::vec2 playerDir = gs.getPlayer().pos - obj.pos;
                if(glm::length(playerDir) < 100){
                    curDir = playerDir.x < 0 ? -1 : 1;
                    obj.acc = glm::vec2(30, 0);
                }
                else{
                    obj.acc = glm::vec2(0);
                    obj.vel = glm::vec2(0);
                }
            }
            case enemyState::damaged:
            {
                if(obj.data.enemy.dmgDuration.step(timeDelta)){
                    obj.data.enemy.state = enemyState::shambling;
                    obj.texture = res.enemyTex;
                    obj.curAnimation = res.ENEMY_ANIMATION;
                }
                break;
            }
            case enemyState::dead:
            {
                obj.vel = glm::vec2(0);
                obj.hitbox = SDL_FRect{-1, -1, -1, -1};
                if(obj.curAnimation != -1 && obj.animations[obj.curAnimation].done()){
                    obj.spriteFrame = 18;
                    obj.curAnimation = -1;
                }
            }
        }
    }
    if(curDir){
        obj.dir = curDir;
    }
    obj.vel += obj.acc * curDir * timeDelta;
    if(std::abs(obj.vel.x) > obj.maxSpeedX) obj.vel.x = obj.maxSpeedX * curDir;
    obj.pos += obj.vel * timeDelta;
    bool foundGround = false;
    for(auto &layer : gs.layers){
        for(GameObject &other : layer){
            if(&other != &obj){
                CollisionDetection(state, gs, obj, other, timeDelta, res);
                
                SDL_FRect sensor{
                    .x = obj.pos.x + obj.hitbox.x,
                    .y = obj.pos.y + obj.hitbox.y + obj.hitbox.h,
                    .w = obj.hitbox.w,
                    .h = 1
                };
                SDL_FRect otherRect{
                    .x = other.pos.x + other.hitbox.x,
                    .y = other.pos.y + other.hitbox.y,
                    .w = other.hitbox.w,
                    .h = other.hitbox.h
                };
                SDL_FRect intersect{0};
                if(SDL_GetRectIntersectionFloat(&sensor, &otherRect, &intersect)){
                    if(intersect.h < intersect.w)
                    foundGround = true;
                }
            }
        }
    }
    if(foundGround != obj.grounded){
        obj.grounded = foundGround;
        if(obj.type == ObjectType::player && foundGround){
            obj.data.player.state = PlayerState::running;
        }
    }
}

void CollisionResponse(const SDLState &state, Resource &res, GameState &gs, GameObject &a, GameObject &b, const SDL_FRect &recA, const SDL_FRect &recB, const SDL_FRect &intersect, float timeDelta, ma_engine engine){
    const auto genericResponse = [&](){
        if(intersect.w < intersect.h){
            // Collision from left or right
            if(a.vel.x > 0){
                // A is on the left of B
                a.pos.x -= intersect.w;
            }
            else if(a.vel.x < 0){
                // A is on the right of B
                a.pos.x += intersect.w;
            }
            a.vel.x = 0;
        }
        else{
            // Collision from top or bottom
            if(a.vel.y > 0){
                // A is on the top of B
                a.pos.y -= intersect.h;
            }
            else if(a.vel.y < 0){
                // A is on the bottom of B
                a.pos.y += intersect.h;
            }
            a.vel.y = 0;
        }
    };
    if(a.type == ObjectType::player){
        switch(b.type){
            case ObjectType::level:
                genericResponse();
                break;
            case ObjectType::enemy:
            if(b.data.enemy.hitRate.step(timeDelta)) a.data.player.HP -= 10; 
                genericResponse();
                break;
        }
    }
    else if(a.type == ObjectType::bullet){
        bool passesThrough = false;
        switch(a.data.bullet.state){
            case BulletState::moving:
                switch(b.type){
                    case ObjectType::level:
                    {
                        ma_engine_play_sound(&engine, "resources/sound/shoot_hit.wav", NULL);
                        break;
                    }
                    case ObjectType::enemy:{
                        if(b.data.enemy.state != enemyState::dead){
                            b.dir = -a.dir;
                            b.flashes = true;
                            b.flashTimer.reset();
                            b.texture = res.enemyHitTex;
                            b.curAnimation = res.ENEMY_DAMAGED_ANIMATION;
                            b.data.enemy.state = enemyState::damaged;
                            b.data.enemy.HP -= 10;
                            //gs.getPlayer().data.player.HP -= 10;
                            if(b.data.enemy.HP <= 0){
                                b.data.enemy.state = enemyState::dead;
                                b.texture = res.enemyDieTex;
                                b.curAnimation = res.ENEMY_DYING_ANIMATION;
                                ma_engine_play_sound(&engine, "resources/sound/monster_die.wav", NULL);
                            }
                            ma_engine_play_sound(&engine, "resources/sound/enemy_hit.wav", NULL);
                        }
                        else{
                            passesThrough = true;
                        }
                        break;
                    }
                }
                if(!passesThrough){
                    genericResponse();
                    a.vel *= 0;
                    a.data.bullet.state = BulletState::colliding;
                    a.texture = res.bulletHitTex;
                    a.curAnimation = res.BULLET_HIT_ANIMATION;
                }
                break;
        }
    }
    else if(a.type == ObjectType::enemy){
        genericResponse();
    }
}
    

void CollisionDetection(const SDLState &state, GameState &gs, GameObject &a, GameObject &b, float timeDelta, Resource &res){
    SDL_FRect rectA{
        .x = a.pos.x + a.hitbox.x,
        .y = a.pos.y + a.hitbox.y,
        .w = a.hitbox.w,
        .h = a.hitbox.h
    };
    SDL_FRect rectB{
        .x = b.pos.x + b.hitbox.x,
        .y = b.pos.y + b.hitbox.y,
        .w = b.hitbox.w,
        .h = b.hitbox.h
    };
    SDL_FRect intersect{0};
    if(gs.debugMode){
        SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(state.renderer, 0, 255, 0, 150);
        SDL_RenderFillRect(state.renderer, &intersect);
        //SDL_RenderFillRect(state.renderer, &gs.MapViewport);
        SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);
    }
    if(SDL_GetRectIntersectionFloat(&rectA, &rectB, &intersect)) CollisionResponse(state, res, gs, a, b, rectA, rectB, intersect, timeDelta, state.engine);

}

void createTiles(const SDLState &state, GameState &gs, Resource &res){
    /*
        1- Ground
        2- Panel
        3- Enemy
        4- Player
        5- Grass
        6- Brick
    */
    short mapData[MAX_ROWS][MAX_COLS] = {
        2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		2, 0, 0, 3, 0, 0, 0, 2, 2, 0, 3, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 3, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
		2, 0, 0, 0, 0, 2, 0, 0, 2, 2, 2, 2, 0, 2, 2, 2, 0, 0, 3, 2, 2, 2, 2, 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2,
		2, 2, 0, 0, 2, 2, 2, 0, 0, 0, 3, 0, 0, 3, 0, 2, 2, 2, 2, 2, 0, 0, 2, 2, 0, 3, 0, 0, 3, 0, 2, 3, 3, 3, 0, 2, 0, 3, 3, 0, 0, 3, 0, 3, 0, 3, 0, 0, 2, 2,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };

    short BackgroundMapData[MAX_ROWS][MAX_COLS] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 6, 6, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    short ForegroundMapData[MAX_ROWS][MAX_COLS] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    const auto loadMap = [&state, &res, &gs](short layer[MAX_ROWS][MAX_COLS]){
        const auto createObj = [&state](SDL_Texture *tex, int r, int c, ObjectType type){
        GameObject obj;
        obj.type = type;
        obj.texture = tex;
        obj.pos = glm::vec2(c * TILE_SIZE, state.logH - (MAX_ROWS - r) * TILE_SIZE);
        obj.hitbox = {
            .x = 0,
            .y = 0,
            .w = static_cast<float>(TILE_SIZE),
            .h = static_cast<float>(TILE_SIZE)
        };
        return obj;
        };

        for(int r = 0; r < MAX_ROWS; r++){
            for(int c = 0; c < MAX_COLS; c++){
                switch(layer[r][c]){
                    case 1:
                        {
                        GameObject ground = createObj(res.groundTex, r, c, ObjectType::level);
                        gs.layers[LAYER_LEVEL_IDX].push_back(ground);
                        break;
                        }
                    case 2:
                        {
                        GameObject panel = createObj(res.panelTex, r, c, ObjectType::level);
                        gs.layers[LAYER_LEVEL_IDX].push_back(panel);
                        break;
                        }
                    case 3:
                        {
                            GameObject enem = createObj(res.enemyTex, r, c, ObjectType::enemy);
                            enem.data.enemy = EnemyData();
                            enem.curAnimation = res.ENEMY_ANIMATION;
                            enem.animations = res.animationsEnemy;
                            enem.dynamic = true;
                            enem.maxSpeedX = 15.0f;
                            enem.hitbox = SDL_FRect{
                                .x = 10,
                                .y = 4,
                                .w = 12,
                                .h = 28
                            };
                            gs.layers[LAYER_CHARACTER_IDX].push_back(enem);
                            break;
                        }
                    case 5:
                        {
                        GameObject grass = createObj(res.grassTex, r, c, ObjectType::level);
                        gs.ForegroundTile.push_back(grass);
                        break;
                        }
                    case 6:
                        {
                        GameObject brick = createObj(res.brickTex, r, c, ObjectType::level);
                        gs.BackgroundTile.push_back(brick);
                        break;
                        }
                    case 4:
                        {
                        GameObject player = createObj(res.idleTex, r, c, ObjectType::player);
                        player.data.player = PlayerData();
                        player.animations = res.animationsPlayer;
                        player.curAnimation = res.PLAYER_IDLE_ANIMATION;
                        player.maxSpeedX = 100;
                        player.acc = glm::vec2(300, 0);
                        player.dynamic = true;
                        player.hitbox = {
                            .x = 11,
                            .y = 6,
                            .w = 10,
                            .h = 26
                        };
                        gs.layers[LAYER_CHARACTER_IDX].push_back(player);
                        gs.playerIdx = static_cast<int>(gs.layers[LAYER_CHARACTER_IDX].size()) - 1;
                        break;
                        }
                }
            }
        }
    };
    loadMap(mapData);
    loadMap(BackgroundMapData);
    loadMap(ForegroundMapData);
    assert(gs.playerIdx != -1);
}

void HandleKey(const SDLState &state, GameState &gs, GameObject &obj, SDL_Scancode key, bool pressed){
    float JUMP_AMT = -200.00f;
    if(obj.type == ObjectType::player){
        switch(obj.data.player.state){
            case PlayerState::idle:
            {
                if(key == SDL_SCANCODE_W && pressed){
                    obj.vel.y += JUMP_AMT;
                    obj.data.player.state = PlayerState::jumping;
                }
                break;
            }
            case PlayerState::running:
            {
                if(key == SDL_SCANCODE_W && pressed){
                    obj.vel.y += JUMP_AMT;
                    obj.data.player.state = PlayerState::jumping;
                }
                break;
            }
            // case PlayerState::jumping:
            // {
            //     if(key == SDL_SCANCODE_W && pressed && obj.grounded){
            //         obj.vel.y += JUMP_AMT;
            //     }
            // }
        }
    }
}

void DrawParallaxBackground(SDL_Renderer *renderer, SDL_Texture *tex, float xVel, float &scrollPos, float scrollFact, float timeDelta){
    scrollPos -= xVel * scrollFact * timeDelta;
    if(scrollPos <= -tex->w) scrollPos = 0;
    SDL_FRect where{
        .x = scrollPos,
        .y = 30,
        .w = tex->w * 2.0f,
        .h = static_cast<float>(tex->h)
    };
    SDL_RenderTextureTiled(renderer, tex, nullptr, 1, &where);
}