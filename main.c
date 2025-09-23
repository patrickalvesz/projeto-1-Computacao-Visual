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

typedef struct {
    SDL_FRect area;
    SDL_Color base;
    SDL_Color foco;
    SDL_Color ativo;
    bool over;
    bool down;
} Botao;

/*— constantes —*/
enum {
    W_IMG_PAD  = 640,
    H_IMG_PAD  = 480,
    W_HST_PAD  = 560,
    H_HST_PAD  = 600
};

/*— globais (separados linha a linha) —*/
static Janela jImg = {0};
static Janela jHist = {0};
static Imagem imgAtiva = {0};
static Imagem imgBackup = {0};
static Botao  botToggle = {0};
static int    hist[256] = {0};
static int    lutEq[256] = {0};
static bool   equalizada = false;
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

/*— texto usado: fonte do sistema —*/
static TTF_Font* abrir_fonte_sistema(int pt) {
#ifdef _WIN32
    const char *candidatas[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\tahoma.ttf"
    };
#else
    const char *candidatas[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
    };
#endif
    for (size_t i = 0; i < sizeof(candidatas) / sizeof(candidatas[0]); ++i) {
        TTF_Font *f = TTF_OpenFont(candidatas[i], pt);
        if (f) {
            return f;
        }
    }
    return NULL;
}

static void desenhar_texto(SDL_Renderer *r, const char *s, int x, int y, SDL_Color c) {
    if (!fonte) {
        return; // sem fonte do sistema, não desenha texto
    }
    SDL_Surface *ts = TTF_RenderText_Solid(fonte, s, 0, c);
    if (!ts) {
        return;
    }
    SDL_Texture *tt = SDL_CreateTextureFromSurface(r, ts);
    if (!tt) {
        SDL_DestroySurface(ts);
        return;
    }
    SDL_FRect dst = { (float)x, (float)y, (float)ts->w, (float)ts->h };
    SDL_RenderTexture(r, tt, NULL, &dst);
    SDL_DestroyTexture(tt);
    SDL_DestroySurface(ts);
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

    /* ajusta mantendo proporção dentro de 640x480 (fit) */
    float tex_w = outA->box.w, tex_h = outA->box.h;
    float scale_x = (float)W_IMG_PAD / tex_w;
    float scale_y = (float)H_IMG_PAD / tex_h;
    float scale = scale_x < scale_y ? scale_x : scale_y;

    float dst_w = tex_w * scale;
    float dst_h = tex_h * scale;
    float dst_x = ((float)W_IMG_PAD - dst_w) * 0.5f;
    float dst_y = ((float)H_IMG_PAD - dst_h) * 0.5f;

    outA->box.x = dst_x; outA->box.y = dst_y; outA->box.w = dst_w; outA->box.h = dst_h;
    outB->box = outA->box; /* usa o mesmo destino para a cópia backup */


}

static int ja_eh_cinza(Imagem *im) {
    if (!im || !im->surf) {
        return -1;
    }

    SDL_LockSurface(im->surf);
    const SDL_PixelFormatDetails *fmt = SDL_GetPixelFormatDetails(im->surf->format);
    Uint32 *px = (Uint32*)im->surf->pixels;
    size_t n = (size_t)im->surf->w * im->surf->h;

    for (size_t i = 0; i < n; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(px[i], fmt, NULL, &r, &g, &b, &a);
        if (!(r == g && g == b)) {
            SDL_UnlockSurface(im->surf);
            return 0;
        }
    }

    SDL_UnlockSurface(im->surf);
    return 1;
}

static void para_cinza(SDL_Renderer *r, Imagem *im) {
    if (!r || !im || !im->surf) {
        return;
    }

    SDL_LockSurface(im->surf);
    const SDL_PixelFormatDetails *fmt = SDL_GetPixelFormatDetails(im->surf->format);
    Uint32 *px = (Uint32*)im->surf->pixels;
    size_t n = (size_t)im->surf->w * im->surf->h;

    for (size_t i = 0; i < n; i++) {
        Uint8 R, G, B, A;
        SDL_GetRGBA(px[i], fmt, NULL, &R, &G, &B, &A);
        float y = 0.2125f * R + 0.7154f * G + 0.0721f * B;
        Uint8 yy = (Uint8)roundf(y);
        px[i] = SDL_MapRGBA(fmt, NULL, yy, yy, yy, A);
    }

    SDL_UnlockSurface(im->surf);

    if (im->tex) {
        SDL_DestroyTexture(im->tex);
    }
    im->tex = SDL_CreateTextureFromSurface(r, im->surf);
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

/*— estatística/eq —*/
static float media_int(void) {
    if (!imgAtiva.surf) {
        return 0.f;
    }

    int n = imgAtiva.surf->w * imgAtiva.surf->h;
    if (!n) {
        return 0.f;
    }

    float s = 0.f;
    SDL_LockSurface(imgAtiva.surf);

    Uint32 *px = (Uint32*)imgAtiva.surf->pixels;
    const SDL_PixelFormatDetails *f = SDL_GetPixelFormatDetails(imgAtiva.surf->format);

    for (int i = 0; i < n; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(px[i], f, NULL, &r, &g, &b, &a);
        s += r;
    }

    SDL_UnlockSurface(imgAtiva.surf);
    return s / (float)n;
}

static float desvio_int(void) {
    if (!imgAtiva.surf) {
        return 0.f;
    }

    int n = imgAtiva.surf->w * imgAtiva.surf->h;
    if (!n) {
        return 0.f;
    }

    float m = media_int();
    float s = 0.f;

    SDL_LockSurface(imgAtiva.surf);

    Uint32 *px = (Uint32*)imgAtiva.surf->pixels;
    const SDL_PixelFormatDetails *f = SDL_GetPixelFormatDetails(imgAtiva.surf->format);

    for (int i = 0; i < n; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(px[i], f, NULL, &r, &g, &b, &a);
        float d = r - m;
        s += d * d;
    }

    SDL_UnlockSurface(imgAtiva.surf);
    return sqrtf(s / (float)n);
}

static void montar_hist(void) {
    if (!imgAtiva.surf) {
        return;
    }

    for (int i = 0; i < 256; i++) {
        hist[i] = 0;
    }

    int n = imgAtiva.surf->w * imgAtiva.surf->h;
    if (!n) {
        return;
    }

    SDL_LockSurface(imgAtiva.surf);

    Uint32 *px = (Uint32*)imgAtiva.surf->pixels;
    const SDL_PixelFormatDetails *f = SDL_GetPixelFormatDetails(imgAtiva.surf->format);

    for (int i = 0; i < n; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(px[i], f, NULL, &r, &g, &b, &a);
        hist[r]++;
    }

    SDL_UnlockSurface(imgAtiva.surf);
}

static void montar_lut(void) {
    if (!imgAtiva.surf) {
        return;
    }

    int n = imgAtiva.surf->w * imgAtiva.surf->h;
    int acc = 0;

    for (int i = 0; i < 256; i++) {
        acc += hist[i];
        lutEq[i] = (int)roundf(((float)acc * (255.0f / (float)n)));
    }
}

static void aplicar_eq(SDL_Renderer *r, Imagem *im) {
    if (!r || !im || !im->surf) {
        return;
    }

    montar_lut();

    SDL_LockSurface(im->surf);
    const SDL_PixelFormatDetails *f = SDL_GetPixelFormatDetails(im->surf->format);
    Uint32 *px = (Uint32*)im->surf->pixels;
    size_t n = (size_t)im->surf->w * im->surf->h;

    for (size_t i = 0; i < n; i++) {
        Uint8 R, G, B, A;
        SDL_GetRGBA(px[i], f, NULL, &R, &G, &B, &A);
        Uint8 k = (Uint8)lutEq[R];
        px[i] = SDL_MapRGBA(f, NULL, k, k, k, A);
    }

    SDL_UnlockSurface(im->surf);

    if (im->tex) {
        SDL_DestroyTexture(im->tex);
    }
    im->tex = SDL_CreateTextureFromSurface(r, im->surf);
}

static void restaurar(SDL_Renderer *r, Imagem *dst, Imagem *src) {
    if (!r || !dst || !src || !dst->surf || !src->surf) {
        return;
    }

    SDL_memcpy(dst->surf->pixels, src->surf->pixels, src->surf->h * src->surf->pitch);

    if (dst->tex) {
        SDL_DestroyTexture(dst->tex);
    }
    dst->tex = SDL_CreateTextureFromSurface(r, dst->surf);
}

/*— rótulos simples —*/
static const char* rotulo_media(float m) {
    int k = (int)roundf(m);
    if (k < 80) {
        return "Escura";
    }
    if (k <= 175) {
        return "Media";
    }
    return "Clara";
}

static const char* rotulo_dp(float d) {
    if (d < 48.f) {
        return "Baixa";
    }
    if (d <= 109.f) {
        return "Media";
    }
    return "Alta";
}

/*— UI: botão —*/
static void desenhar_botao(SDL_Renderer *r, Botao *b, const char *txt) {
    SDL_Color c = b->base;
    if (b->down) {
        c = b->ativo;
    } else if (b->over) {
        c = b->foco;
    }

    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    SDL_RenderFillRect(r, &b->area);

    SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
    SDL_RenderRect(r, &b->area);

    SDL_Color branco = {255, 255, 255, 255};
    desenhar_texto(r, txt, (int)(b->area.x + 12), (int)(b->area.y + 8), branco);
}

static bool evento_botao(Botao *b, SDL_Event *e, SDL_Window *w) {
    SDL_WindowID id = SDL_GetWindowID(w);

    if ((e->type == SDL_EVENT_MOUSE_MOTION && e->motion.windowID != id) ||
        ((e->type == SDL_EVENT_MOUSE_BUTTON_DOWN || e->type == SDL_EVENT_MOUSE_BUTTON_UP) && e->button.windowID != id)) {
        return false;
    }

    float mx = 0;
    float my = 0;

    if (e->type == SDL_EVENT_MOUSE_MOTION) {
        mx = e->motion.x;
        my = e->motion.y;
    } else if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN || e->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        mx = e->button.x;
        my = e->button.y;
    }

    bool dentro = (mx > b->area.x && mx < b->area.x + b->area.w && my > b->area.y && my < b->area.y + b->area.h);
    bool mudou = false;

    if (b->over != dentro) {
        b->over = dentro;
        mudou = true;
    }

    if (dentro) {
        if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            b->down = true;
        } else if (e->type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (b->down) {
                b->down = false;
                return true;
            }
        }
    } else {
        b->down = false;
    }

    return mudou;
}

/*— desenho do histograma —*/
static int plot_histograma() {
    int mx = 1;
    for (int i = 0; i < 256; i++) {
        if (hist[i] > mx) {
            mx = hist[i];
        }
    }

    float bw = (float)(W_HST_PAD - 50) / 256.0f;
    int gh = H_HST_PAD - 240;
    int y0 = 60;

    for (int i = 0; i < 256; i++) {
        float h = ((float)hist[i] / (float)mx) * (gh - 40);
        SDL_SetRenderDrawColor(jHist.r, 180, 210, 240, 255);
        SDL_FRect bar = { 40.f + i * bw, (float)y0 + (float)gh - h, bw, h };
        SDL_RenderFillRect(jHist.r, &bar);
    }

    return mx;
}

/*— renderização —*/
static void draw_all(void) {
    SDL_SetRenderDrawColor(jImg.r, 120, 120, 120, 255);
    SDL_RenderClear(jImg.r);

    if (imgAtiva.tex) {
        SDL_RenderTexture(jImg.r, imgAtiva.tex, NULL, &imgAtiva.box);
    }

    SDL_RenderPresent(jImg.r);

    SDL_SetRenderDrawColor(jHist.r, 30, 30, 50, 255);
    SDL_RenderClear(jHist.r);

    plot_histograma();

    SDL_Color wc = {255, 255, 255, 255};
    SDL_Color lc = {210, 210, 210, 255};

    desenhar_botao(jHist.r, &botToggle, equalizada ? "EQUALIZADO" : "ORIGINAL");
    desenhar_texto(jHist.r, "Histograma", 190, 15, wc);
    desenhar_texto(jHist.r, "Nivel", 220, H_HST_PAD - 170, lc);
    desenhar_texto(jHist.r, "0", 35, H_HST_PAD - 170, lc);
    desenhar_texto(jHist.r, "255", W_HST_PAD - 70, H_HST_PAD - 170, lc);

    float m = media_int();
    float d = desvio_int();

    char sm[32];
    char sd[32];

    snprintf(sm, sizeof(sm), "Média %.2f", m);
    snprintf(sd, sizeof(sd), "Desvio Padrão %.2f", d);

    desenhar_texto(jHist.r, sm, 20, 450, lc);
    desenhar_texto(jHist.r, rotulo_media(m), 20, 490, lc);
    desenhar_texto(jHist.r, sd, 20, 470, lc);
    desenhar_texto(jHist.r, rotulo_dp(d), 20, 510, lc);

    SDL_RenderPresent(jHist.r);
}

/*— ciclo —*/
static void loop(void) {
    bool refresh = true;
    bool run = true;

    SDL_Event e;

    while (run) {
        if (refresh) {
            draw_all();
            refresh = false;
        }

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

            if (evento_botao(&botToggle, &e, jHist.w)) {
                if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && botToggle.over) {
                    equalizada = !equalizada;
                    if (equalizada) {
                        aplicar_eq(jImg.r, &imgAtiva);
                    } else {
                        restaurar(jImg.r, &imgAtiva, &imgBackup);
                    }
                    montar_hist();
                    refresh = true;
                } else {
                    refresh = true;
                }
            }
        }
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

    if (!abrir_janela(&jImg,  "Imagem",     W_IMG_PAD,  H_IMG_PAD, 0) ||
        !abrir_janela(&jHist, "Histograma", W_HST_PAD,  H_HST_PAD, 0)) {
        SDL_Log("Janela falhou: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // torna a janela de histograma filha da janela de imagem
    SDL_SetWindowParent(jHist.w, jImg.w);

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
    fechar_janela(&jHist);

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

    fonte = abrir_fonte_sistema(18); /* se não achar, fallback bitmap é usado */

    carregar_rgba(argv[1], jImg.r, &imgAtiva, &imgBackup);

    if (ja_eh_cinza(&imgAtiva) == 0) {
        para_cinza(jImg.r, &imgAtiva);
    }

    SDL_memcpy(imgBackup.surf->pixels, imgAtiva.surf->pixels, imgAtiva.surf->h * imgAtiva.surf->pitch);
    montar_hist();

    int iw = (int)imgAtiva.box.w;
    int ih = (int)imgAtiva.box.h;

    if (iw > 0 && ih > 0) {
        SDL_SetWindowSize(jImg.w, W_IMG_PAD, H_IMG_PAD);
        SDL_SetWindowPosition(jImg.w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        SDL_Rect bounds;
        SDL_DisplayID did = SDL_GetPrimaryDisplay();

        if (SDL_GetDisplayBounds(did, &bounds) != 1) {
            bounds = (SDL_Rect){0, 0, 1920, 1080};
        }

        bounds.y += 70;

        int hw, hh;
        SDL_GetWindowSize(jHist.w, &hw, &hh);

        int m = 35;
        int x = bounds.x + bounds.w - hw - m;
        int y = bounds.y + m;

        SDL_SetWindowPosition(jHist.w, x, y);
    }

    botToggle.area = (SDL_FRect){ 320, H_HST_PAD - 100, 200, 38 };
    botToggle.base = (SDL_Color){ 20, 100, 220, 255 };
    botToggle.foco = (SDL_Color){ 40, 160, 240, 255 };
    botToggle.ativo = (SDL_Color){ 10, 60, 150, 255 };
    botToggle.over = false;
    botToggle.down = false;

    loop();
    return 0;
}
