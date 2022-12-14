#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include "map.h"
#include "cants_config.h"

#define scp(pointer, message) {                                               \
    if (pointer == NULL) {                                                    \
        SDL_Log("Error: %s! SDL_Error: %s", message, SDL_GetError()); \
        exit(1);                                                              \
    }                                                                         \
}

#define scc(code, message) {                                                  \
    if (code < 0) {                                                           \
        SDL_Log("Error: %s! SDL_Error: %s", message, SDL_GetError()); \
        exit(1);                                                              \
    }                                                                         \
}

// turn an integer literal into a string literal
#define STR_IMPL_(x) #x      //stringify argument
#define STR(x) STR_IMPL_(x)  //indirection to expand argument macros

#define imgcp(pointer, message) { if (pointer == NULL) {SDL_Log("Error: %s! IMG_Error: %s", message, IMG_GetError()); exit(1);}}
#define imgcc(code, message) { if (code < 0) {SDL_Log("Error: %s! IMG_Error: %s", message, IMG_GetError()); exit(1);}}
#define ttfcp(pointer, message) { if (pointer == NULL) {SDL_Log("Error: %s! TTF_Error: %s", message, TTF_GetError()); exit(1);}}
#define ttfcc(code, message) { if (code < 0) {SDL_Log("Error: %s! TTF_Error: %s", message, TTF_GetError()); exit(1);}}

#define emod(a, b) (((a) % (b)) + (b)) % (b)

int screen_width = 1920;
int screen_height = 1080;
int level_width = 3000;
int level_height = 3000;
const Uint32 ANT_ANIM_MS = 100;
const Uint32 ANT_MS_TO_MOVE = 10;
const int ANT_VEL_MAX = 2;
const int ANT_TURN_DEGREES = 1;
const int CELL_SIZE = 50;
const int ANT_STEP_LEN = CELL_SIZE;
const int TILES_PER_FOOD = 90;

enum ANT_STATES {ANT_STATE_PREPARE, ANT_STATE_TURN, ANT_STATE_STEP};
#if TUTORIAL
enum TUTORIAL_STAGES {TUTORIAL_LEAVES, TUTORIAL_UPGRADE, TUTORIAL_TEN, TUTORIAL_DONE};
#endif

//Texture - an SDL_Texture with additional information
typedef struct {
    SDL_Texture *texture_proper;
    int width;
    int height;
} Texture;

//Ant structs hold information needed to draw any ant
//Player and Npc structs are used to calculate motion. There is a sort of 'inheritance' from Ant

typedef struct {
    int8_t frame;
    Uint32 anim_time;
    float x;
    float y;
    int angle;
    float scale;
} Ant;

typedef struct {
    Ant *ant;
    enum ANT_STATES state;
    int target_angle;
    int8_t cw;
    int steps_done;
    int gm_x; //game coordinates
    int gm_y;
    SDL_TimerID timer_id;
} Npc;

typedef struct {
    Ant *ant;
    int vel;
    int turn_vel;
    int width;
    int height;
    int food_count;
    bool in_anthill;
} Player;


typedef struct {
    int x;
    int y;
    int gm_x;
    int gm_y;
    int level;
} Anthill;

//////////////// GLOBALS ////////////////////////////////////////////////////////

SDL_Window* g_window;

SDL_Renderer* g_renderer;

Uint32 g_eventstart;

Texture g_leaf_texture;
Texture g_background_texture;
Texture g_ant_texture;
Texture g_food_count_texture;
Texture g_anthill_texture;
Texture g_anthill_icon_texture;
Texture g_anthill_level_texture;
Texture g_tutorial_prompt;

#if TUTORIAL
enum TUTORIAL_STAGES g_tutorial = TUTORIAL_LEAVES;
#endif
TTF_Font *g_font;

int g_world_food_count;

//Ant frames clip rects
#define ANT_FRAMES_NUM 4
SDL_Rect g_antframes[ANT_FRAMES_NUM];

//remember the inverted y axis
Point g_ant_move_table[8] = {
    {0, -1}, //0
    {1, -1}, //45
    {1,  0}, //90
    {1,  1}, //135
    {0,  1}, //180
    {-1, 1}, //225
    {-1, 0}, //270
    {-1,-1}  //315
};

#define MAX_LEVEL 10
//const int g_levels_table[MAX_LEVEL + 1] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200, 200};
const int g_levels_table[MAX_LEVEL + 1] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 100};

SDL_Rect g_camera = {
    0,
    0,
    0,
    0
};

size_t g_npc_sp;
Npc **g_npc_stack = NULL;

//////////////// FUNCTIONS //////////////////////////////////////////////////////

//create a dynamically allocated stack which holds ants and is used for rendering them all
Npc **init_npc_stack(void);
//push an npc onto npc stack
bool push_npc(Npc *npc);

//check collision of two axis aligned rectangles
bool check_collision(SDL_Rect x, SDL_Rect y);

void init(void) {

	scc(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER), "Could not initialize SDL");
    if( !SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "1" ) ) {
        SDL_Log("Warning: Linear texture filtering not enabled!");
    }
    ttfcc(TTF_Init(), "Could not initialize SDL_ttf");
#if ANDROID_BUILD
    scp((g_window = SDL_CreateWindow("Can'ts", 0, 0, screen_width, screen_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN)),
            "Could not create window");
#else
    scp((g_window = SDL_CreateWindow("Can'ts", 0, 0, screen_width, screen_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE)),
            "Could not create window");
#endif
    //TODO:some functions like this may fail
    SDL_GetWindowSize(g_window, &screen_width, &screen_height);
    g_camera.w = screen_width;
    g_camera.h = screen_height;
    //Create renderer for window
    scp((g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)),
            "Could not create renderer");

    int imgFlags = IMG_INIT_PNG;
    if(!( IMG_Init(imgFlags) & imgFlags)) {
        SDL_Log("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        exit(1);
    }
    //init stack with ant pointers
    if ((g_npc_stack = init_npc_stack()) == NULL) {
        SDL_Log("Error: Could not initialize ant stack!");
        exit(1);
    }
}

//return Texture struct
Texture load_texture(const char *path)
{
	//The final texture
	SDL_Texture *new_texture = NULL;
    SDL_Surface *loaded_surface = NULL;
    Texture texture_struct = {0};

	//Load image at specified path
	imgcp((loaded_surface = IMG_Load(path)), "Could not load image");
    //Create texture from surface pixels
    scp((new_texture = SDL_CreateTextureFromSurface(g_renderer, loaded_surface)), "Could not create texture from surface");

    texture_struct.texture_proper = new_texture;
    texture_struct.width = loaded_surface->w;
    texture_struct.height = loaded_surface->h;

    //Get rid of old loaded surface
    SDL_FreeSurface(loaded_surface);

	return texture_struct;
}

Texture load_text_texture(const char *text){
	SDL_Texture *new_texture = NULL;
    SDL_Surface *text_surface = NULL;

#define OUTLINE_SIZE 2
    /* load font and its outline */
    TTF_SetFontOutline(g_font, OUTLINE_SIZE);

    /* render text and text outline */
    SDL_Color white = {0xFF, 0xFF, 0xFF, 0xFF};
    SDL_Color black = {0x00, 0x00, 0x00, 0xFF};
    text_surface = TTF_RenderText_Blended(g_font, text, black);
    SDL_Surface *fg_surface = TTF_RenderText_Blended(g_font, text, white);
    SDL_Rect rect = {OUTLINE_SIZE, OUTLINE_SIZE, fg_surface->w, fg_surface->h};

    /* blit text onto its outline */
    SDL_SetSurfaceBlendMode(fg_surface, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(fg_surface, NULL, text_surface, &rect);
    SDL_FreeSurface(fg_surface);

    scp((new_texture = SDL_CreateTextureFromSurface(g_renderer, text_surface)), "Could not create texture from surface");

    const Texture texture_struct = {
        new_texture,
        text_surface->w,
        text_surface->h

    };
    SDL_FreeSurface(text_surface);

	return texture_struct;
}

void load_media() {
    g_ant_texture = load_texture(ASSETS_PREFIX"antspritesheet.png");
    for (int i = 0; i < ANT_FRAMES_NUM; i++) {
        g_antframes[i].x = g_ant_texture.width * i / ANT_FRAMES_NUM;
        g_antframes[i].y = 0;
        g_antframes[i].h = g_ant_texture.height;
        g_antframes[i].w = g_ant_texture.width / ANT_FRAMES_NUM;
    }
    g_background_texture = load_texture(ASSETS_PREFIX"grass500x500.png");
    //"https://www.freepik.com/vectors/cartoon-grass" Cartoon grass vector created by babysofja - www.freepik.com
    g_leaf_texture = load_texture(ASSETS_PREFIX"leaf.png");
    g_font = TTF_OpenFont(ASSETS_PREFIX"OpenSans-Regular.ttf", 50);
    assert(g_levels_table[0] == 10 && "wrong first level in a texture");
    g_food_count_texture = load_text_texture("0/10");
    g_anthill_texture = load_texture(ASSETS_PREFIX"anthill.png");
    g_anthill_icon_texture = load_texture(ASSETS_PREFIX"anthill_icon.png");
    g_anthill_level_texture = load_text_texture("1/"STR(MAX_LEVEL));
    g_tutorial_prompt = load_text_texture("Use WASD to move around and collect leaves");
}

void closesdl()
{
	//Free loaded image
	SDL_DestroyTexture(g_ant_texture.texture_proper);
    g_ant_texture.texture_proper = NULL;
    SDL_DestroyTexture(g_background_texture.texture_proper);
    g_background_texture.texture_proper = NULL;
    SDL_DestroyTexture(g_leaf_texture.texture_proper);
    g_background_texture.texture_proper = NULL;
    SDL_DestroyTexture(g_food_count_texture.texture_proper);
    g_background_texture.texture_proper = NULL;
    SDL_DestroyTexture(g_anthill_texture.texture_proper);
    g_background_texture.texture_proper = NULL;
    SDL_DestroyTexture(g_anthill_icon_texture.texture_proper);

	SDL_DestroyRenderer(g_renderer);
	SDL_DestroyWindow(g_window);
	g_window = NULL;
	g_renderer = NULL;

    TTF_CloseFont(g_font);
	//Quit SDL subsystems
	IMG_Quit();
    TTF_Quit();
	SDL_Quit();
}

Ant *create_ant(int x, int y) {
    Ant *ant = malloc(sizeof(Ant));
    if (ant == NULL) {
        return NULL;
    }
    memset((void *) ant, 0, sizeof(Ant));
    ant->anim_time = SDL_GetTicks();
    ant->x = x;
    ant->y = y;

    ant->scale = (double) rand() / RAND_MAX + 0.75;
#if DEBUGMODE
    SDL_Log("Ant #%ld created at x %d y %d\n", g_npc_sp, (int) ant->x, (int) ant->y);
#endif
    return ant;
}

void render_player_anim(Player *player) {
    if (SDL_GetTicks() - player->ant->anim_time > ANT_ANIM_MS && (player->vel != 0 || player->turn_vel != 0)) {
        player->ant->anim_time = SDL_GetTicks();
        player->ant->frame = (player->ant->frame + 1) % ANT_FRAMES_NUM;
    }
    SDL_Rect render_rect = {
        .x = player->ant->x - g_camera.x - g_ant_texture.width * player->ant->scale / ANT_FRAMES_NUM / 2,
        .y = player->ant->y - g_camera.y - g_ant_texture.height * player->ant->scale / 2,
        .w = g_antframes[0].w * player->ant->scale,
        .h = g_antframes[0].h * player->ant->scale,
    };
    SDL_RenderCopyEx(g_renderer, g_ant_texture.texture_proper, &g_antframes[player->ant->frame], &render_rect, player->ant->angle, NULL, SDL_FLIP_NONE);

}

void render_ant_anim(Ant *ant) {
    if (SDL_GetTicks() - ant->anim_time > ANT_ANIM_MS) {
        ant->anim_time = SDL_GetTicks();
        ant->frame = (ant->frame + 1) % ANT_FRAMES_NUM;
    }
    SDL_Rect render_rect = {
        .x = ant->x - g_camera.x - g_ant_texture.width * ant->scale / ANT_FRAMES_NUM / 2,
        .y = ant->y - g_camera.y - g_ant_texture.height * ant->scale / 2,
        .w = g_antframes[0].w * ant->scale,
        .h = g_antframes[0].h * ant->scale,
    };
    SDL_RenderCopyEx(g_renderer, g_ant_texture.texture_proper, &g_antframes[ant->frame], &render_rect, ant->angle, NULL, SDL_FLIP_NONE);
}

void render_texture(Texture texture, int x, int y) {
    SDL_Rect render_rect;
    render_rect.x = x;
    render_rect.y = y;
    render_rect.h = texture.height;
    render_rect.w = texture.width;
    SDL_RenderCopy(g_renderer, texture.texture_proper, NULL, &render_rect);
}

void render_texture_scaled(Texture texture, int x, int y, float scale) {
    SDL_Rect render_rect;
    render_rect.x = x;
    render_rect.y = y;
    render_rect.h = texture.height * scale;
    render_rect.w = texture.width * scale;
    SDL_RenderCopy(g_renderer, texture.texture_proper, NULL, &render_rect);
}

void set_camera(Player *player) {
    //Center the camera over the player
    g_camera.x = ((int) player->ant->x + g_ant_texture.width / (2 * ANT_FRAMES_NUM)) - screen_width / 2;
    g_camera.y = ((int) player->ant->y + g_ant_texture.height / 2) - screen_height / 2;

    //Keep the camera in bounds
    if(g_camera.x < 0) {
        g_camera.x = 0;
    }
    if(g_camera.y < 0) {
        g_camera.y = 0;
    }
    if(g_camera.x > level_width - g_camera.w) {
        g_camera.x = level_width - g_camera.w;
    }
    if(g_camera.y > level_height - g_camera.h) {
        g_camera.y = level_height - g_camera.h;
    }
}

void remove_food(int8_t *cell) {
    *cell = MAP_FREE;
    SDL_Event event;
    SDL_UserEvent userevent;
    event.type = SDL_USEREVENT;
    userevent.type = g_eventstart;
    event.user = userevent;
    SDL_PushEvent(&event);
}

Uint32 move_player(Uint32 interval, void *player_void) {
    Player *player = (Player *) player_void;

    if (player->vel >= 0)
        player->ant->angle += player->turn_vel;
    else
        player->ant->angle -= player->turn_vel;

    if (player->vel != 0) {
                                        //convert angle to radians
        float dx = cosf((player->ant->angle - 90) * M_PI / 180.0);
        float dy = sinf((player->ant->angle - 90) * M_PI / 180.0);
        player->ant->x += player->vel * dx;
        player->ant->y += player->vel * dy;
        //collision checks
        //TODO: accessing the map with the player outside of the map may segfault
        //Circular collision might be worth it
        int8_t *cell = &g_map.matrix[(int) player->ant->y / CELL_SIZE][(int) player->ant->x / CELL_SIZE];
        switch (*cell) {
            case MAP_FREE:
                player->in_anthill = false;
                break;
            case MAP_ANTHILL:
                player->in_anthill = true;
                /* FALLTHRU */
            case MAP_WALL:
                player->ant->x -= player->vel * dx;
                player->ant->y -= player->vel * dy;
                break;
            case MAP_FOOD:
                remove_food(cell);
                break;
        }
    }


    return interval;
}

void update_food_count_texture(int food_count, int next_level) {
    char str[22];
    sprintf(str, "%d/%d", food_count, next_level);
    SDL_DestroyTexture(g_food_count_texture.texture_proper);
    g_food_count_texture = load_text_texture(str);
}
void update_anthill_level_texture(int level) {
    char str[22];
    sprintf(str, "%d/%d", level, MAX_LEVEL);
    SDL_DestroyTexture(g_anthill_level_texture.texture_proper);
    g_anthill_level_texture = load_text_texture(str);
}

Uint32 move_npc(Uint32 interval, void *npc_void) {

    Npc *npc = (Npc *) npc_void;
    switch (npc->state) {
        case ANT_STATE_PREPARE:;

            Point target_cell = {-1, -1};

            for (int i = 0; i < 8; i++) {
                int gm_x = npc->gm_x + g_ant_move_table[i].x;
                int gm_y = npc->gm_y + g_ant_move_table[i].y;
                if (g_map.matrix[gm_y][gm_x] == MAP_FOOD) {
                    target_cell.x = gm_x;
                    target_cell.y = gm_y;
                    npc->target_angle = i * 45;
                }
            }
            if (target_cell.x == -1) {
                //no leaf, choose random cell
                do {
                int n = rand() % 8;
                Point random_offset = g_ant_move_table[n];
                npc->target_angle = n * 45;
                target_cell.x = npc->gm_x + random_offset.x;
                target_cell.y = npc->gm_y + random_offset.y;
                } 
                while (g_map.matrix[target_cell.y][target_cell.x] == MAP_WALL || g_map.matrix[target_cell.y][target_cell.x] == MAP_ANTHILL);
            }
            npc->gm_x = target_cell.x;
            npc->gm_y = target_cell.y;
            npc->steps_done = 0;

            if ((npc->ant->angle > npc->target_angle && npc->ant->angle - npc->target_angle > 180) ||
                    (npc->target_angle > npc->ant->angle && npc->target_angle - npc->ant->angle < 180)) npc->cw = 1;
            else npc->cw = -1;

            npc->state = ANT_STATE_TURN;

            //TODO: may segfault on boundary of the map
            break;

        case ANT_STATE_TURN:

            if (emod(npc->ant->angle, 360) != npc->target_angle) {
                npc->ant->angle = emod(npc->ant->angle + 5 * npc->cw, 360);
            }
            else
                npc->state = ANT_STATE_STEP;
            break;
        case ANT_STATE_STEP:
            if (npc->steps_done < ANT_STEP_LEN) {
                npc->steps_done++;
                float rad = (npc->ant->angle - 90) * M_PI / 180.0;
                if (npc->ant->angle % 90 == 0) {
                    npc->ant->x += cosf(rad);
                    npc->ant->y += sinf(rad);
                }
                else {
                    npc->ant->x += cosf(rad) * M_SQRT2;
                    npc->ant->y += sinf(rad) * M_SQRT2;
                }
            }
            else {
                //correction
                npc->ant->x = npc->gm_x * CELL_SIZE + (float) CELL_SIZE / 2;
                npc->ant->y = npc->gm_y * CELL_SIZE + (float) CELL_SIZE / 2;
                int8_t *cell = &g_map.matrix[(int) npc->ant->y / CELL_SIZE][(int) npc->ant->x / CELL_SIZE];
                if (*cell == MAP_FOOD) {
                    remove_food(cell);
                }
                npc->state = ANT_STATE_PREPARE;
            }
            break;
    }
    return interval;
}

Npc *create_npc(int gm_x, int gm_y) {
    Npc *npc = malloc(sizeof(Npc));
    if (npc == NULL) return NULL;
    npc->ant = create_ant((gm_x + 0.5) * CELL_SIZE, (gm_y + 0.5) * CELL_SIZE);
    if (npc->ant == NULL) {
        free(npc);
        SDL_Log("Warning: Could not allocate memory for an npc ant");
        return NULL;
    }
    if (!push_npc(npc)) {
        SDL_Log("Warning: could not push npc ant\n");
        free(npc->ant);
        free(npc);
        return NULL;
    }
    npc->gm_x = gm_x;
    npc->gm_y = gm_y;
    npc->state = ANT_STATE_PREPARE;
    SDL_AddTimer(ANT_MS_TO_MOVE, move_npc, (void *) npc);
    return npc;
}

void create_food(void) {
    SDL_Rect leaf_rect = { 
        .w = g_leaf_texture.width,
        .h = g_leaf_texture.height
    };
    Point point;
    do {
    point = find_random_free_spot_on_a_map();
    leaf_rect.x = point.x * CELL_SIZE;
    leaf_rect.y = point.y * CELL_SIZE;
    } while (check_collision(leaf_rect, g_camera));

    g_map.matrix[point.y][point.x] = MAP_FOOD;
    g_world_food_count++;
}

//coordinates of the entrance (where the ants spawn)
void init_anthill(Anthill *anthill) {

    anthill->level = 0;
    for (int i = 0; i < g_map.height; i++) {
        for (int j = 0; j < g_map.width; j++) {
            if (g_map.matrix[i][j] == MAP_ANTHILL) {
                anthill->gm_x = j + 1;
                anthill->gm_y = i;
                anthill->x = (anthill->gm_x - 1) * CELL_SIZE;
                anthill->y = (anthill->gm_y) * CELL_SIZE;
                return;
            }
        }
    }
    SDL_Log("The map does not contain an anthill\n");
    exit(1);
}


Texture win(void) {
Texture win_texture = load_text_texture("Congratulations! You won!");
return win_texture;
}

void render_game_objects(Player *player, Anthill *anthill) {
        SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0x00, 0xFF);
        SDL_RenderClear(g_renderer);

        //render background texture tiles (only those that are on the screen)
        for (int y = 0; y < level_height; y += g_background_texture.height) {
            for (int x = 0; x < level_width; x += g_background_texture.width) {
                SDL_Rect coords = {
                    x,
                    y,
                    g_background_texture.width,
                    g_background_texture.height
                };
                if (check_collision(coords, g_camera)) {
                    render_texture(g_background_texture, x - g_camera.x, y - g_camera.y);
                }
            }
        }

        render_player_anim(player);

        //render ants which are on the screen
        for (size_t i = 0; i < g_npc_sp; i++) {
            SDL_Rect coords = {
                g_npc_stack[i]->ant->x,
                g_npc_stack[i]->ant->y,
                g_ant_texture.width / ANT_FRAMES_NUM,
                g_ant_texture.height
            };
            if (check_collision(coords, g_camera)) {
                render_ant_anim(g_npc_stack[i]->ant);
            }
        }


        for (int i = g_camera.y / CELL_SIZE; i < (g_camera.y + g_camera.h + CELL_SIZE) / CELL_SIZE && i < g_map.height; i++) {
            for (int j = g_camera.x / CELL_SIZE; j < (g_camera.x + g_camera.w + CELL_SIZE) / CELL_SIZE && j < g_map.width; j++) {
                if (g_map.matrix[i][j] == MAP_WALL) {
                    SDL_Rect coords = {
                        j * CELL_SIZE - g_camera.x,
                        i * CELL_SIZE - g_camera.y,
                        CELL_SIZE,
                        CELL_SIZE
                    };
                    //TODO: compare SDL_RenderFillRect and SDL_FillRect speed
                    SDL_RenderFillRect(g_renderer, &coords);
                }
                else if (g_map.matrix[i][j] == MAP_FOOD) {
                    render_texture(g_leaf_texture, j * CELL_SIZE - g_camera.x, i * CELL_SIZE - g_camera.y);
                }
            }
        }
        //render anthill
        render_texture(g_anthill_texture, anthill->x - g_camera.x, anthill->y - g_camera.y);

        //draw HUD
        //TODO: maybe draw a single picture (png) instead of many rects (also would be more pretty if drawn nice)
        SDL_SetRenderDrawColor(g_renderer, 0x50, 0x50, 0x50, 0xFF);
        SDL_Rect hud = {0, screen_height * 14 / 15, screen_width, screen_height / 15};
        SDL_RenderFillRect(g_renderer, &hud);
        SDL_SetRenderDrawColor(g_renderer, 0x90, 0xCC, 0x90, 0xFF);
        SDL_Rect space_for_hud1 = {screen_width / 20, screen_height * 44 / 45 - g_food_count_texture.height / 2,
            screen_width * 19/ 20, g_leaf_texture.height};
        SDL_RenderFillRect(g_renderer, &space_for_hud1);
        render_texture(g_leaf_texture, screen_width / 20, screen_height * 34 / 35 - g_leaf_texture.height / 2);
        render_texture(g_food_count_texture, screen_width / 10, screen_height * 34 / 35 - g_food_count_texture.height / 2 - 5);

        render_texture(g_anthill_icon_texture, screen_width * 4 / 5, screen_height * 34 / 35 - g_anthill_icon_texture.height / 2);
        render_texture(g_anthill_level_texture, screen_width * 4 / 5 + g_anthill_icon_texture.width, screen_height * 34 / 35 - g_food_count_texture.height / 2 - 5);


#if TUTORIAL
        static int last_food_count;
        if (g_tutorial != TUTORIAL_DONE) {
            render_texture(g_tutorial_prompt, screen_width / 2 - g_tutorial_prompt.width / 2, 0);
            switch (g_tutorial) {
                case TUTORIAL_LEAVES:
                    if (player->food_count >= 10) {
                        g_tutorial++;
                        SDL_DestroyTexture(g_tutorial_prompt.texture_proper);
#if ANDROID_BUILD
                        g_tutorial_prompt = load_text_texture("Enter your anthill and tap on it to upgrade");
#else
                        g_tutorial_prompt = load_text_texture("Enter your anthill and press Space to upgrade");
#endif
                    }
                    break;
                case TUTORIAL_UPGRADE:
                    if (anthill->level > 0) {
                        g_tutorial++;
                        SDL_DestroyTexture(g_tutorial_prompt.texture_proper);
                        g_tutorial_prompt = load_text_texture("Now reach level "STR(MAX_LEVEL)"!");
                        last_food_count = player->food_count;
                    };
                    break;
                case TUTORIAL_TEN:
                    if (player->food_count > last_food_count) {
                        g_tutorial++;
                        SDL_DestroyTexture(g_tutorial_prompt.texture_proper);
                        g_tutorial_prompt.texture_proper = NULL;
                    }
                    break;
            }
        }
#endif
        
}

void toggle_fullscreen(void) {
    Uint32 FullscreenFlag = SDL_WINDOW_FULLSCREEN;
    bool IsFullscreen = SDL_GetWindowFlags(g_window) & FullscreenFlag;
    SDL_SetWindowFullscreen(g_window, IsFullscreen ? 0 : FullscreenFlag);
    SDL_ShowCursor(IsFullscreen);
}

bool is_in_rect(SDL_Rect *rect, int x, int y) {
    return rect->x <= x && x < rect->x + rect->w && rect->y <= y && y < rect->y + rect->h;
}

//menu returns map_path
char *menu(void) {
    SDL_SetRenderDrawColor(g_renderer, 0x00, 0x90, 0, 0xFF);
    Texture choose_map_prompt = load_text_texture("Choose a map");
    Texture map1thumb_texture = load_texture(ASSETS_PREFIX"map1thumb.png");
    Texture map2thumb_texture = load_texture(ASSETS_PREFIX"map2thumb.png");
    bool quit = false;
    char *map_path = NULL;
    SDL_Event event;
    float thumb_scale = (float) screen_width / (1920 * 2);
    SDL_Rect map1thumb = {
        (float) screen_width / 3 - map1thumb_texture.width * thumb_scale / 2,
        (float) screen_height / 2 - map1thumb_texture.height * thumb_scale / 2 ,
        map2thumb_texture.width * thumb_scale,
        map2thumb_texture.height * thumb_scale,
    };
    SDL_Rect map2thumb = {
        (float) screen_width * 2 / 3 - map1thumb_texture.width * thumb_scale / 2,
        (float) screen_height / 2 - map1thumb_texture.height * thumb_scale / 2 ,
        map2thumb_texture.width * thumb_scale,
        map2thumb_texture.height * thumb_scale,
    };

    while (!quit) {
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        g_camera.w = screen_width = event.window.data1;
                        g_camera.h = screen_height = event.window.data2;
                        thumb_scale = (float) screen_width / (1920 * 2);
#if ANDROID_BUILD
                        SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN);
#endif

                        thumb_scale = (float) screen_width / (1920 * 2);
                        map1thumb.x = (float) screen_width / 3 - map1thumb_texture.width * thumb_scale / 2;
                        map1thumb.y = (float) screen_height / 2 - map1thumb_texture.height * thumb_scale / 2;
                        map1thumb.w = map1thumb_texture.width * thumb_scale;
                        map1thumb.h = map1thumb_texture.height * thumb_scale;

                        map2thumb.x = (float) screen_width * 2 / 3 - map1thumb_texture.width * thumb_scale / 2;
                        map2thumb.y = (float) screen_height / 2 - map1thumb_texture.height * thumb_scale / 2;
                        map2thumb.w = map2thumb_texture.width * thumb_scale;
                        map2thumb.h = map2thumb_texture.height * thumb_scale;
                        }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                      if (event.button.button == SDL_BUTTON_LEFT) {
                          quit = true;
                          if (is_in_rect(&map1thumb, event.button.x, event.button.y)) {
                              map_path = ASSETS_PREFIX"map1.bin";
                          }
                          else if (is_in_rect(&map2thumb, event.button.x, event.button.y)) {
                              map_path = ASSETS_PREFIX"map2.bin";
                          }
                          else quit = false;
                      }
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_ESCAPE:
                        case SDL_SCANCODE_AC_BACK:
                            quit = true;
                            break;
                    }
                    break;
            }
        }
        SDL_RenderClear(g_renderer);

        for (int y = 0; y < screen_height; y += g_background_texture.height) {
            for (int x = 0; x < screen_width; x += g_background_texture.width) {
                render_texture(g_background_texture, x, y);
            }
        }

        render_texture(choose_map_prompt, screen_width / 2 - choose_map_prompt.width / 2, 0);
        render_texture_scaled(map1thumb_texture, map1thumb.x, map1thumb.y, thumb_scale);
        render_texture_scaled(map2thumb_texture, map2thumb.x, map2thumb.y, thumb_scale);

        SDL_RenderPresent(g_renderer);
    }
    SDL_DestroyTexture(choose_map_prompt.texture_proper);
    SDL_DestroyTexture(map1thumb_texture.texture_proper);
    SDL_DestroyTexture(map2thumb_texture.texture_proper);
    return map_path;
}

void destroy_npc(Npc *npc) {
    SDL_RemoveTimer(npc->timer_id);
    free(npc->ant);
    free(npc);
}

void destroy_map(Map *map) {
    for (size_t i = 0; i < map->height; i++) {
        free(map->matrix[i]);
    }
    free(map->matrix);
}

//////////////// MAIN ///////////////////////////////////////////////////////////


int main(int argc, char *argv[]) {
    srand(time(NULL));
    char *map_path;
    init();
    load_media();

    if (argc > 1)
        map_path = argv[1];
    else {
        map_path = menu();

        if (map_path == NULL) {
            closesdl();
            return 0;
        }
    }
    if (!load_map(map_path)) {
        SDL_Log("Could not load map\n");
        exit(1);
    }
    else
        SDL_Log("Map %dx%d loaded successfully!\n", g_map.width, g_map.height);

    
    level_width = g_map.width * CELL_SIZE;
    level_height = g_map.height * CELL_SIZE;

    Anthill anthill = {0, 0, -1, 0, 0};
    init_anthill(&anthill);


    bool quit = false;
    bool reset = true;

    g_eventstart = SDL_RegisterEvents(1);

    SDL_Event event;

    Player player = {0};
#define PLAYER_SPAWN_X (anthill.gm_x + 0.5) * CELL_SIZE
#define PLAYER_SPAWN_Y anthill.gm_y * CELL_SIZE
    player.ant = create_ant(PLAYER_SPAWN_X, PLAYER_SPAWN_Y);
    if (player.ant == NULL) {
        SDL_Log("Error: could not allocate memory for player ant\n");
        exit(1);
    }

    player.ant->scale=1.59;
    player.width = g_ant_texture.width / ANT_FRAMES_NUM;
    player.height = g_ant_texture.height;

    {
        int universal_food_count = g_map.height * g_map.width / TILES_PER_FOOD;
        while(g_world_food_count < universal_food_count) {
            create_food();
        }
    }

    //call move_player each ANT_MS_TO_MOVE sec
    SDL_AddTimer(ANT_MS_TO_MOVE, move_player, (void *) &player);

    while (reset) {
        reset = false;
        while(!(quit || reset)) {
            set_camera(&player);
            while(SDL_PollEvent(&event) != 0) {
                switch (event.type) {
#if ANDROID_BUILD
                    case SDL_FINGERDOWN:;
                        int x = event.tfinger.x * screen_width, y = event.tfinger.y * screen_height;

                        if (anthill.x <= x + g_camera.x && x + g_camera.x <= anthill.x + g_anthill_texture.width &&
                            anthill.y <= y + g_camera.y && y + g_camera.y <= anthill.y + g_anthill_texture.height) {
                            //tapped on the anthill
                            if (player.in_anthill && player.food_count >= g_levels_table[anthill.level] && anthill.level < MAX_LEVEL) {
                                player.food_count -= g_levels_table[anthill.level];
                                update_food_count_texture(player.food_count, g_levels_table[anthill.level + 1]);

                                for (int i = 0; i < g_levels_table[anthill.level] / 2; i++)
                                    if (create_npc(anthill.gm_x, anthill.gm_y) == NULL)
                                        SDL_Log("Warning: could not create NPC ant\n");

                                update_anthill_level_texture(++anthill.level);
                                if (anthill.level == MAX_LEVEL) {
                                    goto win;
                                }
                            }
                        }
                        else if (event.tfinger.x <= 1.0 / 3) {
                            player.turn_vel = -ANT_TURN_DEGREES;
                        }

                        else if (event.tfinger.x >= 2.0 / 3) {
                            player.turn_vel = ANT_TURN_DEGREES;
                        }
                        else if (event.tfinger.y <= 0.5)
                            if (player.vel < 0)
                                player.vel = 0;
                            else
                                player.vel = ANT_VEL_MAX;
                        else
                            if (player.vel > 0)
                                player.vel = 0;
                            else
                                player.vel = -ANT_VEL_MAX / 2;

                        break;
                    case SDL_FINGERUP:
                        if (event.tfinger.x <= 1.0 / 3)
                            //left
                            player.turn_vel = 0;
                        else if (event.tfinger.x >= 2.0 / 3)
                            //right
                            player.turn_vel = 0;
                        break;
#else
                    case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
#if DEBUGMODE
                        //cheats for developers
                        case SDL_SCANCODE_LCTRL:
                            if (create_npc(anthill.gm_x, anthill.gm_y) == NULL)
                                SDL_Log("Warning: Could not allocate memory for an npc ant");
                            break;
                        case SDL_SCANCODE_RCTRL:
                            player.food_count++;
                            update_food_count_texture(player.food_count, g_levels_table[anthill.level]);
                            break;
#endif
                        case SDL_SCANCODE_SPACE:
                            //upgrade if inside (TODO: copied to android btw which is a problem)
                            if (player.in_anthill && player.food_count >= g_levels_table[anthill.level] && anthill.level < MAX_LEVEL) {
                                player.food_count -= g_levels_table[anthill.level];
                                update_food_count_texture(player.food_count, g_levels_table[anthill.level + 1]);
                                for (int i = 0; i < g_levels_table[anthill.level] / 2; i++)
                                    if (create_npc(anthill.gm_x, anthill.gm_y) == NULL)
                                        SDL_Log("Warning: could not create NPC ant\n");
                                update_anthill_level_texture(++anthill.level);
                                if (anthill.level == MAX_LEVEL) {
                                    goto win;
                                }
                            }
                            break;
                        case SDL_SCANCODE_F11:
                            toggle_fullscreen();
                            break;
                        case SDL_SCANCODE_ESCAPE:
                        case SDL_SCANCODE_AC_BACK:
                            reset = true;
                            break;
                        }
                        if (event.key.repeat == 0)
                            switch (event.key.keysym.scancode) {
                                case SDL_SCANCODE_W:
                                    player.vel += ANT_VEL_MAX;
                                    break;
                                case SDL_SCANCODE_S:
                                    player.vel -= ANT_VEL_MAX / 2;
                                    break;
                                case SDL_SCANCODE_A:
                                    player.turn_vel -= ANT_TURN_DEGREES;
                                    break;
                                case SDL_SCANCODE_D:
                                    player.turn_vel += ANT_TURN_DEGREES;
                                    break;
                        }
                        break;
                    case SDL_KEYUP:
                        switch (event.key.keysym.scancode) {
                            case SDL_SCANCODE_W:
                                player.vel -= ANT_VEL_MAX;
                                break;
                            case SDL_SCANCODE_S:
                                player.vel += ANT_VEL_MAX / 2;
                                break;
                            case SDL_SCANCODE_A:
                                player.turn_vel += ANT_TURN_DEGREES;
                                break;
                            case SDL_SCANCODE_D:
                                player.turn_vel -= ANT_TURN_DEGREES;
                                break;
                        }
                        break;
#endif
                    case SDL_WINDOWEVENT:
                          if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                            g_camera.w = screen_width = event.window.data1;
                            g_camera.h = screen_height = event.window.data2;
#if ANDROID_BUILD
                            SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN);
#endif
                          }
                        break;
                    case SDL_QUIT:
                        quit = true;
                        break;
                    case SDL_USEREVENT:
                        //only friendly ants currently
                        player.food_count++;
                        update_food_count_texture(player.food_count, g_levels_table[anthill.level]);
                        create_food();
                        break;
                }
            }
            render_game_objects(&player, &anthill);
            SDL_RenderPresent(g_renderer);
        }

        if (reset) {
            player.vel = 0;
            player.turn_vel = 0;
            if ((map_path = menu()) != NULL) {
                for (size_t i = 0; i < g_npc_sp; i++) {
                    destroy_npc(g_npc_stack[i]);
                }
                g_npc_sp = 0;
                destroy_map(&g_map);
                load_map(map_path);
                level_width = g_map.width * CELL_SIZE;
                level_height = g_map.height * CELL_SIZE;
                init_anthill(&anthill);
                player.ant->angle = 0;
                player.ant->x = PLAYER_SPAWN_X;
                player.ant->y = PLAYER_SPAWN_Y;
                init_anthill(&anthill);
                int universal_food_count = g_map.height * g_map.width / TILES_PER_FOOD;
                g_world_food_count = 0;
                while(g_world_food_count < universal_food_count) {
                    create_food();
                }

            }
        }
    }

	closesdl();
	return 0;

win:;
    Texture win_texture = win();
    while(!quit) {
            while(SDL_PollEvent(&event) != 0) {
                switch (event.type) {
                    case SDL_QUIT:
                        quit = true;
                        break;
                //partially copypasted from main event loop which is a problem, will find a fix later
                case SDL_WINDOWEVENT:
                      if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        g_camera.w = screen_width = event.window.data1;
                        g_camera.h = screen_height = event.window.data2;
#if ANDROID_BUILD
                        SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN);
#endif
                      }
                    break;
                }
            }

        render_game_objects(&player, &anthill);
        render_texture(win_texture, screen_width / 2 - win_texture.width / 2, screen_height / 2 - win_texture.height / 2);
        SDL_RenderPresent(g_renderer);
    }
    closesdl();
    return 0;
}

#define ANT_STACK_INIT_SIZE 10
Npc** init_npc_stack(void) {
    return malloc(ANT_STACK_INIT_SIZE * sizeof(Npc *));
}

bool push_npc(Npc *npc) {
    static size_t stack_size = ANT_STACK_INIT_SIZE;
    if (g_npc_sp < stack_size) {
        g_npc_stack[g_npc_sp++] = npc;
    }
    else {
        if ((g_npc_stack = realloc((void *) g_npc_stack, stack_size * 2 * sizeof(Ant *))) == NULL) {
            return false;
        }
        stack_size *= 2;
        g_npc_stack[g_npc_sp++] = npc;

    }
    return true;
}

bool check_collision(SDL_Rect a, SDL_Rect b) {
    if(a.y + a.h <= b.y  ||
        a.y >= b.y + b.h ||
        a.x + a.w <= b.x ||
        a.x >= b.x + b.w)
        return false;
    return true;
}

