#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define BM_UNUSED(x) (void)x
void bm_todo(const char *msg, ...);
#define BM_ASSERT(expr)                                    \
    do {                                                   \
        if(!(expr))                                        \
        {                                                  \
            fprintf(stderr,                                \
                    "Assertion failed: %s\n"               \
                    "  File: %s\n"                         \
                    "  Line: %d\n",                        \
                    #expr, __FILE__, __LINE__);            \
            abort();                                       \
        }                                                  \
    } while(0)

typedef enum 
{
    BM_SUCCESS = 0 << 1,
    BM_DPY_ERR,
    BM_ROOT_WIN_ERR,
    BM_APP_WIN_ERR,
    BM_GLX_VISUAL_ERR,
} bm_error_t;

#define BM_ERROR_CHECK(expr)      \
    do {                          \
        if ((expr) != BM_SUCCESS) \
        {                         \
            bm_get_error();       \
            return 1;             \
        }                         \
    } while(0)

_Thread_local static bm_error_t err = BM_SUCCESS;

typedef struct
{
    int x, y;
} bm_vec2i_t;

typedef struct
{
    float x, y;
} bm_vec2f_t;

typedef struct 
{
    Display *dpy;
    Window root, app;
    int screen, width, height;
} boomer_t;

typedef struct 
{
    bm_vec2f_t pos;
    float scale;
} bm_camera_t;

#ifdef BM_DEBUG_LOG
extern void bm_log(const char *file_path, const char *msg, ...);
#define BOOMER_LOG_STDOUT(msg, ...) bm_log(NULL, msg, __VA_ARGS__)
#endif

bm_vec2f_t bm_scalar_scale_float(bm_vec2f_t v, float scale)
{
    if (scale == 0) return v;
    return (bm_vec2f_t) {
        .x = v.x * scale,
        .y = v.y * scale,
    };
}

// NOTE: No need to check for null in the calle function.
static XImage *bm_capture_screenshot(boomer_t *b)
{
    XSync(b->dpy, False);
    XImage *img = XGetImage(b->dpy, b->root, 0, 0, b->width, b->height, AllPlanes, ZPixmap);
    BM_ASSERT(img != NULL);
    return img;
}

static bm_error_t bm_initialize_x11(boomer_t *b)
{
    b->dpy = XOpenDisplay(NULL);
    if (!b->dpy) return (err = BM_DPY_ERR);

    b->root = XDefaultRootWindow(b->dpy);
    if (b->root == 0) return (err = BM_ROOT_WIN_ERR);

    b->screen = DefaultScreen(b->dpy);

    b->width = DisplayWidth(b->dpy, b->screen);
    b->height = DisplayHeight(b->dpy, b->screen);

    BM_ASSERT(b->width != 0 || b->height != 0);
    return BM_SUCCESS;
}

static bm_error_t bm_initialize_glx(boomer_t *b)
{
    GLint attr[] = {
        GLX_RGBA, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        None
    };

    XVisualInfo *vi = glXChooseVisual(b->dpy, b->screen, attr);
    if (!vi) return (err = BM_GLX_VISUAL_ERR);

    bm_todo("bm_initialize_glx unimplemented");
    return BM_SUCCESS;
}

void bm_get_error()
{
    #define BM_LOG_RETURN(msg)    \
    {                             \
            fprintf(stderr, msg); \
            putchar('\n');        \
            return;               \
    }

    switch(err)
    {
        case BM_SUCCESS: { return; }
        case BM_DPY_ERR: BM_LOG_RETURN("Could not initalize X11 display");
        case BM_ROOT_WIN_ERR: BM_LOG_RETURN("Could not initalize X11 root window");
        case BM_APP_WIN_ERR: BM_LOG_RETURN("Could not initalize X11 application window");
        case BM_GLX_VISUAL_ERR: BM_LOG_RETURN("Could not initalize GLX ChooseVisual failed");
    }

    #undef BM_LOG_RETURN
}

int main(void)
{
    boomer_t b = {0};
    BM_ERROR_CHECK(bm_initialize_x11(&b));

    XImage *boomer_captured = bm_capture_screenshot(&b);
    BM_ERROR_CHECK(bm_initialize_glx(&b));

#ifdef BM_DEBUG_STDOUT_LOG
    BOOMER_LOG_STDOUT("Dpy %p", (void *)b.dpy);
    BOOMER_LOG_STDOUT("Root %ld",b.root);
    BOOMER_LOG_STDOUT("Screen %d", b.screen);
    BOOMER_LOG_STDOUT("Width %d", b.width);
    BOOMER_LOG_STDOUT("Height %d", b.height);
#endif
    return 0;
}

_Noreturn void bm_todo(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    putchar('\n');
    exit(1);
}

#ifdef BM_DEBUG_LOG

extern void bm_log(const char *file_path, const char *msg, ...)
{
    FILE *stream = stdout;
    if (file_path) stream = fopen(file_path, "ab+");
    if (!stream) stream = stdout;

    va_list args;
    va_start(args, msg);
    vfprintf(stream, msg, args);
    va_end(args);
    putchar('\n');
}

#endif
