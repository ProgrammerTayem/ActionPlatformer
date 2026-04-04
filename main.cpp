#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vector>
#include <map>
#include <string>
#include <array>
#include <stack>
#include <format>
#include <numeric>
#include "gameobject.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#undef interface

// This structure stores basic information of the window, renderer and the audio engine
// The constructor is basically to store the keys pressed
struct SDLState{
    SDL_Window *window;
    SDL_Renderer *renderer;
    int w, h, logW, logH;
    const bool *keys;
    ma_engine engine;
    
    SDLState() : keys(SDL_GetKeyboardState(nullptr)) {}
};

// Which interface to use currently
enum class CurrentInterface{
    MENU, OPTIONS, GAME, GAME_OVER, COMPLETE, KEYBINDINGS, QUIT
};
CurrentInterface T = CurrentInterface::MENU;

enum class KeyAction {
    MOVE_LEFT,
    MOVE_RIGHT,
    JUMP,
    SHOOT,
    DEBUG_TOGGLE,
    ACTION_COUNT
};

const char* actionNames[] = {"Move Left", "Move Right", "Jump", "Shoot", "Toggle Debug"};

enum class ModalType{
    NONE, CONFIRM_EXIT, PAUSE, INVALID
};
ModalType currentModal = ModalType::NONE;

// All resources (textures / animations) loading and unloading process is accumulated in this struct
struct UiElements{
    std::vector<Button> buttons;
    enum Textures{
        BUTTON,
        VOLUME_SLIDER_TXT,
        HP_BAR,
        QUIT_MSG,
        INVALID_MSG,
            TexturesCount
    };
    enum Buttons{
        PLAY,
        OPTIONS,
        EXIT,
        YES,
        NO,
        BACK,
        RESUME,
        RESTART,
        MENU_BTN,
        MAIN_MENU,
        KEYBINDINGS,
        APPLY,
        OK,
            ButtonsCount
    };
    std::vector<SDL_Texture*> textures;
    TTF_Font *font, *bolderFont;
    CurrentInterface previousInterface = CurrentInterface::MENU;
    ModalType previousModal = ModalType::NONE;
    bool pendingReset = false;

    SDL_Texture* addTexture(const std::string &path, SDL_Renderer *renderer){
        SDL_Texture *tex = IMG_LoadTexture(renderer, path.c_str());
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        return tex;
    }

    std::vector<buttonState> GetSceneButtonStates(CurrentInterface change, ModalType modal){
        std::vector<buttonState> states(Buttons::ButtonsCount, buttonState::hidden);

        if(modal == ModalType::CONFIRM_EXIT){
            states[Buttons::YES] = buttonState::active;
            states[Buttons::NO] = buttonState::active;
            return states;
        }

        if(modal == ModalType::PAUSE){
            states[Buttons::RESUME] = buttonState::active;
            states[Buttons::BACK] = buttonState::active;
            // Exit in the pause modal (optional)
            states[Buttons::EXIT] = buttonState::active;
            return states;
        }

        if(modal == ModalType::INVALID){
            states[Buttons::OK] = buttonState::active;
            return states;
        }

        switch(change){
            case CurrentInterface::MENU:
                states[Buttons::PLAY] = buttonState::active;
                states[Buttons::OPTIONS] = buttonState::active;
                states[Buttons::EXIT] = buttonState::active;
                break;
            case CurrentInterface::OPTIONS:
                states[Buttons::KEYBINDINGS] = buttonState::active;
                states[Buttons::BACK] = buttonState::active;
                break;
            case CurrentInterface::GAME:
                // no main menu buttons in gameplay
                break;
            case CurrentInterface::GAME_OVER:
                states[Buttons::RESTART] = buttonState::active;
                states[Buttons::MENU_BTN] = buttonState::active;
                break;
            case CurrentInterface::COMPLETE:
                states[Buttons::RESTART] = buttonState::active;
                states[Buttons::MENU_BTN] = buttonState::active;
                break;
            case CurrentInterface::KEYBINDINGS:
                states[Buttons::APPLY] = buttonState::active;
                break;
            case CurrentInterface::QUIT:
                break;
        }

        return states;
    }

    void ApplyStateChanges(const std::vector<buttonState> &stateChanges){
        if(stateChanges.size() != buttons.size()) return;
        for(size_t i = 0; i < buttons.size(); i++){
            buttons[i].state = stateChanges[i];
        }
    }

    void SetScene(CurrentInterface change, ModalType modal){
        if (modal != ModalType::NONE){
            previousInterface = T;
            previousModal = currentModal;
        }
        T = change;
        currentModal = modal;
        ApplyStateChanges(GetSceneButtonStates(change, modal));
    }

    void SetScene(CurrentInterface change, ModalType modal, const std::vector<buttonState> &stateChanges){
        // Deprecated path for backwards compatibility.
        if (modal != ModalType::NONE){
            previousInterface = T;
            previousModal = currentModal;
        }
        T = change;
        currentModal = modal;
        if(modal == ModalType::CONFIRM_EXIT || modal == ModalType::PAUSE){
            ApplyStateChanges(GetSceneButtonStates(change, modal));
        } else {
            ApplyStateChanges(stateChanges);
        }
    }

    Button CreateButton(Button b, const SDLState &state, CurrentInterface change, const std::vector<buttonState> &buttonStateChanges, ModalType modal = ModalType::NONE, bool restorePrevious = false){
        b.StateChanges = buttonStateChanges;
        if(restorePrevious){
            b.onClick = [this](Button &btn){
                SetScene(previousInterface, previousModal);
            };
        } else {
            b.onClick = [this, change, modal](Button &btn){
                SetScene(change, modal);
            };
        }
        SDL_Surface *surf = TTF_RenderText_Blended(font, b.s, strlen(b.s), {b.text.r, b.text.g, b.text.b, b.text.a});
        b.txtTexture = SDL_CreateTextureFromSurface(state.renderer, surf);
        b.txt_dim = glm::vec2(surf->w, surf->h);
        SDL_DestroySurface(surf);
        return b;
    }

    void ButtonStateArrayReset(std::vector<buttonState> &ref){
        std::fill(ref.begin(), ref.end(), buttonState::hidden);
    }
    
    bool ResizeButton(Button &b, SDL_FRect newRect){
        // fprintf(stderr, "%f %f %f %f %s\n", b.pos.x, b.pos.y, b.w, b.h, b.s);
        // fprintf(stderr, "%f %f %f %f %s\n", newRect.x, newRect.y, newRect.w, newRect.h, b.s);
        // fprintf(stderr, "%d %d %d\n", (glm::distance(b.pos, glm::vec2(newRect.x, newRect.y))) == 0.0f, (b.w == newRect.w ), (b.h == newRect.h));
        if((glm::distance(b.pos, glm::vec2(newRect.x, newRect.y))) == 0.0f && (b.w == newRect.w) && (b.h == newRect.h)){
            return false;
        }
        b.pos = glm::vec2(newRect.x, newRect.y);
        b.w = newRect.w;
        b.h = newRect.h;
        return true;
    }

    void UILoad(SDLState &state){
        font = TTF_OpenFont("resources/fonts/Roboto-Thin.ttf", 14);
        bolderFont = TTF_OpenFont("resources/fonts/Roboto_Condensed-Black.ttf", 16);
        if(font == nullptr || bolderFont == nullptr){
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to TextureLoad fonts!", nullptr);
        }
        textures.resize(Textures::TexturesCount);
        textures[Textures::BUTTON] = addTexture("resources/button.png", state.renderer);
        buttons.resize(Buttons::ButtonsCount, Button());

        std::vector<buttonState> buttonStateChanges;
        buttonStateChanges.resize(Buttons::ButtonsCount);
        // Initialize with all buttons hidden upon a click
        ButtonStateArrayReset(buttonStateChanges);


        Button btn = Button();
        btn.s = "Play";
        btn.h = 30.0f;
        btn.pos = glm::vec2(static_cast<float>(state.logW/2-75), static_cast<float>(state.logH/2-15-50));
        btn.text = SDL_Color{50, 50, 50, 255};
        btn.texture = textures[Textures::BUTTON];
        btn.state = buttonState::active;
        buttons[Buttons::PLAY] = CreateButton(btn, state, CurrentInterface::GAME, buttonStateChanges);

        btn.s = "Options";
        btn.pos = glm::vec2(static_cast<float>(state.logW/2-75), static_cast<float>(state.logH/2-15-10));
        btn.state = buttonState::active;
        buttonStateChanges[Buttons::BACK] = buttonState::active;
        buttons[Buttons::OPTIONS] = CreateButton(btn, state, CurrentInterface::OPTIONS, buttonStateChanges);
        ButtonStateArrayReset(buttonStateChanges);

        btn.s = "Exit";
        btn.pos = glm::vec2(static_cast<float>(state.logW/2-75), static_cast<float>(state.logH/2-15+30));
        btn.state = buttonState::active;
        buttonStateChanges[Buttons::PLAY] = buttonState::inactive;
        buttonStateChanges[Buttons::OPTIONS] = buttonState::inactive;
        buttonStateChanges[Buttons::EXIT] = buttonState::inactive;
        buttonStateChanges[Buttons::YES] = buttonState::active;
        buttonStateChanges[Buttons::NO] = buttonState::active;

        buttons[Buttons::EXIT] = CreateButton(btn, state, CurrentInterface::MENU, buttonStateChanges, ModalType::CONFIRM_EXIT);
        ButtonStateArrayReset(buttonStateChanges);

        btn.s = "Yes";
        btn.h = 25.0f;
        btn.pos = glm::vec2(0.0f, 0.0f);
        btn.text = SDL_Color{110, 110, 110, 255};
        btn.state = buttonState::hidden;
        buttons[Buttons::YES] = CreateButton(btn, state, CurrentInterface::QUIT, buttonStateChanges);

        btn.s = "No";
        buttonStateChanges[Buttons::PLAY] = buttonState::active;
        buttonStateChanges[Buttons::OPTIONS] = buttonState::active;
        buttonStateChanges[Buttons::EXIT] = buttonState::active;
        btn.state = buttonState::hidden;
        buttons[Buttons::NO] = CreateButton(btn, state, CurrentInterface::MENU, buttonStateChanges, ModalType::NONE, true);
        ButtonStateArrayReset(buttonStateChanges);

        btn.s = "Ok";
        btn.pos = glm::vec2(0.0f, 0.0f);
        btn.h = 25.0f;
        btn.text = SDL_Color{110, 110, 110, 255};
        btn.state = buttonState::hidden;
        buttons[Buttons::OK] = CreateButton(btn, state, CurrentInterface::KEYBINDINGS, buttonStateChanges, ModalType::NONE, true);
        ButtonStateArrayReset(buttonStateChanges);

        btn.s = "Back";
        btn.pos = glm::vec2(static_cast<float>(state.logW/2-75), static_cast<float>(state.logH/2-15+70));
        btn.text = SDL_Color{50, 50, 50, 255};
        buttonStateChanges[Buttons::PLAY] = buttonState::active;
        buttonStateChanges[Buttons::OPTIONS] = buttonState::active;
        buttonStateChanges[Buttons::EXIT] = buttonState::active;
        btn.state = buttonState::hidden;
        buttons[Buttons::BACK] = CreateButton(btn, state, CurrentInterface::MENU, buttonStateChanges);
        ButtonStateArrayReset(buttonStateChanges);

        btn.s = "Keybindings";
        btn.pos = glm::vec2(static_cast<float>(state.logW/2-75), static_cast<float>(state.logH/2-15+35));
        btn.text = SDL_Color{50, 50, 50, 255};
        btn.state = buttonState::hidden;
        buttons[Buttons::KEYBINDINGS] = CreateButton(btn, state, CurrentInterface::KEYBINDINGS, buttonStateChanges);
        ButtonStateArrayReset(buttonStateChanges);

        btn.s = "Apply";
        btn.pos = glm::vec2(static_cast<float>(state.logW/2-75), static_cast<float>(state.logH/2-15+120));
        btn.text = SDL_Color{50, 50, 50, 255};
        buttonStateChanges[Buttons::KEYBINDINGS] = buttonState::active;
        btn.state = buttonState::hidden;
        buttons[Buttons::APPLY] = CreateButton(btn, state, CurrentInterface::OPTIONS, buttonStateChanges);
        ButtonStateArrayReset(buttonStateChanges);

        btn.s = "Resume";
        btn.pos = glm::vec2(static_cast<float>(state.logW/2-75), static_cast<float>(state.logH/2-15-50));
        btn.state = buttonState::hidden;
        buttons[Buttons::RESUME] = CreateButton(btn, state, CurrentInterface::GAME, buttonStateChanges);

        btn.s = "Restart";
        btn.h = 30.0f;
        btn.pos = glm::vec2(static_cast<float>(state.logW/2-75), static_cast<float>(state.logH/2-15-50));
        btn.text = SDL_Color{50, 50, 50, 255};
        btn.state = buttonState::hidden;
        buttons[Buttons::RESTART] = CreateButton(btn, state, CurrentInterface::GAME, buttonStateChanges);
        // Override onClick for RESTART to set pendingReset flag
        buttons[Buttons::RESTART].onClick = [this](Button &btn){
            pendingReset = true;
            SetScene(CurrentInterface::GAME, ModalType::NONE);
        };
        ButtonStateArrayReset(buttonStateChanges);

        btn.s = "Menu";
        btn.pos = glm::vec2(static_cast<float>(state.logW/2-75), static_cast<float>(state.logH/2-15+30));
        btn.state = buttonState::hidden;
        buttonStateChanges[Buttons::PLAY] = buttonState::active;
        buttonStateChanges[Buttons::OPTIONS] = buttonState::active;
        buttonStateChanges[Buttons::EXIT] = buttonState::active;
        buttons[Buttons::MENU_BTN] = CreateButton(btn, state, CurrentInterface::MENU, buttonStateChanges);
        ButtonStateArrayReset(buttonStateChanges);

        SDL_Surface *surf = TTF_RenderText_Blended(bolderFont, "Volume:", strlen("Volume:"), {255, 255, 255, 255});
        textures[Textures::VOLUME_SLIDER_TXT] = SDL_CreateTextureFromSurface(state.renderer, surf);
        SDL_DestroySurface(surf);
        surf = TTF_RenderText_Blended(bolderFont, "Are you sure you want to quit", strlen("Are you sure you want to quit"), {255, 255, 255, 255});
        textures[Textures::QUIT_MSG] = SDL_CreateTextureFromSurface(state.renderer, surf);
        SDL_DestroySurface(surf);
        surf = TTF_RenderText_Blended(bolderFont, "This button is already assigned to another action!", strlen("This button is already assigned to another action!"), {255, 255, 255, 255});
        textures[Textures::INVALID_MSG] = SDL_CreateTextureFromSurface(state.renderer, surf);
        SDL_DestroySurface(surf);
    }

    void unload(){
        for(SDL_Texture* tex : textures){
            SDL_DestroyTexture(tex);
        }
        for(Button &b : buttons){
            SDL_DestroyTexture(b.txtTexture);
        }
        buttons.clear();
        TTF_CloseFont(font);
        TTF_CloseFont(bolderFont);
    }

};
struct Resource{
    UiElements UI;
    enum PlayerAnimationType{
        playerIdle, 
        playerRunning,
        playerSliding,
        playerShooting,
        playerSlideShooting,
            playerAnimationTypeCount
    };

    enum BulletAnimationType{
         bulletMoving, 
         bulletHit, 
            BulletAnimationTypeCount
    };

    enum EnemyAnimationType{
        enemyIdle, 
        enemyDamaged, 
        enemyDying, 
            enemyAnimationTypeCount
    };

    enum Textures{
        IDLE, RUN, SLIDE, PANEL, GROUND, ENEMY, GRASS, BRICK, BCKGRND1, BCKGRND2, BCKGRND3, BCKGRND4,
        BULLET, BULLET_HIT, SHOOT, RUN_SHOOT, SLIDE_SHOOT, ENEMY_HIT, ENEMY_DIE, TexturesCount
    };

    std::vector<Animation> animationsPlayer, animationsBullet, animationsEnemy;
    std::vector<SDL_Texture*> textures;

    SDL_Texture* addTexture(const std::string &path, SDL_Renderer *renderer){
        SDL_Texture *tex = IMG_LoadTexture(renderer, path.c_str());
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        return tex;
    }

    void TextureLoad(SDLState &state){
        animationsPlayer.resize(PlayerAnimationType::playerAnimationTypeCount);
        animationsPlayer[PlayerAnimationType::playerIdle] = Animation(8, 1.6f);
        animationsPlayer[PlayerAnimationType::playerRunning] = Animation(4, 0.5f);
        animationsPlayer[PlayerAnimationType::playerSliding] = Animation(1, 1.0f);
        animationsPlayer[PlayerAnimationType::playerShooting] = Animation(4, 0.5f);
        animationsPlayer[PlayerAnimationType::playerSlideShooting] = Animation(4, 0.5f);

        animationsBullet.resize(BulletAnimationType::BulletAnimationTypeCount);
        animationsBullet[BulletAnimationType::bulletMoving] = Animation(4, 0.05f);
        animationsBullet[BulletAnimationType::bulletHit] = Animation(4, 0.15f);

        animationsEnemy.resize(EnemyAnimationType::enemyAnimationTypeCount);
        animationsEnemy[EnemyAnimationType::enemyIdle] = Animation(8, 1.0f);
        animationsEnemy[EnemyAnimationType::enemyDamaged] = Animation(8, 1.0f);
        animationsEnemy[EnemyAnimationType::enemyDying] = Animation(18, 2.0f);

        textures.resize(Textures::TexturesCount);
        textures[Textures::IDLE] = addTexture("resources/idle.png", state.renderer);
        textures[Textures::RUN] = addTexture("resources/run.png", state.renderer);
        textures[Textures::SLIDE] = addTexture("resources/slide.png", state.renderer);
        textures[Textures::PANEL] = addTexture("resources/tiles/panel.png", state.renderer);
        textures[Textures::GROUND] = addTexture("resources/tiles/ground.png", state.renderer);
        textures[Textures::GRASS] = addTexture("resources/tiles/grass.png", state.renderer);
        textures[Textures::BRICK] = addTexture("resources/tiles/brick.png", state.renderer);
        textures[Textures::BCKGRND1] = addTexture("resources/bckgrnd/bg_layer1.png", state.renderer);
        textures[Textures::BCKGRND2] = addTexture("resources/bckgrnd/bg_layer2.png", state.renderer);
        textures[Textures::BCKGRND3] = addTexture("resources/bckgrnd/bg_layer3.png", state.renderer);
        textures[Textures::BCKGRND4] = addTexture("resources/bckgrnd/bg_layer4.png", state.renderer);
        textures[Textures::BULLET] = addTexture("resources/bullet.png", state.renderer);
        textures[Textures::BULLET_HIT] = addTexture("resources/bullet_hit.png", state.renderer);
        textures[Textures::SHOOT] = addTexture("resources/shoot.png", state.renderer);
        textures[Textures::RUN_SHOOT] = addTexture("resources/shoot_run.png", state.renderer);
        textures[Textures::SLIDE_SHOOT] = addTexture("resources/slide_shoot.png", state.renderer);
        textures[Textures::ENEMY] = addTexture("resources/enemy.png", state.renderer);
        textures[Textures::ENEMY_HIT] = addTexture("resources/enemy_hit.png", state.renderer);
        textures[Textures::ENEMY_DIE] = addTexture("resources/enemy_die.png", state.renderer);
        UI.UILoad(state);
    }

    void unload(){
        for(SDL_Texture* tex : textures){
            SDL_DestroyTexture(tex);
        }
    }
};


const int LAYER_LEVEL_IDX = 0;
const int LAYER_CHARACTER_IDX = 1;;

enum class Axis{
    X, Y
};

struct GameState{
    std::array<std::vector<GameObject>, 2>layers;
    std::vector<GameObject> Bullets;
    SDL_FRect MapViewport;
    Slider musicSlider;
    int playerIdx;
    float bg2scroll, bg3scroll, bg4scroll;
    bool debugMode, entitiesLoaded;
    int enemiesKilled, totalEnemies;
    SDL_Scancode keyBindings[static_cast<int>(KeyAction::ACTION_COUNT)];
    int rebindingAction;
    std::string newKeyName;
    SDL_Scancode newScancode;
    GameState(const SDLState &state) : playerIdx(-1), enemiesKilled(0), totalEnemies(0), rebindingAction(-1), newScancode(SDL_SCANCODE_UNKNOWN) {
        MapViewport = SDL_FRect{
            .x = 0,
            .y = 0,
            .w = static_cast<float>(state.logW),
            .h = static_cast<float>(state.logH)
        };
        bg2scroll = bg3scroll = bg4scroll = 0.0f;
        debugMode = false;
        entitiesLoaded = false;
        keyBindings[static_cast<int>(KeyAction::MOVE_LEFT)] = SDL_SCANCODE_A;
        keyBindings[static_cast<int>(KeyAction::MOVE_RIGHT)] = SDL_SCANCODE_D;
        keyBindings[static_cast<int>(KeyAction::JUMP)] = SDL_SCANCODE_W;
        keyBindings[static_cast<int>(KeyAction::SHOOT)] = SDL_SCANCODE_K;
        keyBindings[static_cast<int>(KeyAction::DEBUG_TOGGLE)] = SDL_SCANCODE_F10;
    }
    GameObject &getPlayer(){
        return layers[LAYER_CHARACTER_IDX][playerIdx];
    }
    
    void reset(){
        layers[LAYER_LEVEL_IDX].clear();
        layers[LAYER_CHARACTER_IDX].clear();
        Bullets.clear();
        bg2scroll = bg3scroll = bg4scroll = 0.0f;
        entitiesLoaded = false;
        playerIdx = -1;
        enemiesKilled = 0;
        totalEnemies = 0;
        rebindingAction = -1;
        newKeyName = "";
        newScancode = SDL_SCANCODE_UNKNOWN;
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
void drawButton(const SDLState &state, Resource &res, Button &b);
void update(const SDLState &state, GameState &gs,GameObject &obj, Resource &res, float timeDelta, ma_engine engine);
void DrawBackground(SDL_Renderer *renderer, const SDLState &state, GameState &gs, Resource &res, float xVel, float timeDelta);
void RenderSlider(const SDLState &state, GameState &gs, Resource &res, float mousePosx, float mousePosy, glm::vec2 padding = glm::vec2(0.0f, 0.0f), float scaleFactor = 1.0f);
void HandleSliderUpdate(const SDL_Event &e, const SDLState &state, GameState &gs, ma_sound &music);
bool createTiles(const SDLState &state, GameState &gs, Resource &res);
void HandleKey(const SDLState &state, GameState &gs, Resource &res, GameObject &obj, SDL_Scancode key, bool pressed);
void DrawParallaxBackground(SDL_Renderer *renderer, SDL_Texture *tex, float xVel, float &scrollPos, float scrollFact, float timeDelta);
void HandleButtonEvent(Button &btn, const SDL_Event &e);
void ProcessMovementForObject(const SDLState &state, GameState &gs, GameObject &obj, float dt, Resource &res, ma_engine &engine);
void CollisionDetectionAxis(const SDLState &state, GameState &gs, GameObject &a, GameObject &b,
                            const SDL_FRect &rectA, const SDL_FRect &rectB, Axis axis,
                            float timeDelta, Resource &res, ma_engine &engine, bool &flag);
static void OnCollisionBegin(GameState &gs, GameObject &a, GameObject &b, Resource &res, ma_engine &engine, float timeDelta, const SDLState &state);
static void OnCollisionPersist(GameState &gs, GameObject &a, GameObject &b, Resource &res, ma_engine &engine, float timeDelta, const SDLState &state);
static void ResolveOverlapX(GameObject &a, const SDL_FRect &intersect);
static void ResolveOverlapY(GameObject &a, const SDL_FRect &intersect);
void HandleEvents(const SDL_Event &event, SDLState &state, GameState &gs, Resource &res, CurrentInterface &T, ModalType &currentModal, ma_sound &music, bool &running);
void UpdateMousePosition(float &mousePosx, float &mousePosy, const SDLState &state);
void HandlePendingReset(GameState &gs, Resource &res);
void UpdateGame(const SDLState &state, GameState &gs, Resource &res, float timeDelta);
void Render(const SDLState &state, GameState &gs, Resource &res, CurrentInterface T, ModalType currentModal, float mousePosx, float mousePosy, float timeDelta);

inline bool GetRectIntersectionSafe(const SDL_FRect &A, const SDL_FRect &B, SDL_FRect &out, float epsilon = 1e-4f)
{
    float x1 = std::max(A.x, B.x);
    float y1 = std::max(A.y, B.y);
    float x2 = std::min(A.x + A.w, B.x + B.w);
    float y2 = std::min(A.y + A.h, B.y + B.h);

    float w = x2 - x1;
    float h = y2 - y1;

    if (w <= epsilon || h <= epsilon) return false;

    out.x = x1; out.y = y1; out.w = w; out.h = h;
    return true;
}

inline SDL_FRect MakeWorldRect(const GameObject &o) {
    return SDL_FRect{ o.pos.x + o.hitbox.x, o.pos.y + o.hitbox.y, o.hitbox.w, o.hitbox.h };
}

inline void updateSliderKnob(Slider &s){
    s.knob.x = s.track.x + s.val * s.track.w  - s.knob.w / 2.0;
}

inline SDL_FRect MakeRect(float x, float y, float w, float h){
    return SDL_FRect{x, y, w, h};
}

inline bool PointInRect(float px, float py, const SDL_FRect &rect){
    return (px >= rect.x && px <= rect.x + rect.w && py >= rect.y && py <= rect.y + rect.h);
}

inline Slider SliderInit(const SDLState &state, glm::vec2 padding = glm::vec2(0.0f, 0.0f), float scaleFactor = 1.0f, float initialVal = 1.0f, bool dragging = false){
    return Slider(state.logW/2.0 - 100.0f + padding.x * scaleFactor, state.logH/2.0 - 5.0f + padding.y * scaleFactor, 200.0f * scaleFactor, 10.0f * scaleFactor, 8.0f * scaleFactor, 16.0f * scaleFactor, initialVal, dragging);
}

void RenderDimOverlay(SDL_Renderer* renderer, int w, int h){
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_FRect overlayRect = {0, 0, static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderFillRect(renderer, &overlayRect);
}

int main(int argc, char* argv[]){
    float mx, my, mousePosx, mousePosy;
    SDLState state;
    SDL_FRect playButton;
    Resource res;
    state.w = 1600;
    state.h = 900;
    state.logW = 640;
    state.logH = 320;
    if(init(state) == false) return 1;
    ma_sound music;
    GameState gs(state);
    res.TextureLoad(state);
    gs.musicSlider = SliderInit(state);
    ma_sound_init_from_file(&state.engine, "resources/sound/Juhani Junkala.mp3", MA_SOUND_FLAG_LOOPING, NULL, NULL, &music);
    ma_sound_set_volume(&music, gs.musicSlider.val);
    ma_sound_start(&music);

    uint64_t timeP = SDL_GetTicks();

    bool running = true;
    while(running){
        uint64_t timeC = SDL_GetTicks();
        float timeDelta = (timeC - timeP) / 1000.0f;
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            HandleEvents(event, state, gs, res, T, currentModal, music, running);
        }

        UpdateMousePosition(mousePosx, mousePosy, state);

        HandlePendingReset(gs, res);

        if(T == CurrentInterface::QUIT) running = false;

        if(T == CurrentInterface::GAME){
            UpdateGame(state, gs, res, timeDelta);
        }
        else{
            DrawBackground(state.renderer, state, gs, res, 150.0f, timeDelta);
            //SDL_RenderPresent(state.renderer);
        }


        Render(state, gs, res, T, currentModal, mousePosx, mousePosy, timeDelta);
        SDL_RenderPresent(state.renderer);
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
    TTF_Quit();
    SDL_Quit();
}

void HandleEvents(const SDL_Event &event, SDLState &state, GameState &gs, Resource &res, CurrentInterface &T, ModalType &currentModal, ma_sound &music, bool &running){
    for(auto &b : res.UI.buttons){
        HandleButtonEvent(b, event);
    }
    if(T == CurrentInterface::OPTIONS || currentModal == ModalType::PAUSE) HandleSliderUpdate(event, state, gs, music);
    switch(event.type){
        case SDL_EVENT_QUIT:
            running = false;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            state.w = event.window.data1;
            state.h = event.window.data2;
            break;
        case SDL_EVENT_KEY_DOWN:
            if(T == CurrentInterface::GAME){
                HandleKey(state, gs, res, gs.getPlayer(), event.key.scancode, true);
            } else if(T == CurrentInterface::KEYBINDINGS){
                if(gs.rebindingAction == -1){
                    for(int i = 0; i < static_cast<int>(KeyAction::ACTION_COUNT); i++){
                        if(event.key.scancode == gs.keyBindings[i]){
                            gs.rebindingAction = i;
                            break;
                        }
                    }
                } else {
                    if(event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_KP_ENTER){
                        if(!gs.newKeyName.empty()){
                            bool duplicate = false;
                            for(int j = 0; j < static_cast<int>(KeyAction::ACTION_COUNT); j++){
                                if(j != gs.rebindingAction && gs.keyBindings[j] == gs.newScancode){
                                    duplicate = true;
                                    break;
                                }
                            }
                            if(duplicate){
                                res.UI.SetScene(CurrentInterface::KEYBINDINGS, ModalType::INVALID);
                            } else {
                                gs.keyBindings[gs.rebindingAction] = gs.newScancode;
                            }
                        }
                        gs.rebindingAction = -1;
                        gs.newKeyName = "";
                    } else {
                        gs.newScancode = event.key.scancode;
                        gs.newKeyName = SDL_GetScancodeName(event.key.scancode);
                    }
                }
            }
            break;
        case SDL_EVENT_KEY_UP:
            HandleKey(state, gs, res, gs.getPlayer(), event.key.scancode, false);
            if(event.key.scancode == gs.keyBindings[static_cast<int>(KeyAction::DEBUG_TOGGLE)]) gs.debugMode = !gs.debugMode;
            break;
        default:
            break;
    }
}

void UpdateMousePosition(float &mousePosx, float &mousePosy, const SDLState &state){
    SDL_GetMouseState(&mousePosx, &mousePosy);
    SDL_RenderCoordinatesFromWindow(state.renderer, mousePosx, mousePosy, &mousePosx, &mousePosy);
}

void HandlePendingReset(GameState &gs, Resource &res){
    if(res.UI.pendingReset){
        gs.reset();
        res.UI.pendingReset = false;
    }
}

void UpdateGame(const SDLState &state, GameState &gs, Resource &res, float timeDelta){
    if(!gs.entitiesLoaded){
        createTiles(state, gs, res);
    }
    if(SDL_CursorVisible()) SDL_HideCursor();
    for(auto &layer : gs.layers){
        for(GameObject &obj : layer){
            if(obj.Tar == ObjectTarget::playable) update(state, gs, obj, res, timeDelta, state.engine);
        }
    }

    for(GameObject &bullet : gs.Bullets){
        update(state, gs, bullet, res, timeDelta, state.engine);
    }

    gs.MapViewport.x = gs.getPlayer().pos.x + TILE_SIZE / 2 - state.logW / 2;
    bool isNotPaused = (currentModal != ModalType::PAUSE);
    DrawBackground(state.renderer, state, gs, res, gs.getPlayer().vel.x * isNotPaused, timeDelta);
    if(gs.debugMode){
        SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 255);
        char stateText[80];
        int idle_bullets = 0, moving_bullets = 0, colliding_bullets = 0;
        for(int i = 0; i < gs.Bullets.size(); i++){
            if(gs.Bullets[i].data.bullet.state == BulletState::idle){
                idle_bullets++;
            }
            if(gs.Bullets[i].data.bullet.state == BulletState::moving) moving_bullets++;
            if(gs.Bullets[i].data.bullet.state == BulletState::colliding) colliding_bullets++;
        }
        SDL_snprintf(stateText, 80, "S: %d IB: %d MB: %d CB: %d Grnd: %d", static_cast<int>(gs.getPlayer().data.player.state), idle_bullets, moving_bullets, colliding_bullets, gs.getPlayer().grounded);
        SDL_RenderDebugText(state.renderer, 5, 5, stateText);
        SDL_SetRenderDrawColor(state.renderer, 0, 255, 0, 255);
    }

    for(auto &obj : gs.layers[LAYER_LEVEL_IDX]){
        if(obj.Tar == ObjectTarget::background){
            SDL_FRect to{
            .x = obj.pos.x - gs.MapViewport.x,
            .y = obj.pos.y,
            .w = static_cast<float>(obj.texture->w),
            .h = static_cast<float>(obj.texture->h)
            };
            SDL_RenderTexture(state.renderer, obj.texture, nullptr, &to);
        }
    }

    for(auto &layer : gs.layers){
        for(GameObject &obj : layer){
            if(obj.Tar == ObjectTarget::playable); DrawObj(state, gs, obj, TILE_SIZE, TILE_SIZE, timeDelta);
        }
    }

    for(GameObject &gb : gs.Bullets){
        if(gb.data.bullet.state != BulletState::idle) DrawObj(state, gs, gb, gb.hitbox.w, gb.hitbox.h, timeDelta);
    }

    for(auto &obj : gs.layers[LAYER_LEVEL_IDX]){
        if(obj.Tar == ObjectTarget::foreground){
            SDL_FRect to{
            .x = obj.pos.x - gs.MapViewport.x,
            .y = obj.pos.y,
            .w = static_cast<float>(obj.texture->w),
            .h = static_cast<float>(obj.texture->h)
            };
            SDL_RenderTexture(state.renderer, obj.texture, nullptr, &to);
        }
    }

    float percHP = gs.getPlayer().data.player.HP / gs.getPlayer().data.player.HPmax;
    percHP = glm::clamp(percHP, 0.0f, 1.0f);

    char hpText[64];
    SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 255);
    SDL_snprintf(hpText, 64, "HP: %.0f / %.0f", gs.getPlayer().data.player.HP, gs.getPlayer().data.player.HPmax);
    SDL_Color none = {255, 255, 255, 0};
    SDL_Surface *surf = TTF_RenderText_Blended(res.UI.font, hpText, strlen(hpText), none);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(state.renderer, surf);
    SDL_DestroySurface(surf);
    float w, h;
    SDL_GetTextureSize(texture, &w, &h);
    SDL_FRect dst = MakeRect(static_cast<float>(state.logW - 200), 15.0f, w, h);
    SDL_RenderTexture(state.renderer, texture, nullptr, &dst);

    // Render task text
    char taskText[128];
    SDL_snprintf(taskText, 128, "Kill all enemies\nProgress: %d/%d", gs.enemiesKilled, gs.totalEnemies);
    SDL_Color taskColor = {255, 255, 255, 255};
    std::string taskStr = taskText;
    size_t pos = 0;
    float taskY = 15.0f;
    while ((pos = taskStr.find('\n')) != std::string::npos) {
        std::string line = taskStr.substr(0, pos);
        taskStr.erase(0, pos + 1);
        SDL_Surface *taskSurf = TTF_RenderText_Blended(res.UI.bolderFont, line.c_str(), line.length(), taskColor);
        SDL_Texture *taskTexture = SDL_CreateTextureFromSurface(state.renderer, taskSurf);
        SDL_DestroySurface(taskSurf);
        float taskW, taskH;
        SDL_GetTextureSize(taskTexture, &taskW, &taskH);
        SDL_FRect taskDst = MakeRect(10.0f, taskY, taskW, taskH);
        SDL_RenderTexture(state.renderer, taskTexture, nullptr, &taskDst);
        SDL_DestroyTexture(taskTexture);
        taskY += taskH;
    }
    if (!taskStr.empty()) {
        SDL_Surface *taskSurf = TTF_RenderText_Blended(res.UI.bolderFont, taskStr.c_str(), taskStr.length(), taskColor);
        SDL_Texture *taskTexture = SDL_CreateTextureFromSurface(state.renderer, taskSurf);
        SDL_DestroySurface(taskSurf);
        float taskW, taskH;
        SDL_GetTextureSize(taskTexture, &taskW, &taskH);
        SDL_FRect taskDst = MakeRect(10.0f, taskY, taskW, taskH);
        SDL_RenderTexture(state.renderer, taskTexture, nullptr, &taskDst);
        SDL_DestroyTexture(taskTexture);
    }

    SDL_FRect bg = {static_cast<float>(state.logW - 200), 45.0f, HP_BAR_WIDTH, HP_BAR_HEIGHT}, 
    fg = {static_cast<float>(state.logW - 200), 45.0f, percHP*150, HP_BAR_HEIGHT},
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
    
    // Check if player died
    if(gs.getPlayer().data.player.HP <= 0){
        res.UI.SetScene(CurrentInterface::GAME_OVER, ModalType::NONE);
    }
    
    // Check if task completed
    if(gs.enemiesKilled == gs.totalEnemies && gs.totalEnemies > 0){
        res.UI.SetScene(CurrentInterface::COMPLETE, ModalType::NONE);
    }
}

void Render(const SDLState &state, GameState &gs, Resource &res, CurrentInterface T, ModalType currentModal, float mousePosx, float mousePosy, float timeDelta){
    if(T == CurrentInterface::GAME){
        // Game rendering is handled in UpdateGame
    } else {
        DrawBackground(state.renderer, state, gs, res, 150.0f, timeDelta);
    }

    if(T == CurrentInterface::OPTIONS){
        RenderSlider(state, gs, res, mousePosx, mousePosy);
    }

    if(T == CurrentInterface::KEYBINDINGS){
        // Render table
        float tableX = state.logW / 2.0f - 200.0f;
        float tableY = state.logH / 2.0f - 100.0f;
        float rowHeight = 30.0f;
        float col1X = tableX;
        float col2X = tableX + 150.0f;
        float col3X = tableX + 250.0f;

        // Headers
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface *headerSurf = TTF_RenderText_Blended(res.UI.font, "Action", strlen("Action"), white);
        SDL_Texture *headerTex = SDL_CreateTextureFromSurface(state.renderer, headerSurf);
        SDL_DestroySurface(headerSurf);
        float hw, hh;
        SDL_GetTextureSize(headerTex, &hw, &hh);
        SDL_FRect headerDst = MakeRect(col1X, tableY, hw, hh);
        SDL_RenderTexture(state.renderer, headerTex, nullptr, &headerDst);
        SDL_DestroyTexture(headerTex);

        headerSurf = TTF_RenderText_Blended(res.UI.font, "Key", strlen("Key"), white);
        headerTex = SDL_CreateTextureFromSurface(state.renderer, headerSurf);
        SDL_DestroySurface(headerSurf);
        SDL_GetTextureSize(headerTex, &hw, &hh);
        headerDst = MakeRect(col2X, tableY, hw, hh);
        SDL_RenderTexture(state.renderer, headerTex, nullptr, &headerDst);
        SDL_DestroyTexture(headerTex);

        headerSurf = TTF_RenderText_Blended(res.UI.font, "Change to", strlen("Change to"), white);
        headerTex = SDL_CreateTextureFromSurface(state.renderer, headerSurf);
        SDL_DestroySurface(headerSurf);
        SDL_GetTextureSize(headerTex, &hw, &hh);
        headerDst = MakeRect(col3X, tableY, hw, hh);
        SDL_RenderTexture(state.renderer, headerTex, nullptr, &headerDst);
        SDL_DestroyTexture(headerTex);

        // White background for Key and Change to columns
        SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
        SDL_FRect background = {col2X, tableY + rowHeight, 200, rowHeight * static_cast<int>(KeyAction::ACTION_COUNT)};
        SDL_RenderFillRect(state.renderer, &background);

        // Rows
        for(int i = 0; i < static_cast<int>(KeyAction::ACTION_COUNT); i++){
            float rowY = tableY + (i + 1) * rowHeight;

            // Highlight entire row if rebinding
            if(gs.rebindingAction == i){
                SDL_SetRenderDrawColor(state.renderer, 50, 50, 50, 255);
                SDL_FRect highlight = {tableX - 10, rowY - 5, 400, rowHeight};
                SDL_RenderFillRect(state.renderer, &highlight);
            }

            // Action name (left-aligned)
            SDL_Surface *actionSurf = TTF_RenderText_Blended(res.UI.font, actionNames[i], strlen(actionNames[i]), white);
            SDL_Texture *actionTex = SDL_CreateTextureFromSurface(state.renderer, actionSurf);
            SDL_DestroySurface(actionSurf);
            float aw, ah;
            SDL_GetTextureSize(actionTex, &aw, &ah);
            float actionTextY = rowY + (rowHeight - ah) / 2;
            SDL_FRect actionDst = MakeRect(col1X, actionTextY, aw, ah);
            SDL_RenderTexture(state.renderer, actionTex, nullptr, &actionDst);
            SDL_DestroyTexture(actionTex);

            // Current key text centered in Key column
            const char* keyName = SDL_GetScancodeName(gs.keyBindings[i]);
            SDL_Color keyTextColor = (gs.rebindingAction == i) ? SDL_Color{255, 255, 255, 255} : SDL_Color{0, 0, 0, 255};
            SDL_Surface *keySurf = TTF_RenderText_Blended(res.UI.font, keyName, strlen(keyName), keyTextColor);
            SDL_Texture *keyTex = SDL_CreateTextureFromSurface(state.renderer, keySurf);
            SDL_DestroySurface(keySurf);
            float kw, kh;
            SDL_GetTextureSize(keyTex, &kw, &kh);
            float keyTextX = col2X + 50 - kw / 2;
            float keyTextY = rowY + (rowHeight - kh) / 2;
            SDL_FRect keyDst = MakeRect(keyTextX, keyTextY, kw, kh);
            SDL_RenderTexture(state.renderer, keyTex, nullptr, &keyDst);
            SDL_DestroyTexture(keyTex);

            // Change to text if rebinding and key pressed
            if(gs.rebindingAction == i && !gs.newKeyName.empty()){
                SDL_Surface *changeSurf = TTF_RenderText_Blended(res.UI.bolderFont, gs.newKeyName.c_str(), gs.newKeyName.length(), SDL_Color{255, 255, 255, 255});
                SDL_Texture *changeTex = SDL_CreateTextureFromSurface(state.renderer, changeSurf);
                SDL_DestroySurface(changeSurf);
                float cw, ch;
                SDL_GetTextureSize(changeTex, &cw, &ch);
                float changeTextX = col3X + 50 - cw / 2;
                float changeTextY = rowY + (rowHeight - ch) / 2;
                SDL_FRect changeDst = MakeRect(changeTextX, changeTextY, cw, ch);
                SDL_RenderTexture(state.renderer, changeTex, nullptr, &changeDst);
                SDL_DestroyTexture(changeTex);
            }
        }
    }

    if(T == CurrentInterface::GAME_OVER){
        if(!SDL_CursorVisible()) SDL_ShowCursor();
        SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(state.renderer);
        
        // Render "YOU DIED" text
        SDL_Color whiteText = {255, 255, 255, 255};
        SDL_Surface *diedSurf = TTF_RenderText_Blended(res.UI.bolderFont, "YOU DIED", strlen("YOU DIED"), whiteText);
        SDL_Texture *diedTexture = SDL_CreateTextureFromSurface(state.renderer, diedSurf);
        float diedW, diedH;
        SDL_GetTextureSize(diedTexture, &diedW, &diedH);
        SDL_FRect diedDst = MakeRect(state.logW / 2.0f - diedW / 2.0f, state.logH / 2.0f - 60.0f, diedW, diedH);
        SDL_RenderTexture(state.renderer, diedTexture, nullptr, &diedDst);
        SDL_DestroyTexture(diedTexture);
        SDL_DestroySurface(diedSurf);
        
        // Position RESTART and MENU buttons
        Button &restart = res.UI.buttons[res.UI.Buttons::RESTART], &menu = res.UI.buttons[res.UI.Buttons::MENU_BTN];
        res.UI.ResizeButton(restart, MakeRect(state.logW / 2.0f - 80.0f, state.logH / 2.0f + 20.0f, restart.txt_dim.x + 40.0f, restart.h));
        res.UI.ResizeButton(menu, MakeRect(state.logW / 2.0f + 10.0f, state.logH / 2.0f + 20.0f, menu.txt_dim.x + 40.0f, menu.h));
    }

    if(T == CurrentInterface::COMPLETE){
        if(!SDL_CursorVisible()) SDL_ShowCursor();
        SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(state.renderer);
        
        // Render "Congrats! You killed all enemies" text
        SDL_Color whiteText = {255, 255, 255, 255};
        const char* congratsText = "Congrats! You killed all enemies";
        SDL_Surface *congratsSurf = TTF_RenderText_Blended(res.UI.bolderFont, congratsText, strlen(congratsText), whiteText);
        SDL_Texture *congratsTexture = SDL_CreateTextureFromSurface(state.renderer, congratsSurf);
        float congratsW, congratsH;
        SDL_GetTextureSize(congratsTexture, &congratsW, &congratsH);
        SDL_FRect congratsDst = MakeRect(state.logW / 2.0f - congratsW / 2.0f, state.logH / 2.0f - 60.0f, congratsW, congratsH);
        SDL_RenderTexture(state.renderer, congratsTexture, nullptr, &congratsDst);
        SDL_DestroyTexture(congratsTexture);
        SDL_DestroySurface(congratsSurf);
        
        // Position RESTART and MENU buttons
        Button &restart = res.UI.buttons[res.UI.Buttons::RESTART], &menu = res.UI.buttons[res.UI.Buttons::MENU_BTN];
        res.UI.ResizeButton(restart, MakeRect(state.logW / 2.0f - 80.0f, state.logH / 2.0f + 20.0f, restart.txt_dim.x + 40.0f, restart.h));
        res.UI.ResizeButton(menu, MakeRect(state.logW / 2.0f + 10.0f, state.logH / 2.0f + 20.0f, menu.txt_dim.x + 40.0f, menu.h));
    }

    // Draw buttons
    for(auto &b : res.UI.buttons){
        drawButton(state, res, b);
    }

    // Handle modals
    if(currentModal != ModalType::NONE){
        RenderDimOverlay(state.renderer, state.logW, state.logH);
        if(currentModal == ModalType::CONFIRM_EXIT){
            SDL_FRect panel = {
                state.logW * 0.25f,
                state.logH * 0.35f,
                state.logW * 0.5f,
                state.logH * 0.3f
            };

            SDL_SetRenderDrawColor(state.renderer, 40, 40, 40, 255);
            SDL_RenderFillRect(state.renderer, &panel);
            SDL_SetRenderDrawColor(state.renderer, 200, 200, 200, 255);
            SDL_RenderRect(state.renderer, &panel);
            SDL_FRect msgDst = {
                panel.x + panel.w / 2.0f - static_cast<float>(res.UI.textures[res.UI.Textures::QUIT_MSG]->w) / 2.0f,
                panel.y + 20.0f,
                static_cast<float>(res.UI.textures[res.UI.Textures::QUIT_MSG]->w),
                static_cast<float>(res.UI.textures[res.UI.Textures::QUIT_MSG]->h)
            };
            SDL_RenderTexture(state.renderer, res.UI.textures[res.UI.Textures::QUIT_MSG], nullptr, &msgDst);
            Button &yes = res.UI.buttons[res.UI.Buttons::YES], &no = res.UI.buttons[res.UI.Buttons::NO]; 
            res.UI.ResizeButton(yes, MakeRect(panel.x + 40.0f, panel.y + panel.h - 40.0f, yes.txt_dim.x + 40.0f, yes.h));
            res.UI.ResizeButton(no, MakeRect(panel.x + panel.w - 80.0f, panel.y + panel.h - 40.0f, no.txt_dim.x + 40.0f, no.h));
            drawButton(state, res, yes);
            drawButton(state, res, no);
        }
        else if(currentModal == ModalType::INVALID){
            SDL_FRect panel = {
                state.logW * 0.2f,
                state.logH * 0.35f,
                state.logW * 0.6f,
                state.logH * 0.3f
            };

            SDL_SetRenderDrawColor(state.renderer, 40, 40, 40, 255);
            SDL_RenderFillRect(state.renderer, &panel);
            SDL_SetRenderDrawColor(state.renderer, 200, 200, 200, 255);
            SDL_RenderRect(state.renderer, &panel);
            SDL_FRect msgDst = {
                panel.x + panel.w / 2.0f - static_cast<float>(res.UI.textures[res.UI.Textures::INVALID_MSG]->w) / 2.0f,
                panel.y + 20.0f,
                static_cast<float>(res.UI.textures[res.UI.Textures::INVALID_MSG]->w),
                static_cast<float>(res.UI.textures[res.UI.Textures::INVALID_MSG]->h)
            };
            SDL_RenderTexture(state.renderer, res.UI.textures[res.UI.Textures::INVALID_MSG], nullptr, &msgDst);
            Button &ok = res.UI.buttons[res.UI.Buttons::OK];
            res.UI.ResizeButton(ok, MakeRect(panel.x + panel.w / 2.0f - ok.txt_dim.x / 2.0f - 25.0f, panel.y + panel.h - 40.0f, ok.txt_dim.x + 40.0f, ok.h));
            drawButton(state, res, ok);
        }
        else if(currentModal == ModalType::PAUSE){
            if(!SDL_CursorVisible()) SDL_ShowCursor();
            RenderDimOverlay(state.renderer, state.logW, state.logH);
            for(auto &b : res.UI.buttons){
                drawButton(state, res, b);
            }
            // Shift pause slider down so it does not overlap pause menu buttons
            RenderSlider(state, gs, res, mousePosx, mousePosy, glm::vec2(50.0f, 0.0f), 0.5f);
        }
        
    }

    // Update cursor
    for(auto &b : res.UI.buttons){
        if(b.state == buttonState::hidden || b.state == buttonState::inactive) continue;
        b.hovered = PointInRect(mousePosx, mousePosy, MakeRect(b.pos.x, b.pos.y, b.w, b.h));
        if(b.hovered){
            b.box = glm::vec3(220, 220, 220);
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER));
            break;
        }
        else{
            SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT));
            b.box = glm::vec3(255, 255, 255);
        }
    }
}

void HandleSliderUpdate(const SDL_Event &e, const SDLState &state, GameState &gs, ma_sound &music){
    float mx, my;
    if((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_MOTION) && e.button.button == SDL_BUTTON_LEFT){
        SDL_GetMouseState(&mx, &my);
        SDL_RenderCoordinatesFromWindow(state.renderer, mx, my, &mx, &my);
        if(PointInRect(mx, my, gs.musicSlider.knob)){
            gs.musicSlider.dragging = true;
            //SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "E", SDL_GetError(), state.window);
        }
        else if(PointInRect(mx, my, gs.musicSlider.track)){
            gs.musicSlider.val = glm::clamp((mx - gs.musicSlider.track.x) / gs.musicSlider.track.w, 0.0f, 1.0f);
            updateSliderKnob(gs.musicSlider);
            ma_sound_set_volume(&music, gs.musicSlider.val);
        }
        if(gs.musicSlider.dragging){
            float minX = gs.musicSlider.track.x, maxX = minX + gs.musicSlider.track.w, clampedX = glm::clamp(mx, minX, maxX);
            gs.musicSlider.val = (clampedX - minX) / gs.musicSlider.track.w;
            updateSliderKnob(gs.musicSlider);
            ma_sound_set_volume(&music, gs.musicSlider.val);
        }
    }
    else if(e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT){
        gs.musicSlider.dragging = false;
    }
}

void RenderSlider(const SDLState &state, GameState &gs, Resource &res, float mousePosx, float mousePosy, glm::vec2 padding, float scaleFactor){
    gs.musicSlider = SliderInit(state, padding, scaleFactor, gs.musicSlider.val, gs.musicSlider.dragging);
    updateSliderKnob(gs.musicSlider);
    SDL_SetRenderDrawColor(state.renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(state.renderer, &gs.musicSlider.track);
    SDL_FRect filled = gs.musicSlider.track, txtDst = {gs.musicSlider.track.x, gs.musicSlider.track.y - 30,
            static_cast<float>(res.UI.textures[res.UI.Textures::VOLUME_SLIDER_TXT]->w), static_cast<float>(res.UI.textures[res.UI.Textures::VOLUME_SLIDER_TXT]->h)};
    SDL_RenderTexture(state.renderer, res.UI.textures[res.UI.Textures::VOLUME_SLIDER_TXT], nullptr, &txtDst);
    filled.w *= gs.musicSlider.val;
    SDL_SetRenderDrawColor(state.renderer, 120, 120, 255, 255);
    SDL_RenderFillRect(state.renderer, &filled);
    SDL_SetRenderDrawColor(state.renderer, 230, 230, 230, 255);
    SDL_RenderFillRect(state.renderer, &gs.musicSlider.knob);
    if(PointInRect(mousePosx, mousePosy, gs.musicSlider.knob) || PointInRect(mousePosx, mousePosy, gs.musicSlider.track) || gs.musicSlider.dragging){
        SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER));
    }
    else{
        SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT));
    }
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
    if(TTF_Init() == -1){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initializing SDL_ttf", nullptr);
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
        if(gs.debugMode && obj.Tar == ObjectTarget::playable){
        SDL_FRect rectA{
        .x = obj.pos.x + obj.hitbox.x - gs.MapViewport.x,
        .y = obj.pos.y + obj.hitbox.y,
        .w = obj.hitbox.w,
        .h = obj.hitbox.h
        };
        SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 150);
        SDL_RenderRect(state.renderer, &rectA);
        SDL_RenderRect(state.renderer, &gs.MapViewport);
        SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);
    }
}

void drawButton(const SDLState &state, Resource &res, Button &b){
        if(b.state == buttonState::hidden) return;
        SDL_FRect brdr = MakeRect(b.pos.x, b.pos.y, b.w, b.h), dst = MakeRect(b.pos.x, b.pos.y, b.w, b.h);
        SDL_SetTextureColorModFloat(b.texture, b.box.r / 255.0f, b.box.g / 255.0f, b.box.b / 255.0f);
        SDL_RenderTexture(state.renderer, b.texture, NULL, &dst);
        SDL_SetRenderDrawColor(state.renderer, b.border.r, b.border.g, b.border.b, b.border.a);
        SDL_RenderRect(state.renderer, &brdr);
        dst.w = b.txt_dim.x;
        dst.h = b.txt_dim.y;
        dst.x = b.pos.x + b.w / 2 - dst.w / 2;
        dst.y = b.pos.y + b.h / 2 - dst.h / 2;
        SDL_RenderTexture(state.renderer, b.txtTexture, NULL, &dst);
        SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 255);
        
}

void HandleButtonEvent(Button &btn, const SDL_Event &e){
    if(btn.state != buttonState::active) return;
    if(e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT){
        if(btn.hovered){
            btn.pressed = true;
        }
    }

    if(e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT){
        if(btn.pressed && btn.hovered) {
            if(btn.onClick){
                btn.onClick(btn);
            }
        }
        btn.pressed = false;
    }
}


void update(const SDLState &state, GameState &gs,GameObject &obj, Resource &res, float timeDelta, ma_engine engine){
    if(currentModal == ModalType::PAUSE) return;
    if(obj.curAnimation != -1) obj.animations[obj.curAnimation].step(timeDelta);
    if(obj.dynamic && !obj.grounded) obj.vel += glm::vec2(0, 400) * timeDelta; // gravity
    float curDir = 0;
    if(obj.type == ObjectType::player){
        if(state.keys[gs.keyBindings[static_cast<int>(KeyAction::MOVE_LEFT)]]){
            curDir += -1;
        }
        if(state.keys[gs.keyBindings[static_cast<int>(KeyAction::MOVE_RIGHT)]]){
            curDir += 1;
        }
        Timer &weaponTimer = obj.data.player.WeaponTimer;
        weaponTimer.step(timeDelta);
        const auto handleShooting = [&state, &gs, &res, &obj, &weaponTimer, &engine](SDL_Texture *tex, SDL_Texture *shootTex, int AnimIndex, int ShootAnimIndex){
            if(state.keys[gs.keyBindings[static_cast<int>(KeyAction::SHOOT)]]){
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
                    bullet.texture = res.textures[Resource::BULLET];
                    bullet.maxSpeedX = 1000.0f;
                    bullet.curAnimation = res.BulletAnimationType::bulletMoving;
                    bullet.hitbox = SDL_FRect{
                        .x = 0,
                        .y = 0,
                        .w = static_cast<float>(res.textures[Resource::BULLET]->h),
                        .h = static_cast<float>(res.textures[Resource::BULLET]->h),
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
                handleShooting(res.textures[res.Textures::IDLE], res.textures[Resource::SHOOT], res.PlayerAnimationType::playerIdle, res.PlayerAnimationType::playerShooting);
                break;
            }
            case PlayerState::running:
            {
                if(!curDir){
                    obj.data.player.state = PlayerState::idle;
                }
                if(obj.vel.x * obj.dir < 0 && obj.grounded){
                    handleShooting(res.textures[Resource::SLIDE], res.textures[Resource::SLIDE_SHOOT], res.PlayerAnimationType::playerSliding, res.PlayerAnimationType::playerSlideShooting);
                }
                
                else{
                    handleShooting(res.textures[Resource::RUN], res.textures[Resource::RUN_SHOOT], res.PlayerAnimationType::playerRunning, res.PlayerAnimationType::playerRunning);
                }
                break;
            }
            case PlayerState::jumping:
            {
                handleShooting(res.textures[Resource::RUN], res.textures[Resource::RUN_SHOOT], res.PlayerAnimationType::playerRunning, res.PlayerAnimationType::playerRunning);
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
                    obj.acc = glm::vec2(40, 0);
                }
                else{
                    obj.acc.x = 0;
                    obj.vel.x = 0;
                }
                break;
            }
            case enemyState::damaged:
            {
                if(obj.data.enemy.dmgDuration.step(timeDelta)){
                    obj.data.enemy.state = enemyState::shambling;
                    obj.texture = res.textures[Resource::ENEMY];
                    obj.curAnimation = res.EnemyAnimationType::enemyIdle;
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
    ProcessMovementForObject(state, gs, obj, timeDelta, res, engine);
}

static void OnCollisionBegin(GameState &gs, GameObject &a, GameObject &b, Resource &res, ma_engine &engine, float timeDelta, const SDLState &state)
{
    auto *player = (a.type == ObjectType::player) ? &a : (b.type == ObjectType::player ? &b : nullptr);
    auto *enemy  = (a.type == ObjectType::enemy)  ? &a : (b.type == ObjectType::enemy  ? &b : nullptr);
    auto *level  = (a.type == ObjectType::level)  ? &a : (b.type == ObjectType::level  ? &b : nullptr);
    auto *bullet  = (a.type == ObjectType::bullet)  ? &a : (b.type == ObjectType::bullet  ? &b : nullptr);
    if (player && enemy) {

        if (enemy->data.enemy.contactRate.step(timeDelta)) {
            player->data.player.HP = SDL_max(0, player->data.player.HP - 15);
            ma_engine_play_sound(&engine, "resources/sound/Hit.wav", NULL);
        }
        a.flashes = true;
    }
    else if (bullet && bullet->data.bullet.state == BulletState::moving) {
        if (level) {
            ma_engine_play_sound(&engine, "resources/sound/shoot_hit.wav", NULL);
        } 
        else if (enemy) {
            if (enemy->data.enemy.state != enemyState::dead) {
                enemy->dir = -bullet->dir;
                enemy->flashes = true;
                enemy->flashTimer.reset();
                enemy->texture = res.textures[Resource::ENEMY_HIT];
                enemy->curAnimation = res.EnemyAnimationType::enemyDamaged;
                enemy->data.enemy.state = enemyState::damaged;
                enemy->data.enemy.contactRate.prime();
                enemy->data.enemy.HP -= 10;
                if (enemy->data.enemy.HP <= 0) {
                    enemy->data.enemy.state = enemyState::dead;
                    enemy->texture = res.textures[Resource::ENEMY_DIE];
                    enemy->curAnimation = res.EnemyAnimationType::enemyDying;
                    ma_engine_play_sound(&engine, "resources/sound/monster_die.wav", NULL);
                    gs.enemiesKilled++;
                }
                ma_engine_play_sound(&engine, "resources/sound/enemy_hit.wav", NULL);
            }
        }
    }
}

static void OnCollisionPersist(GameState &gs, GameObject &a, GameObject &b, Resource &res, ma_engine &engine, float timeDelta, const SDLState &state)
{
    auto *player = (a.type == ObjectType::player) ? &a : (b.type == ObjectType::player ? &b : nullptr);
    auto *enemy  = (a.type == ObjectType::enemy)  ? &a : (b.type == ObjectType::enemy  ? &b : nullptr);
    if (player && enemy) {
        if (enemy->data.enemy.contactRate.step(timeDelta)) {
            player->data.player.HP = SDL_max(0, player->data.player.HP - 15);
            ma_engine_play_sound(&engine, "resources/sound/Hit.wav", NULL);
            player->flashes = true;
        }
    }
}

static void ResolveOverlapX(GameObject &a, const SDL_FRect &intersect)
{
    if (intersect.w <= 0.0f) return;
    if (a.vel.x > 0.0f) {
        a.pos.x -= intersect.w;
    } else if (a.vel.x < 0.0f) {
        a.pos.x += intersect.w;
    }
    a.vel.x = 0.0f;
}

static void ResolveOverlapY(GameObject &a, const SDL_FRect &intersect)
{
    if (intersect.h <= 0.0f) return;
    if (a.vel.y > 0.0f) {
        a.pos.y -= intersect.h;
        a.vel.y = 0.0f;
        a.grounded = true; 
    } else if (a.vel.y < 0.0f) {
        a.pos.y += intersect.h;
        a.vel.y = 0.0f;
    }
}
void CollisionDetectionAxis(const SDLState &state, GameState &gs, GameObject &a, GameObject &b,
                            const SDL_FRect &rectA, const SDL_FRect &rectB, Axis axis,
                            float timeDelta, Resource &res, ma_engine &engine, bool &flag)
{
    SDL_FRect intersection;
    auto *player = (a.type == ObjectType::player) ? &a : (b.type == ObjectType::player ? &b : nullptr);
    auto *enemy  = (a.type == ObjectType::enemy)  ? &a : (b.type == ObjectType::enemy  ? &b : nullptr);
    auto *level  = (a.type == ObjectType::level)  ? &a : (b.type == ObjectType::level  ? &b : nullptr);
    auto *bullet  = (a.type == ObjectType::bullet)  ? &a : (b.type == ObjectType::bullet  ? &b : nullptr);
    if (!GetRectIntersectionSafe(rectA, rectB, intersection)) return;

    if(flag){
        OnCollisionBegin(gs, a, b, res, engine, timeDelta, state);
        flag = false;
    } else {
        OnCollisionPersist(gs, a, b, res, engine, timeDelta, state);
    }

    if (axis == Axis::X) {
        ResolveOverlapX(a, intersection);
    } else { // Axis::Y
        ResolveOverlapY(a, intersection);
    }
    if (bullet) {
        bool passesThrough = false;
        if (bullet->data.bullet.state == BulletState::moving) {
            if (enemy && enemy->data.enemy.state == enemyState::dead) {
                passesThrough = true;
                bullet->data.bullet.state = BulletState::colliding;
            }
            if (!passesThrough) {
                bullet->vel = glm::vec2{0.0f, 0.0f};
                bullet->data.bullet.state = BulletState::colliding;
                bullet->texture = res.textures[Resource::BULLET_HIT];
                bullet->curAnimation = res.BulletAnimationType::bulletHit;
            }
        }
    }
}

void ProcessMovementForObject(const SDLState &state, GameState &gs, GameObject &obj, float dt, Resource &res, ma_engine &engine)
{
    obj.pos.x += obj.vel.x * dt;
    bool flag = true;
    SDL_FRect rectA = MakeWorldRect(obj);

    for (auto &layer : gs.layers) {
        for (GameObject &other : layer) {
            if (&other == &obj) continue;
            if (other.Tar != ObjectTarget::playable) continue;
            if (obj.type == ObjectType::enemy && other.type == ObjectType::enemy) continue;

            SDL_FRect rectB = MakeWorldRect(other);
            CollisionDetectionAxis(state, gs, obj, other, rectA, rectB, Axis::X, dt, res, engine, flag);
        }
    }

    obj.pos.y += obj.vel.y * dt;
    obj.grounded = false; 

    rectA = MakeWorldRect(obj);

    for (auto &layer : gs.layers) {
        for (GameObject &other : layer) {
            if (&other == &obj) continue;
            if (other.Tar != ObjectTarget::playable) continue;
            if (obj.type == ObjectType::enemy && other.type == ObjectType::enemy) continue;

            SDL_FRect rectB = MakeWorldRect(other);
            CollisionDetectionAxis(state, gs, obj, other, rectA, rectB, Axis::Y, dt, res, engine, flag);
        }
    }

    {
        SDL_FRect feetSensor{
            obj.pos.x + obj.hitbox.x,
            obj.pos.y + obj.hitbox.y + obj.hitbox.h,
            obj.hitbox.w,
            2.0f
        };
        for (auto &layer : gs.layers) {
            for (GameObject &other : layer) {
                if (&other == &obj) continue;
                if (other.Tar != ObjectTarget::playable) continue;
                SDL_FRect otherRect = MakeWorldRect(other);
                SDL_FRect tmp;
                if (GetRectIntersectionSafe(feetSensor, otherRect, tmp, 0.01f)) {
                    // ensure we are not moving up
                    if (obj.vel.y >= 0.0f) {
                        obj.grounded = true;
                    }
                }
            }
        }
    }
}

bool createTiles(const SDLState &state, GameState &gs, Resource &res){
    /*
        1- Ground
        2- Panel
        3- Enemy
        4- Player
        5- Grass
        6- Brick
    */
    short mapData[MAX_ROWS][MAX_COLS] = {
        2, 4, 0, 3, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		2, 0, 0, 0, 0, 3, 0, 2, 2, 0, 3, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
		2, 0, 0, 0, 0, 2, 0, 0, 2, 2, 2, 2, 0, 2, 2, 2, 0, 0, 0, 2, 2, 2, 2, 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
		2, 2, 0, 0, 2, 2, 2, 0, 3, 0, 3, 3, 0, 3, 0, 2, 2, 2, 2, 2, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2,
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
        const auto createObj = [&state](SDL_Texture *tex, int r, int c, ObjectType type, ObjectTarget T = ObjectTarget::playable){
        GameObject obj;
        obj.type = type;
        obj.Tar = T;
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
                        GameObject ground = createObj(res.textures[Resource::GROUND], r, c, ObjectType::level);
                        gs.layers[LAYER_LEVEL_IDX].push_back(ground);
                        break;
                        }
                    case 2:
                        {
                        GameObject panel = createObj(res.textures[Resource::PANEL], r, c, ObjectType::level);
                        gs.layers[LAYER_LEVEL_IDX].push_back(panel);
                        break;
                        }
                    case 3:
                        {
                            GameObject enem = createObj(res.textures[Resource::ENEMY], r, c, ObjectType::enemy);
                            enem.data.enemy = EnemyData();
                            enem.curAnimation = res.EnemyAnimationType::enemyIdle;
                            enem.animations = res.animationsEnemy;
                            enem.dynamic = true;
                            enem.maxSpeedX = 15.0f;
                            enem.hitbox = SDL_FRect{
                                .x = 10,
                                .y = 4,
                                .w = 18,
                                .h = 28
                            };
                            gs.layers[LAYER_CHARACTER_IDX].push_back(enem);
                            gs.totalEnemies++;
                            break;
                        }
                    case 5:
                        {
                        GameObject grass = createObj(res.textures[Resource::GRASS], r, c, ObjectType::level, ObjectTarget::foreground);
                        // gs.ForegroundTile.push_back(grass);
                        gs.layers[LAYER_LEVEL_IDX].push_back(grass);
                        break;
                        }
                    case 6:
                        {
                        GameObject brick = createObj(res.textures[Resource::BRICK], r, c, ObjectType::level, ObjectTarget::background);
                        // gs.BackgroundTile.push_back(brick);
                        gs.layers[LAYER_LEVEL_IDX].push_back(brick);
                        break;
                        }
                    case 4:
                        {
                        GameObject player = createObj(res.textures[res.Textures::IDLE], r, c, ObjectType::player);
                        player.data.player = PlayerData();
                        player.animations = res.animationsPlayer;
                        player.curAnimation = res.PlayerAnimationType::playerIdle;
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
    gs.entitiesLoaded = true;
    assert(gs.playerIdx != -1);
    return true;
}

void HandleKey(const SDLState &state, GameState &gs, Resource &res, GameObject &obj, SDL_Scancode key, bool pressed){
    if(T != CurrentInterface::GAME) return;
    if(key == SDL_SCANCODE_ESCAPE){
        res.UI.SetScene(CurrentInterface::GAME, ModalType::PAUSE);
    }
    float JUMP_AMT = -200.00f;
    if(obj.type == ObjectType::player){
        switch(obj.data.player.state){
            case PlayerState::idle:
            {
                if(key == gs.keyBindings[static_cast<int>(KeyAction::JUMP)] && pressed){
                    obj.vel.y += JUMP_AMT;
                    obj.data.player.state = PlayerState::jumping;
                }
                break;
            }
            case PlayerState::running:
            {
                if(key == gs.keyBindings[static_cast<int>(KeyAction::JUMP)] && pressed){
                    obj.vel.y += JUMP_AMT;
                    obj.data.player.state = PlayerState::jumping;
                }
                break;
            }
        }
    }
}

void DrawParallaxBackground(SDL_Renderer *renderer, SDL_Texture *tex, float xVel, float &scrollPos, float scrollFact, float timeDelta){
    scrollPos -= xVel * scrollFact * timeDelta;
    if(scrollPos <= -tex->w) scrollPos = 0;
    SDL_FRect where{
        .x = scrollPos,
        .y = 30,
        .w = tex->w * 2.5f,
        .h = static_cast<float>(tex->h)
    };
    SDL_RenderTextureTiled(renderer, tex, nullptr, 1, &where);
}

void DrawBackground(SDL_Renderer *renderer, const SDLState &state, GameState &gs, Resource &res, float xVel, float timeDelta){
    SDL_RenderTexture(state.renderer, res.textures[Resource::BCKGRND1], nullptr, nullptr);
    DrawParallaxBackground(state.renderer, res.textures[Resource::BCKGRND4], xVel, gs.bg4scroll, 0.075f, timeDelta);
    DrawParallaxBackground(state.renderer, res.textures[Resource::BCKGRND3], xVel, gs.bg3scroll, 0.15f, timeDelta);
    DrawParallaxBackground(state.renderer, res.textures[Resource::BCKGRND2], xVel, gs.bg2scroll, 0.3f, timeDelta);
}