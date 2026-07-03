#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <X11/Xlib.h>
#include "GL/glew.h"
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

#ifdef BM_DEBUG_LOG
extern void bm_log(const char *file_path, const char *msg, ...);
#define BOOMER_LOG_STDOUT(msg, ...) bm_log(NULL, msg, __VA_ARGS__)
#endif

#define BM_ERROR_CHECK(expr)      \
    do {                          \
        bm_error_t e = (expr);    \
        if ((e) != BM_SUCCESS)    \
        {                         \
            bm_get_error(e);      \
            return 1;             \
        }                         \
    } while(0)


typedef enum 
{
    BM_SUCCESS,
    BM_DPY_ERR,
    BM_ROOT_WIN_ERR,
    BM_APP_WIN_ERR,
    BM_GLX_VISUAL_ERR,
    BM_GLEW_ERR,
} bm_error_t;

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
    GLXContext context;
} boomer_t;

typedef struct 
{
    bm_vec2f_t pos;
    float scale;
} bm_camera_t;

typedef struct
{
    char *items;
    size_t count;
    size_t capacity;
} bm_string_builder_t;

_Thread_local static GLenum glew_err = GLEW_NO_ERROR;

bm_vec2f_t bm_scalar_scale_float(bm_vec2f_t v, float scale)
{
    if (scale <= 0) return v;
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

static void bm_append_to_builder_s(bm_string_builder_t *sb, const char *item, size_t s)
{
    while (sb->count + s + 1 > sb->capacity)
    {
        sb->capacity = sb->capacity == 0 ? 256 : sb->capacity * 2;
        sb->items = realloc(sb->items, sb->capacity);
        BM_ASSERT(sb->items != NULL);
    }
    memcpy(sb->items + sb->count, item, s);
    sb->count += s;
    sb->items[sb->count] = 0;
}

void bm_read_shader(bm_string_builder_t *sb, const char *program)
{
    FILE *f = fopen(program, "r");
    BM_ASSERT(f != NULL);
    fseek(f, 0, SEEK_END);
    size_t file_size =  ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(sizeof(*buf) * file_size);
    BM_ASSERT(buf != NULL);
    fread(buf, 1, file_size, f);

    bm_append_to_builder_s(sb, buf, file_size);
}

static GLuint bm_compile_shader(const bm_string_builder_t *sb, GLenum kind)
{
    BM_ASSERT(sb->items != NULL);
    GLuint shader = glCreateShader(kind);
    const char *src = sb->items;
    GLint len = (GLint)sb->count;
    glShaderSource(shader, 1, &src, &len);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error (%s):\n%s\n",
                kind == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        return 0;
    }
    return ok;
}

static GLuint bm_make_program(const bm_string_builder_t *vert_sb,
                                  const bm_string_builder_t *frag_sb)
{

    GLuint v = bm_compile_shader(vert_sb, GL_VERTEX_SHADER);
    GLuint f = bm_compile_shader(frag_sb, GL_FRAGMENT_SHADER);
    if (!v || !f) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "Program error: %s\n", log);
        glDeleteProgram(program);
        return 0;
    }

    return ok;
}

static bm_error_t bm_initialize_x11(boomer_t *b)
{
    b->dpy = XOpenDisplay(NULL);
    if (!b->dpy) return BM_DPY_ERR;

    b->root = XDefaultRootWindow(b->dpy);
    if (b->root == 0) return BM_ROOT_WIN_ERR;

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

    XVisualInfo *visual_info  = glXChooseVisual(b->dpy, b->screen, attr);
    if (!visual_info) return BM_GLX_VISUAL_ERR;

    Colormap color_map = XCreateColormap(b->dpy, b->root, 
                                         visual_info->visual, AllocNone);
    XSetWindowAttributes attributes = {0};
    attributes.colormap = color_map;

    // TODO: Set Xlib-attribute for key press or mouse click.
    attributes.event_mask = KeyPressMask
                            | ButtonPressMask | ButtonReleaseMask
                            | PointerMotionMask;

    b->app = XCreateWindow(b->dpy,   b->root, 0, 0, 
                           b->width, b->height, 
                           0, visual_info->depth, 
                           InputOutput, visual_info->visual, 
                           CWColormap | CWEventMask | CWOverrideRedirect, &attributes);

    XMapRaised(b->dpy, b->app);
    b->context = glXCreateContext(b->dpy, visual_info, 
                                  NULL, True);
    XFree(visual_info);

    glXMakeCurrent(b->dpy, b->app, b->context);
    glewExperimental = True;

    glew_err = glewInit();
    if (glew_err != GLEW_OK) return BM_GLEW_ERR;

    return BM_SUCCESS;
}

void bm_get_error(bm_error_t err)
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
        case BM_GLEW_ERR: BM_LOG_RETURN((char *)glewGetErrorString(glew_err));
    }

    #undef BM_LOG_RETURN
}

int main(void)
{
    boomer_t b = {0};
    BM_ERROR_CHECK(bm_initialize_x11(&b));

    XImage *boomer_captured = bm_capture_screenshot(&b);
    BM_ERROR_CHECK(bm_initialize_glx(&b));

    const char *vert_shader_path = "vert.glsl";
    const char *frag_shader_path = "frag.glsl";

    bm_string_builder_t vert_sb = {0};
    bm_string_builder_t frag_sb = {0};
    bm_read_shader(&vert_sb, vert_shader_path);
    bm_read_shader(&frag_sb, frag_shader_path);

    GLuint prog = bm_make_program(&vert_sb, &frag_sb);
    BM_ASSERT(prog != 0);

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

    va_list args;
    va_start(args, msg);
    vfprintf(stream, msg, args);
    va_end(args);
    putchar('\n');
}

#endif
