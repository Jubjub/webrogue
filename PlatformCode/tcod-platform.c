#ifdef BROGUE_TCOD
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libwebsockets.h>
#include "platform.h"
#include "cJSON.h"


#if TCOD_TECHVERSION >= 0x01050103
#define USE_NEW_TCOD_API
#endif

/* tcod->websocket compatibility layer */
typedef int bool;
#define false 0
#define true 1

typedef enum {
    TCODK_NONE,
    TCODK_ESCAPE,
    TCODK_BACKSPACE,
    TCODK_TAB,
    TCODK_ENTER,
    TCODK_SHIFT,
    TCODK_CONTROL,
    TCODK_ALT,
    TCODK_PAUSE,
    TCODK_CAPSLOCK,
    TCODK_PAGEUP,
    TCODK_PAGEDOWN,
    TCODK_END,
    TCODK_HOME,
    TCODK_UP,
    TCODK_LEFT,
    TCODK_RIGHT,
    TCODK_DOWN,
    TCODK_PRINTSCREEN,
    TCODK_INSERT,
    TCODK_DELETE,
    TCODK_LWIN,
    TCODK_RWIN,
    TCODK_APPS,
    TCODK_0,
    TCODK_1,
    TCODK_2,
    TCODK_3,
    TCODK_4,
    TCODK_5,
    TCODK_6,
    TCODK_7,
    TCODK_8,
    TCODK_9,
    TCODK_KP0,
    TCODK_KP1,
    TCODK_KP2,
    TCODK_KP3,
    TCODK_KP4,
    TCODK_KP5,
    TCODK_KP6,
    TCODK_KP7,
    TCODK_KP8,
    TCODK_KP9,
    TCODK_KPADD,
    TCODK_KPSUB,
    TCODK_KPDIV,
    TCODK_KPMUL,
    TCODK_KPDEC,
    TCODK_KPENTER,
    TCODK_F1,
    TCODK_F2,
    TCODK_F3,
    TCODK_F4,
    TCODK_F5,
    TCODK_F6,
    TCODK_F7,
    TCODK_F8,
    TCODK_F9,
    TCODK_F10,
    TCODK_F11,
    TCODK_F12,
    TCODK_NUMLOCK,
    TCODK_SCROLLLOCK,
    TCODK_SPACE,
    TCODK_CHAR,
    TCODK_TEXT
} TCOD_keycode_t;

typedef unsigned char uint8;
typedef unsigned int uint32;

typedef struct {
    uint8 r,g,b;
} TCOD_color_t;

typedef enum {
    TCOD_KEY_PRESSED=1,
    TCOD_KEY_RELEASED=2,
} TCOD_key_status_t;

typedef struct {
    int x,y; /* absolute position */
    int dx,dy; /* movement since last update in pixels */
    int cx,cy; /* cell coordinates in the root console */
    int dcx,dcy; /* movement since last update in console cells */
    bool lbutton ; /* left button status */
    bool rbutton ; /* right button status */
    bool mbutton ; /* middle button status */
    bool lbutton_pressed ; /* left button pressed event */ 
    bool rbutton_pressed ; /* right button pressed event */ 
    bool mbutton_pressed ; /* middle button pressed event */ 
    bool wheel_up ; /* wheel up event */
    bool wheel_down ; /* wheel down event */
} TCOD_mouse_t;

#define TCOD_KEY_TEXT_SIZE 32

typedef struct {
    TCOD_keycode_t vk; /*  key code */
    char c; /* character if vk == TCODK_CHAR else 0 */
    char text[TCOD_KEY_TEXT_SIZE]; /* text if vk == TCODK_TEXT else text[0] == '\0' */
    bool pressed ; /* does this correspond to a key press or key release event ? */
    bool lalt ;
    bool lctrl ;
    bool ralt ;
    bool rctrl ;
    bool shift ;
} TCOD_key_t;

/* holds characters in the virtual console */
typedef struct {
    int character;
    TCOD_color_t foreground;
    TCOD_color_t background;
} ConsoleCell;

/* globals */
bool needs_websocket_update = false;
char *consoleJSON = 0;
ConsoleCell *console = 0;

/* websocket */
#define MAX_BROGUE_PAYLOAD 1400

struct sessionData {
    uint8 buf[LWS_SEND_BUFFER_PRE_PADDING + MAX_BROGUE_PAYLOAD + LWS_SEND_BUFFER_POST_PADDING];
    uint32 len;
    uint32 index;
};

static int callback_brogue(struct libwebsocket_context *context, struct libwebsocket *wsi,
        enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len) {
    struct sessionData *pss = (struct sessionData *)user;
    int n;
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            puts("client connected");
            /* start triggering callbacks */
            libwebsocket_callback_on_writable(context, wsi);
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            puts("checking for deliverable data");
            if (needs_websocket_update) {
                puts("delivering data");
                needs_websocket_update = false;
                n = libwebsocket_write(wsi, &consoleJSON[LWS_SEND_BUFFER_PRE_PADDING],
                        strlen(consoleJSON + LWS_SEND_BUFFER_PRE_PADDING), LWS_WRITE_TEXT);
                if (n < 0) {
                    lwsl_err("ERROR %d writing to socket, hanging up\n", n);
                    return 1;
                }
                if (n < (int)pss->len) {
                    lwsl_err("Partial write\n");
                    return -1;
                }
            }
            libwebsocket_callback_on_writable(context, wsi);
            break;

        case LWS_CALLBACK_RECEIVE:
                memcpy(&pss->buf[LWS_SEND_BUFFER_PRE_PADDING], in, len);
                pss->len = (uint32)len;
                printf("received: %s\n", in);
            break;
        default:
            break;
    }
    return 0;
}

static struct libwebsocket_protocols protocols[] = {
    {
        "default",
        callback_brogue,
        sizeof(struct sessionData)
    },
    {
        NULL, NULL, 0
    }
};


struct libwebsocket_context *context;

void init_websockets() {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = 7732;
    info.protocols = protocols;
    context = libwebsocket_create_context(&info);
    if (!context) {
        puts("unable to create lws context");
        exit(1);
    }
}

/* compat functions */

bool TCOD_console_is_key_pressed(TCOD_keycode_t key) {
    return false;
}

void TCOD_sys_sleep_milli(int ms) {
    Sleep(ms);
    /* FIXME: lag? never heard of it! */
    int n;
    n = libwebsocket_service(context, 3000);
}

void console_put_char_ex(int x, int y, int character, TCOD_color_t fg, TCOD_color_t bg) {
    console[y * COLS + x].character = character;
    console[y * COLS + x].foreground = fg;
    console[y * COLS + x].background = bg;
}

void TCOD_console_flush() {
    /*
    FILE *fp = fopen("out.txt", "wb");
    int x, y;
    for (y = 0; y < ROWS; y++) {
        for (x = 0; x < COLS; x++) {
            fputc(console[y * COLS + x].character, fp);
        }
        fputc('\n', fp);
    }
    fclose(fp);
    */
    cJSON *root, *tiles;
    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "tiles",  tiles = cJSON_CreateArray());
    int x, y;
    for (y = 0; y < ROWS; y++) {
        for (x = 0; x < COLS; x++) {
            ConsoleCell *cell = &console[y * COLS + x];
            cJSON *tile;
            cJSON_AddItemToArray(tiles, tile = cJSON_CreateArray());
            cJSON_AddItemToArray(tile, cJSON_CreateNumber(x));
            cJSON_AddItemToArray(tile, cJSON_CreateNumber(y));
            cJSON_AddItemToArray(tile, cJSON_CreateNumber(cell->character));
        }
    }
    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    free(consoleJSON);
    consoleJSON = malloc(LWS_SEND_BUFFER_PRE_PADDING + strlen(json) + 1
            + LWS_SEND_BUFFER_POST_PADDING);
    strcpy(consoleJSON + LWS_SEND_BUFFER_PRE_PADDING, json);
    free(json);
    needs_websocket_update = true;
}

/************************/

extern playerCharacter rogue;
extern short brogueFontSize;

struct mouseState {int x, y, lmb, rmb;};

static int isFullScreen = false;
static int hasMouseMoved = false;

static TCOD_key_t bufferedKey = {TCODK_NONE};
static struct mouseState brogueMouse = {0, 0, 0, 0};
static struct mouseState missedMouse = {-1, -1, 0, 0};

static int desktop_width, desktop_height;

static void loadFont(int detectSize) {
    puts("initializing console");
    printf("buffer size: %ix%i\n", COLS, ROWS);
    console = malloc(sizeof(ConsoleCell) * COLS * ROWS);
    puts("initializing websocket");
    init_websockets();
}

static void gameLoop()
{
    loadFont(true);
    rogueMain();
}

static void tcod_plotChar(uchar inputChar,
        short xLoc, short yLoc,
        short foreRed, short foreGreen, short foreBlue,
        short backRed, short backGreen, short backBlue) {

    TCOD_color_t fore;
    TCOD_color_t back;

    fore.r = (uint8) foreRed * 255 / 100;
    fore.g = (uint8) foreGreen * 255 / 100;
    fore.b = (uint8) foreBlue * 255 / 100;
    back.r = (uint8) backRed * 255 / 100;
    back.g = (uint8) backGreen * 255 / 100;
    back.b = (uint8) backBlue * 255 / 100;

    if (inputChar == STATUE_CHAR) {
        inputChar = 223;
    } else if (inputChar > 255) {
        switch (inputChar) {
#ifdef USE_UNICODE
            case FLOOR_CHAR: inputChar = 128 + 0; break;
            case CHASM_CHAR: inputChar = 128 + 1; break;
            case TRAP_CHAR: inputChar = 128 + 2; break;
            case FIRE_CHAR: inputChar = 128 + 3; break;
            case FOLIAGE_CHAR: inputChar = 128 + 4; break;
            case AMULET_CHAR: inputChar = 128 + 5; break;
            case SCROLL_CHAR: inputChar = 128 + 6; break;
            case RING_CHAR: inputChar = 128 + 7; break;
            case WEAPON_CHAR: inputChar = 128 + 8; break;
            case GEM_CHAR: inputChar = 128 + 9; break;
            case TOTEM_CHAR: inputChar = 128 + 10; break;
            case BAD_MAGIC_CHAR: inputChar = 128 + 12; break;
            case GOOD_MAGIC_CHAR: inputChar = 128 + 13; break;

            case DOWN_ARROW_CHAR: inputChar = 144 + 1; break;
            case LEFT_ARROW_CHAR: inputChar = 144 + 2; break;
            case RIGHT_ARROW_CHAR: inputChar = 144 + 3; break;
            case UP_TRIANGLE_CHAR: inputChar = 144 + 4; break;
            case DOWN_TRIANGLE_CHAR: inputChar = 144 + 5; break;
            case OMEGA_CHAR: inputChar = 144 + 6; break;
            case THETA_CHAR: inputChar = 144 + 7; break;
            case LAMDA_CHAR: inputChar = 144 + 8; break;
            case KOPPA_CHAR: inputChar = 144 + 9; break; // is this right?
            case CHARM_CHAR: inputChar = 144 + 9; break;
            case LOZENGE_CHAR: inputChar = 144 + 10; break;
            case CROSS_PRODUCT_CHAR: inputChar = 144 + 11; break;
#endif
            default: inputChar = '?'; break;
        }
    }
    //printf("%i %i %c\n", xLoc, yLoc, inputChar);
    console_put_char_ex(xLoc, yLoc, (int) inputChar, fore, back);
}

static void initWithFont(int fontSize)
{
    loadFont(false);
    refreshScreen();
}

static boolean processSpecialKeystrokes(TCOD_key_t k, boolean text) {
    /* we dont' really need screenshot and resizing keys */
    return false;
}


struct mapsymbol {
    int in_vk, out_vk;
    int in_c, out_c;
    struct mapsymbol *next;
};

static struct mapsymbol *keymap = NULL;

static void rewriteKey(TCOD_key_t *key, boolean text) {
    /* FIXME: rewrite
       if (key->vk == TCODK_CHAR && (SDL_GetModState() & KMOD_CAPS)) {
    // cancel out caps lock
    if (!key->shift) {
    key->c = tolower(key->c);
    }
    }

    struct mapsymbol *s = keymap;
    while (s != NULL) {
    if (key->vk == s->in_vk) {
    if (s->in_vk != TCODK_CHAR || (!text && s->in_c == key->c)) {
    // we have a match!
    key->vk = s->out_vk;
    key->c = s->out_c;

    // only apply one remapping
    return;
    }
    }
    s = s->next;
    }
    */
}

static void getModifiers(rogueEvent *returnEvent) {
    /* FIXME: rewrite
       Uint8 *keystate = SDL_GetKeyState(NULL);
       returnEvent->controlKey = keystate[SDLK_LCTRL] || keystate[SDLK_RCTRL];
       returnEvent->shiftKey = keystate[SDLK_LSHIFT] || keystate[SDLK_RSHIFT];
       */
}


// returns true if input is acceptable
static boolean processKeystroke(TCOD_key_t key, rogueEvent *returnEvent, boolean text)
{
    if (processSpecialKeystrokes(key, text)) {
        return false;
    }

    returnEvent->eventType = KEYSTROKE;
    getModifiers(returnEvent);
    switch (key.vk) {
        case TCODK_CHAR:
        case TCODK_0:
        case TCODK_1:
        case TCODK_2:
        case TCODK_3:
        case TCODK_4:
        case TCODK_5:
        case TCODK_6:
        case TCODK_7:
        case TCODK_8:
        case TCODK_9:
            returnEvent->param1 = (unsigned short) key.c;
            if (returnEvent->shiftKey && returnEvent->param1 >= 'a' && returnEvent->param1 <= 'z') {
                returnEvent->param1 += 'A' - 'a';
            }
            break;
        case TCODK_SPACE:
            returnEvent->param1 = ACKNOWLEDGE_KEY;
            break;
        case TCODK_ESCAPE:
            returnEvent->param1 = ESCAPE_KEY;
            break;
        case TCODK_UP:
            returnEvent->param1 = UP_ARROW;
            break;
        case TCODK_DOWN:
            returnEvent->param1 = DOWN_ARROW;
            break;
        case TCODK_RIGHT:
            returnEvent->param1 = RIGHT_ARROW;
            break;
        case TCODK_LEFT:
            returnEvent->param1 = LEFT_ARROW;
            break;
        case TCODK_ENTER:
            returnEvent->param1 = RETURN_KEY;
            break;
        case TCODK_KPENTER:
            returnEvent->param1 = ENTER_KEY;
            break;
        case TCODK_BACKSPACE:
            returnEvent->param1 = DELETE_KEY;
            break;
        case TCODK_TAB:
            returnEvent->param1 = TAB_KEY;
            break;
        case TCODK_KP0:
            returnEvent->param1 = NUMPAD_0;
            break;
        case TCODK_KP1:
            returnEvent->param1 = NUMPAD_1;
            break;
        case TCODK_KP2:
            returnEvent->param1 = NUMPAD_2;
            break;
        case TCODK_KP3:
            returnEvent->param1 = NUMPAD_3;
            break;
        case TCODK_KP4:
            returnEvent->param1 = NUMPAD_4;
            break;
        case TCODK_KP5:
            returnEvent->param1 = NUMPAD_5;
            break;
        case TCODK_KP6:
            returnEvent->param1 = NUMPAD_6;
            break;
        case TCODK_KP7:
            returnEvent->param1 = NUMPAD_7;
            break;
        case TCODK_KP8:
            returnEvent->param1 = NUMPAD_8;
            break;
        case TCODK_KP9:
            returnEvent->param1 = NUMPAD_9;
            break;
        default:
            return false;
    }
    return true;
}

static boolean tcod_pauseForMilliseconds(short milliseconds)
{
    TCOD_mouse_t mouse;
    TCOD_console_flush();
    TCOD_sys_sleep_milli((unsigned int) milliseconds);

#ifdef USE_NEW_TCOD_API
    if (bufferedKey.vk == TCODK_NONE) {
        TCOD_sys_check_for_event(TCOD_EVENT_KEY_PRESS | TCOD_EVENT_MOUSE, &bufferedKey, &mouse);
    } else {
        TCOD_sys_check_for_event(TCOD_EVENT_MOUSE, 0, &mouse);
    }
#else
    if (bufferedKey.vk == TCODK_NONE) {
        //bufferedKey = TCOD_console_check_for_keypress(TCOD_KEY_PRESSED);
    }
#endif

    if (missedMouse.lmb == 0 && missedMouse.rmb == 0) {
        /* FIXME mouse input */
        //mouse = TCOD_mouse_get_status();
        if (mouse.lbutton_pressed || mouse.rbutton_pressed) {
            missedMouse.x = mouse.cx;
            missedMouse.y = mouse.cy;
            if (mouse.lbutton_pressed) missedMouse.lmb = MOUSE_DOWN;
            if (mouse.rbutton_pressed) missedMouse.rmb = MOUSE_DOWN;
        }
    }

    return (bufferedKey.vk != TCODK_NONE || missedMouse.lmb || missedMouse.rmb);
}


#define PAUSE_BETWEEN_EVENT_POLLING		36//17

static void tcod_nextKeyOrMouseEvent(rogueEvent *returnEvent, boolean textInput, boolean colorsDance)
{
    boolean tryAgain;
    TCOD_key_t key;
    TCOD_mouse_t mouse;
    uint32 theTime, waitTime;
    short x, y;

    TCOD_console_flush();

    key.vk = TCODK_NONE;

    if (noMenu && rogue.nextGame == NG_NOTHING) rogue.nextGame = NG_NEW_GAME;

    for (;;) {
        //theTime = TCOD_sys_elapsed_milli();

        /* FIXME
        if (TCOD_console_is_window_closed()) {
            rogue.gameHasEnded = true; // causes the game loop to terminate quickly
            rogue.nextGame = NG_QUIT; // causes the menu to drop out immediately
            returnEvent->eventType = KEYSTROKE;
            returnEvent->param1 = ESCAPE_KEY;
            return;
        }
        */

        tryAgain = false;

        if (bufferedKey.vk != TCODK_NONE) {
            rewriteKey(&bufferedKey, textInput);
            if (processKeystroke(bufferedKey, returnEvent, textInput)) {
                bufferedKey.vk = TCODK_NONE;
                return;
            } else {
                bufferedKey.vk = TCODK_NONE;
            }
        }

        if (missedMouse.lmb) {
            returnEvent->eventType = missedMouse.lmb;

            returnEvent->param1 = missedMouse.x;
            returnEvent->param2 = missedMouse.y;
            if (TCOD_console_is_key_pressed(TCODK_CONTROL)) {
                returnEvent->controlKey = true;
            }
            if (TCOD_console_is_key_pressed(TCODK_SHIFT)) {
                returnEvent->shiftKey = true;
            }

            missedMouse.lmb = missedMouse.lmb == MOUSE_DOWN ? MOUSE_UP : 0;
            return;
        }

        if (missedMouse.rmb) {
            returnEvent->eventType = missedMouse.rmb == MOUSE_DOWN ? RIGHT_MOUSE_DOWN : RIGHT_MOUSE_UP;

            returnEvent->param1 = missedMouse.x;
            returnEvent->param2 = missedMouse.y;
            if (TCOD_console_is_key_pressed(TCODK_CONTROL)) {
                returnEvent->controlKey = true;
            }
            if (TCOD_console_is_key_pressed(TCODK_SHIFT)) {
                returnEvent->shiftKey = true;
            }

            missedMouse.rmb = missedMouse.rmb == MOUSE_DOWN ? MOUSE_UP : 0;
            return;
        }

        /*  FIXME: color shuffle
            if (!(serverMode || (SDL_GetAppState() & SDL_APPACTIVE))) {
            TCOD_sys_sleep_milli(100);
            } else {
            if (colorsDance) {
            shuffleTerrainColors(3, true);
            commitDraws();
            }
            TCOD_console_flush();
            }*/

#ifdef USE_NEW_TCOD_API
        TCOD_sys_check_for_event(TCOD_EVENT_KEY_PRESS | TCOD_EVENT_MOUSE, &key, &mouse);
#else
        //key = TCOD_console_check_for_keypress(TCOD_KEY_PRESSED);
#endif

        rewriteKey(&key, textInput);
        if (processKeystroke(key, returnEvent, textInput)) {
            return;
        }

        //mouse = TCOD_mouse_get_status();

        /* FIXME: mouse input */
        /*
        if (serverMode || (SDL_GetAppState() & SDL_APPACTIVE)) {
            x = mouse.cx;
            y = mouse.cy;
        } else {
            x = 0;
            y = 0;
        }
        */

        if (
                mouse.lbutton_pressed || mouse.rbutton_pressed
                || mouse.lbutton != brogueMouse.lmb || mouse.rbutton != brogueMouse.rmb
                || brogueMouse.x !=x || brogueMouse.y != y) {

            returnEvent->param1 = x;
            returnEvent->param2 = y;

            getModifiers(returnEvent);

            if (mouse.lbutton_pressed) {
                if (!brogueMouse.lmb) {
                    // we missed a mouseDown event -- better make up for it!
                    missedMouse.x = x;
                    missedMouse.y = y;
                    missedMouse.lmb = MOUSE_UP;
                    returnEvent->eventType = MOUSE_DOWN;
                } else {
                    returnEvent->eventType = MOUSE_UP;
                }
            } else if (mouse.lbutton && !brogueMouse.lmb) {
                returnEvent->eventType = MOUSE_DOWN;
            } else {
                returnEvent->eventType = MOUSE_ENTERED_CELL;
            }

            if (mouse.rbutton_pressed) {
                if (!brogueMouse.rmb) {
                    // we missed a mouseDown event -- better make up for it!
                    missedMouse.x = x;
                    missedMouse.y = y;
                    missedMouse.rmb = MOUSE_UP;
                    returnEvent->eventType = RIGHT_MOUSE_DOWN;
                } else {
                    returnEvent->eventType = RIGHT_MOUSE_UP;
                }
            } else if (mouse.rbutton && !brogueMouse.rmb) {
                returnEvent->eventType = RIGHT_MOUSE_DOWN;
            }

            brogueMouse.x = x;
            brogueMouse.y = y;
            brogueMouse.lmb = mouse.lbutton;
            brogueMouse.rmb = mouse.rbutton;

            if (returnEvent->eventType == MOUSE_ENTERED_CELL && !hasMouseMoved) {
                hasMouseMoved = true;
            } else {
                return;
            }
        }

        /* FIXME
        waitTime = PAUSE_BETWEEN_EVENT_POLLING + theTime - TCOD_sys_elapsed_milli();

        if (waitTime > 0 && waitTime <= PAUSE_BETWEEN_EVENT_POLLING) {
            TCOD_sys_sleep_milli(waitTime);
        }
        */
    }
}



// tcodkeys is derived from console_types.h
char *tcodkeys[] = {
    "NONE",
    "ESCAPE",
    "BACKSPACE",
    "TAB",
    "ENTER",
    "SHIFT",
    "CONTROL",
    "ALT",
    "PAUSE",
    "CAPSLOCK",
    "PAGEUP",
    "PAGEDOWN",
    "END",
    "HOME",
    "UP",
    "LEFT",
    "RIGHT",
    "DOWN",
    "PRINTSCREEN",
    "INSERT",
    "DELETE",
    "LWIN",
    "RWIN",
    "APPS",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "KP0",
    "KP1",
    "KP2",
    "KP3",
    "KP4",
    "KP5",
    "KP6",
    "KP7",
    "KP8",
    "KP9",
    "KPADD",
    "KPSUB",
    "KPDIV",
    "KPMUL",
    "KPDEC",
    "KPENTER",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "NUMLOCK",
    "SCROLLLOCK",
    "SPACE",
    "CHAR",
    NULL
};

static void tcod_remap(const char *input_name, const char *output_name) {
    // find input and output in the list of tcod keys, if it's there
    int i;
    struct mapsymbol *sym = malloc(sizeof(*sym));

    if (sym == NULL) return; // out of memory?  seriously?

    // default to treating the names as literal ascii symbols
    sym->in_c = input_name[0];
    sym->out_c = output_name[0];
    sym->in_vk = TCODK_CHAR;
    sym->out_vk = TCODK_CHAR;

    // but try to find them in the list of tcod keys
    for (i = 0; tcodkeys[i] != NULL; i++) {
        if (strcmp(input_name, tcodkeys[i]) == 0) {
            sym->in_vk = i;
            sym->in_c = 0;
            break;
        }
    }

    for (i = 0; tcodkeys[i] != NULL; i++) {
        if (strcmp(output_name, tcodkeys[i]) == 0) {
            sym->out_vk = i;
            sym->out_c = 0;
            break;
        }
    }

    sym->next = keymap;
    keymap = sym;
}

static boolean modifier_held(int modifier) {
    rogueEvent tempEvent;

    getModifiers(&tempEvent);
    if (modifier == 0) return tempEvent.shiftKey;
    if (modifier == 1) return tempEvent.controlKey;

    return 0;
}

struct brogueConsole tcodConsole = {
    gameLoop,
    tcod_pauseForMilliseconds,
    tcod_nextKeyOrMouseEvent,
    tcod_plotChar,
    tcod_remap,
    modifier_held
};

#endif

