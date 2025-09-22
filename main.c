// Nicholas dos Santos Leal - 10409210
// Patrick Alves Gonçalves - 10409363
// Gustavo Ibara - 10389067

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

/*— tipos de dados —*/
typedef struct {
    SDL_Window *w;
    SDL_Renderer *r;
} Janela;

typedef struct {
    SDL_Surface *surf;
    SDL_Texture *tex;
    SDL_FRect box;
} Imagem;

/*— globais (separados linha a linha) —*/
static Janela jImg = {0};
static Imagem imgAtiva = {0};
static Imagem imgBackup = {0};
static TTF_Font *fonte = NULL;

/*— util —*/
static bool abrir_janela(Janela *j, const char *titulo, int w, int h, SDL_WindowFlags f) {
    return SDL_CreateWindowAndRenderer(titulo, w, h, f, &j->w, &j->r);
}

static void fechar_janela(Janela *j) {
    if (!j) {
        return;
    }
    if (j->r) {
        SDL_DestroyRenderer(j->r);
    }
    if (j->w) {
        SDL_DestroyWindow(j->w);
    }
    j->w = NULL;
    j->r = NULL;
}

static void limpar_imagem(Imagem *im) {
    if (!im) {
        return;
    }
    if (im->tex) {
        SDL_DestroyTexture(im->tex);
    }
    if (im->surf) {
        SDL_DestroySurface(im->surf);
    }
    im->tex = NULL;
    im->surf = NULL;
    im->box = (SDL_FRect){0, 0, 0, 0};
}

/*— imagem —*/
static void carregar_rgba(const char *path, SDL_Renderer *r, Imagem *outA, Imagem *outB) {
    if (!path || !r || !outA || !outB) {
        return;
    }

    limpar_imagem(outA);
    limpar_imagem(outB);

    SDL_Surface *s = IMG_Load(path);
    if (!s) {
        SDL_Log("Falha IMG: %s", SDL_GetError());
        return;
    }

    outA->surf = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    outB->surf = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(s);

    if (!outA->surf || !outB->surf) {
        SDL_Log("RGBA32 falhou: %s", SDL_GetError());
        return;
    }

    outA->tex = SDL_CreateTextureFromSurface(r, outA->surf);
    outB->tex = SDL_CreateTextureFromSurface(r, outB->surf);
    SDL_GetTextureSize(outA->tex, &outA->box.w, &outA->box.h);
    SDL_GetTextureSize(outB->tex, &outB->box.w, &outB->box.h);
}

static void salvar_png(Imagem *im, const char *path) {
    if (!im || !im->surf) {
        SDL_Log("Salvar: imagem nula");
        return;
    }

    if (IMG_SavePNG(im->surf, path) != 1) {
        SDL_Log("Salvar falhou '%s': %s", path, SDL_GetError());
    } else {
        SDL_Log("PNG salvo em '%s'", path);
    }
}

/*— init/quit —*/
static SDL_AppResult init_all(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL Init falhou: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (TTF_Init() != 1) {
        SDL_Log("TTF Init falhou: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!abrir_janela(&jImg,  "Imagem", 640, 480, 0)) {
        SDL_Log("Janela falhou: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

static void quit_all(void) {
    if (fonte) {
        TTF_CloseFont(fonte);
        fonte = NULL;
    }

    limpar_imagem(&imgAtiva);
    limpar_imagem(&imgBackup);

    fechar_janela(&jImg);

    TTF_Quit();
    SDL_Quit();
}

/*— main —*/
int main(int argc, char **argv) {
    atexit(quit_all);

    if (argc < 2) {
        SDL_Log("Uso: %s <imagem>", argv[0]);
        return SDL_APP_FAILURE;
    }

    if (init_all() == SDL_APP_FAILURE) {
        return SDL_APP_FAILURE;
    }

    carregar_rgba(argv[1], jImg.r, &imgAtiva, &imgBackup);

    int iw = (int)imgAtiva.box.w;
    int ih = (int)imgAtiva.box.h;

    if (iw > 0 && ih > 0) {
        SDL_SetWindowSize(jImg.w, iw, ih);
        SDL_SetWindowPosition(jImg.w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    SDL_Event e;
    bool run = true;

    while (run) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                run = false;
            } else if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_S) {
                    salvar_png(&imgAtiva, "output_image.png");
                }
            } else if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                run = false;
            }
        }
    }

    return 0;
}