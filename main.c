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
typedef struct
{
    SDL_Window *w;
    SDL_Renderer *r;
} Janela;

typedef struct
{
    SDL_Surface *surf;
    SDL_Texture *tex;
    SDL_FRect box;
} Imagem;

typedef struct
{
    SDL_FRect area;
    SDL_Color base;
    SDL_Color foco;
    SDL_Color ativo;
    bool over;
    bool down;
} Botao;

/*— constantes —*/
enum
{
    W_IMG_PAD = 640,
    H_IMG_PAD = 480,
    W_HST_PAD = 560,
    H_HST_PAD = 600
};

/*— globais (separados linha a linha) —*/
static Janela jImg = {0};
static Janela jHist = {0};
static Imagem imgAtiva = {0};
static Imagem imgBackup = {0};
static Botao botToggle = {0};
static int hist[256] = {0};
static TTF_Font *fonte = NULL;

/*— util —*/
static bool abrir_janela(Janela *j, const char *titulo, int w, int h, SDL_WindowFlags f)
{
    return SDL_CreateWindowAndRenderer(titulo, w, h, f, &j->w, &j->r);
}

static void fechar_janela(Janela *j)
{
    if (!j)
    {
        return;
    }
    if (j->r)
    {
        SDL_DestroyRenderer(j->r);
    }
    if (j->w)
    {
        SDL_DestroyWindow(j->w);
    }
    j->w = NULL;
    j->r = NULL;
}

static void limpar_imagem(Imagem *im)
{
    if (!im)
    {
        return;
    }
    if (im->tex)
    {
        SDL_DestroyTexture(im->tex);
    }
    if (im->surf)
    {
        SDL_DestroySurface(im->surf);
    }
    im->tex = NULL;
    im->surf = NULL;
    im->box = (SDL_FRect){0, 0, 0, 0};
}

/*— imagem —*/
static void carregar_rgba(const char *path, SDL_Renderer *r, Imagem *outA, Imagem *outB)
{
    if (!path || !r || !outA || !outB)
    {
        return;
    }

    limpar_imagem(outA);
    limpar_imagem(outB);

    SDL_Surface *s = IMG_Load(path);
    if (!s)
    {
        SDL_Log("Falha IMG: %s", SDL_GetError());
        return;
    }

    outA->surf = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    outB->surf = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(s);

    if (!outA->surf || !outB->surf)
    {
        SDL_Log("RGBA32 falhou: %s", SDL_GetError());
        return;
    }

    outA->tex = SDL_CreateTextureFromSurface(r, outA->surf);
    outB->tex = SDL_CreateTextureFromSurface(r, outB->surf);
    SDL_GetTextureSize(outA->tex, &outA->box.w, &outA->box.h);
    SDL_GetTextureSize(outB->tex, &outB->box.w, &outB->box.h);
}

static int ja_eh_cinza(Imagem *im)
{
    if (!im || !im->surf)
    {
        return -1;
    }

    SDL_LockSurface(im->surf);
    const SDL_PixelFormatDetails *fmt = SDL_GetPixelFormatDetails(im->surf->format);
    Uint32 *px = (Uint32 *)im->surf->pixels;
    size_t n = (size_t)im->surf->w * im->surf->h;

    for (size_t i = 0; i < n; i++)
    {
        Uint8 r, g, b, a;
        SDL_GetRGBA(px[i], fmt, NULL, &r, &g, &b, &a);
        if (!(r == g && g == b))
        {
            SDL_UnlockSurface(im->surf);
            return 0;
        }
    }

    SDL_UnlockSurface(im->surf);
    return 1;
}

static void para_cinza(SDL_Renderer *r, Imagem *im)
{
    if (!r || !im || !im->surf)
    {
        return;
    }

    SDL_LockSurface(im->surf);
    const SDL_PixelFormatDetails *fmt = SDL_GetPixelFormatDetails(im->surf->format);
    Uint32 *px = (Uint32 *)im->surf->pixels;
    size_t n = (size_t)im->surf->w * im->surf->h;

    for (size_t i = 0; i < n; i++)
    {
        Uint8 R, G, B, A;
        SDL_GetRGBA(px[i], fmt, NULL, &R, &G, &B, &A);
        float y = 0.2125f * R + 0.7154f * G + 0.0721f * B;
        Uint8 yy = (Uint8)roundf(y);
        px[i] = SDL_MapRGBA(fmt, NULL, yy, yy, yy, A);
    }

    SDL_UnlockSurface(im->surf);

    if (im->tex)
    {
        SDL_DestroyTexture(im->tex);
    }
    im->tex = SDL_CreateTextureFromSurface(r, im->surf);
}

static void salvar_png(Imagem *im, const char *path)
{
    if (!im || !im->surf)
    {
        SDL_Log("Salvar: imagem nula");
        return;
    }

    if (IMG_SavePNG(im->surf, path) != 1)
    {
        SDL_Log("Salvar falhou '%s': %s", path, SDL_GetError());
    }
    else
    {
        SDL_Log("PNG salvo em '%s'", path);
    }
}

/*— histograma —*/
static void montar_hist(void)
{
    if (!imgAtiva.surf)
    {
        return;
    }

    for (int i = 0; i < 256; i++)
    {
        hist[i] = 0;
    }

    int n = imgAtiva.surf->w * imgAtiva.surf->h;
    if (!n)
    {
        return;
    }

    SDL_LockSurface(imgAtiva.surf);

    Uint32 *px = (Uint32 *)imgAtiva.surf->pixels;
    const SDL_PixelFormatDetails *f = SDL_GetPixelFormatDetails(imgAtiva.surf->format);

    for (int i = 0; i < n; i++)
    {
        Uint8 r, g, b, a;
        SDL_GetRGBA(px[i], f, NULL, &r, &g, &b, &a);
        hist[r]++;
    }

    SDL_UnlockSurface(imgAtiva.surf);
}

static int plot_histograma()
{
    int mx = 1;
    for (int i = 0; i < 256; i++)
    {
        if (hist[i] > mx)
        {
            mx = hist[i];
        }
    }

    float bw = (float)(W_HST_PAD - 50) / 256.0f;
    int gh = H_HST_PAD - 240;
    int y0 = 60;

    for (int i = 0; i < 256; i++)
    {
        float h = ((float)hist[i] / (float)mx) * (gh - 40);
        SDL_SetRenderDrawColor(jHist.r, 180, 210, 240, 255);
        SDL_FRect bar = {40.f + i * bw, (float)y0 + (float)gh - h, bw, h};
        SDL_RenderFillRect(jHist.r, &bar);
    }

    return mx;
}

/*— UI: botão —*/
static void desenhar_texto(SDL_Renderer *r, const char *s, int x, int y, SDL_Color c)
{
    if (!fonte)
    {
        return;
    }
    SDL_Surface *ts = TTF_RenderText_Solid(fonte, s, 0, c);
    if (!ts)
    {
        return;
    }
    SDL_Texture *tt = SDL_CreateTextureFromSurface(r, ts);
    if (!tt)
    {
        SDL_DestroySurface(ts);
        return;
    }
    SDL_FRect dst = {(float)x, (float)y, (float)ts->w, (float)ts->h};
    SDL_RenderTexture(r, tt, NULL, &dst);
    SDL_DestroyTexture(tt);
    SDL_DestroySurface(ts);
}

static void desenhar_botao(SDL_Renderer *r, Botao *b, const char *txt)
{
    SDL_Color c = b->base;
    if (b->down)
    {
        c = b->ativo;
    }
    else if (b->over)
    {
        c = b->foco;
    }

    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    SDL_RenderFillRect(r, &b->area);

    SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
    SDL_RenderRect(r, &b->area);

    SDL_Color branco = {255, 255, 255, 255};
    desenhar_texto(r, txt, (int)(b->area.x + 12), (int)(b->area.y + 8), branco);
}

static bool evento_botao(Botao *b, SDL_Event *e, SDL_Window *w)
{
    SDL_WindowID id = SDL_GetWindowID(w);

    if ((e->type == SDL_EVENT_MOUSE_MOTION && e->motion.windowID != id) ||
        ((e->type == SDL_EVENT_MOUSE_BUTTON_DOWN || e->type == SDL_EVENT_MOUSE_BUTTON_UP) && e->button.windowID != id))
    {
        return false;
    }

    float mx = 0;
    float my = 0;

    if (e->type == SDL_EVENT_MOUSE_MOTION)
    {
        mx = e->motion.x;
        my = e->motion.y;
    }
    else if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN || e->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        mx = e->button.x;
        my = e->button.y;
    }

    bool dentro = (mx > b->area.x && mx < b->area.x + b->area.w && my > b->area.y && my < b->area.y + b->area.h);
    bool mudou = false;

    if (b->over != dentro)
    {
        b->over = dentro;
        mudou = true;
    }

    if (dentro)
    {
        if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            b->down = true;
        }
        else if (e->type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            if (b->down)
            {
                b->down = false;
                return true;
            }
        }
    }
    else
    {
        b->down = false;
    }

    return mudou;
}

/*— renderização —*/
static void draw_all(void)
{
    SDL_SetRenderDrawColor(jImg.r, 120, 120, 120, 255);
    SDL_RenderClear(jImg.r);

    if (imgAtiva.tex)
    {
        SDL_RenderTexture(jImg.r, imgAtiva.tex, &imgAtiva.box, &imgAtiva.box);
    }

    SDL_RenderPresent(jImg.r);

    SDL_SetRenderDrawColor(jHist.r, 30, 30, 50, 255);
    SDL_RenderClear(jHist.r);

    plot_histograma();

    SDL_Color wc = {255, 255, 255, 255};
    SDL_Color lc = {210, 210, 210, 255};

    desenhar_botao(jHist.r, &botToggle, "ORIGINAL");
    desenhar_texto(jHist.r, "Histograma", 190, 15, wc);
    desenhar_texto(jHist.r, "Nivel", 220, H_HST_PAD - 170, lc);
    desenhar_texto(jHist.r, "0", 35, H_HST_PAD - 170, lc);
    desenhar_texto(jHist.r, "255", W_HST_PAD - 70, H_HST_PAD - 170, lc);

    SDL_RenderPresent(jHist.r);
}

/*— init/quit —*/
static SDL_AppResult init_all(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL Init falhou: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (TTF_Init() != 1)
    {
        SDL_Log("TTF Init falhou: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!abrir_janela(&jImg, "Imagem", W_IMG_PAD, H_IMG_PAD, 0) ||
        !abrir_janela(&jHist, "Histograma", W_HST_PAD, H_HST_PAD, 0))
    {
        SDL_Log("Janela falhou: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

static void quit_all(void)
{
    if (fonte)
    {
        TTF_CloseFont(fonte);
        fonte = NULL;
    }

    limpar_imagem(&imgAtiva);
    limpar_imagem(&imgBackup);

    fechar_janela(&jImg);
    fechar_janela(&jHist);

    TTF_Quit();
    SDL_Quit();
}

/*— main —*/
int main(int argc, char **argv)
{
    atexit(quit_all);

    if (argc < 2)
    {
        SDL_Log("Uso: %s <imagem>", argv[0]);
        return SDL_APP_FAILURE;
    }

    if (init_all() == SDL_APP_FAILURE)
    {
        return SDL_APP_FAILURE;
    }

    carregar_rgba(argv[1], jImg.r, &imgAtiva, &imgBackup);

    if (ja_eh_cinza(&imgAtiva) == 0)
    {
        para_cinza(jImg.r, &imgAtiva);
    }

    SDL_memcpy(imgBackup.surf->pixels, imgAtiva.surf->pixels, imgAtiva.surf->h * imgAtiva.surf->pitch);
    montar_hist();

    int iw = (int)imgAtiva.box.w;
    int ih = (int)imgAtiva.box.h;

    if (iw > 0 && ih > 0)
    {
        SDL_SetWindowSize(jImg.w, iw, ih);
        SDL_SetWindowPosition(jImg.w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    botToggle.area = (SDL_FRect){320, H_HST_PAD - 100, 200, 38};
    botToggle.base = (SDL_Color){20, 100, 220, 255};
    botToggle.foco = (SDL_Color){40, 160, 240, 255};
    botToggle.ativo = (SDL_Color){10, 60, 150, 255};
    botToggle.over = false;
    botToggle.down = false;

    SDL_Event e;
    bool run = true;
    bool refresh = true;

    while (run)
    {
        if (refresh)
        {
            draw_all();
            refresh = false;
        }

        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT)
            {
                run = false;
            }
            else if (e.type == SDL_EVENT_KEY_DOWN)
            {
                if (e.key.key == SDLK_S)
                {
                    salvar_png(&imgAtiva, "output_image.png");
                }
            }
            else if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                run = false;
            }

            if (evento_botao(&botToggle, &e, jHist.w))
            {
                refresh = true;
            }
        }
    }

    return 0;
}
