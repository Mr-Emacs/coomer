#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include "GL/glew.h"
#include <GL/gl.h>
#include <GL/glx.h>

#include "bm_shaders.h"

#define BM_UNUSED(x) (void)x
_Noreturn void bm_todo(const char *msg, ...);
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

#define BM_MIN_SCALE                 0.01f
#define BM_SCROLL_SPEED              1.5f
#define BM_DRAG_FRICTION             6.0f
#define BM_SCALE_FRICTION            4.0f
#define BM_VELOCITY_THRESHOLD        15.0f
#define BM_FL_RADIUS                 200.0f
#define BM_FL_SHADOW                 0.8f
#define BM_FL_SHADOW_SPEED           6.0f
#define BM_FL_DELTA_RADIUS           250.0f
#define BM_FL_RADIUS_DECELERATION    10.0f

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
    Atom wm_delete;
    bool windowed;
} boomer_t;

typedef struct
{
    bm_vec2f_t pos;
    bm_vec2f_t velocity;
    float scale;
    float delta_scale;
    bm_vec2f_t scale_pivot;
} bm_camera_t;

typedef struct
{
    bool enabled;
    float shadow;
    float radius;
    float delta_radius;
} bm_flashlight_t;

typedef struct
{
    bm_vec2f_t curr;
    bm_vec2f_t prev;
    bool drag;
} bm_mouse_t;

typedef struct
{
    GLint camera_pos;
    GLint camera_scale;
    GLint window_size;
    GLint screenshot_size;
    GLint cursor_pos;
    GLint fl_shadow;
    GLint fl_radius;
    GLint mirror;
    GLint tex;
} bm_uniforms_t;

typedef struct
{
    char *items;
    size_t count;
    size_t capacity;
} bm_string_builder_t;

_Thread_local static GLenum glew_err = GLEW_NO_ERROR;

void bm_get_error(bm_error_t err);

static float bm_vec2f_length(bm_vec2f_t v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
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
    long file_size = ftell(f);
    BM_ASSERT(file_size > 0);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(sizeof(*buf) * (size_t)file_size);
    BM_ASSERT(buf != NULL);
    size_t n = fread(buf, 1, (size_t)file_size, f);
    BM_ASSERT(n == (size_t)file_size);
    fclose(f);

    bm_append_to_builder_s(sb, buf, (size_t)file_size);
    free(buf);
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
        glDeleteShader(shader);
        return 0;
    }
    return shader;
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

    glBindAttribLocation(program, 0, "aPos");
    glBindAttribLocation(program, 1, "aTexCoord");

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

    return program;
}

static GLuint bm_make_quad_cord(boomer_t *b)
{
    float w = (float)b->width;
    float h = (float)b->height;
    float quad_cord[] = {
        0.0f, 0.0f,  0.0f, 1.0f,
        w,    0.0f,  1.0f, 1.0f,
        w,    h,     1.0f, 0.0f,
        0.0f, h,     0.0f, 0.0f,
    };

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_cord), quad_cord, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    return vao;
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

    BM_ASSERT(b->width != 0 && b->height != 0);
    return BM_SUCCESS;
}

static bm_error_t bm_initialize_glx(boomer_t *b)
{
    GLint attr[] = {
        GLX_RGBA, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        None
    };

    XVisualInfo *visual_info = glXChooseVisual(b->dpy, b->screen, attr);
    if (!visual_info) return BM_GLX_VISUAL_ERR;

    Colormap color_map = XCreateColormap(b->dpy, b->root,
                                         visual_info->visual, AllocNone);
    XSetWindowAttributes attributes = {0};
    attributes.colormap = color_map;

    attributes.event_mask = KeyPressMask
                            | ButtonPressMask | ButtonReleaseMask
                            | PointerMotionMask | ExposureMask;

    unsigned long valuemask = CWColormap | CWEventMask;
    if (!b->windowed)
    {
        attributes.override_redirect = True;
        attributes.save_under = True;
        valuemask |= CWOverrideRedirect | CWSaveUnder;
    }

    b->app = XCreateWindow(b->dpy,   b->root, 0, 0,
                           b->width, b->height,
                           0, visual_info->depth,
                           InputOutput, visual_info->visual,
                           valuemask, &attributes);
    if (b->app == 0) return BM_APP_WIN_ERR;

    XStoreName(b->dpy, b->app, "boomer");
    XClassHint hints = { .res_name = (char *)"boomer", .res_class = (char *)"Boomer" };
    XSetClassHint(b->dpy, b->app, &hints);

    b->wm_delete = XInternAtom(b->dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(b->dpy, b->app, &b->wm_delete, 1);

    XMapRaised(b->dpy, b->app);
    XSync(b->dpy, False);

    b->context = glXCreateContext(b->dpy, visual_info,
                                  NULL, True);
    XFree(visual_info);

    glXMakeCurrent(b->dpy, b->app, b->context);
    glewExperimental = True;

    glew_err = glewInit();
    if (glew_err != GLEW_OK) return BM_GLEW_ERR;

    glViewport(0, 0, b->width, b->height);
    return BM_SUCCESS;
}

static GLuint bm_upload_raw_image(XImage *raw)
{
    BM_ASSERT(raw->bits_per_pixel == 32 || raw->bits_per_pixel == 24);
    GLenum format = raw->bits_per_pixel == 32 ? GL_BGRA : GL_BGR;
    GLint  stride = raw->bytes_per_line / (raw->bits_per_pixel / 8);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 raw->width, raw->height, 0,
                 format, GL_UNSIGNED_BYTE, raw->data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

static void bm_get_uniforms(GLuint prog, bm_uniforms_t *u)
{
    u->camera_pos      = glGetUniformLocation(prog, "cameraPos");
    u->camera_scale    = glGetUniformLocation(prog, "cameraScale");
    u->window_size     = glGetUniformLocation(prog, "windowSize");
    u->screenshot_size = glGetUniformLocation(prog, "screenshotSize");
    u->cursor_pos      = glGetUniformLocation(prog, "cursorPos");
    u->fl_shadow       = glGetUniformLocation(prog, "flShadow");
    u->fl_radius       = glGetUniformLocation(prog, "flRadius");
    u->mirror          = glGetUniformLocation(prog, "mirror");
    u->tex             = glGetUniformLocation(prog, "tex");
}

static void bm_camera_update(bm_camera_t *cam, float dt, const bm_mouse_t *mouse, bm_vec2f_t window_size)
{
    if (fabsf(cam->delta_scale) > 0.5f)
    {
        bm_vec2f_t p0 = {
            (cam->scale_pivot.x - window_size.x * 0.5f) / cam->scale,
            (cam->scale_pivot.y - window_size.y * 0.5f) / cam->scale,
        };
        cam->scale += cam->delta_scale * dt;
        if (cam->scale < BM_MIN_SCALE) cam->scale = BM_MIN_SCALE;
        bm_vec2f_t p1 = {
            (cam->scale_pivot.x - window_size.x * 0.5f) / cam->scale,
            (cam->scale_pivot.y - window_size.y * 0.5f) / cam->scale,
        };
        cam->pos.x += p0.x - p1.x;
        cam->pos.y += p0.y - p1.y;

        cam->delta_scale -= cam->delta_scale * dt * BM_SCALE_FRICTION;
    }

    if (!mouse->drag && bm_vec2f_length(cam->velocity) > BM_VELOCITY_THRESHOLD)
    {
        cam->pos.x += cam->velocity.x * dt;
        cam->pos.y += cam->velocity.y * dt;
        cam->velocity.x -= cam->velocity.x * dt * BM_DRAG_FRICTION;
        cam->velocity.y -= cam->velocity.y * dt * BM_DRAG_FRICTION;
    }
}

static void bm_flashlight_update(bm_flashlight_t *fl, float dt)
{
    if (fabsf(fl->delta_radius) > 1.0f)
    {
        fl->radius += fl->delta_radius * dt;
        if (fl->radius < 0.0f) fl->radius = 0.0f;
        fl->delta_radius -= fl->delta_radius * dt * BM_FL_RADIUS_DECELERATION;
    }

    if (fl->enabled)
    {
        fl->shadow += BM_FL_SHADOW_SPEED * dt;
        if (fl->shadow > BM_FL_SHADOW) fl->shadow = BM_FL_SHADOW;
    }
    else
    {
        fl->shadow -= BM_FL_SHADOW_SPEED * dt;
        if (fl->shadow < 0.0f) fl->shadow = 0.0f;
    }
}

static bool bm_is_animating(const bm_camera_t *cam, const bm_flashlight_t *fl, const bm_mouse_t *mouse)
{
    if (fabsf(cam->delta_scale) > 0.5f) return true;
    if (!mouse->drag && bm_vec2f_length(cam->velocity) > BM_VELOCITY_THRESHOLD) return true;
    if (fabsf(fl->delta_radius) > 1.0f) return true;
    float target = fl->enabled ? BM_FL_SHADOW : 0.0f;
    if (fl->shadow != target) return true;
    return false;
}

static void bm_scroll(bm_camera_t *cam, bm_flashlight_t *fl, const bm_mouse_t *mouse,
                      unsigned int state, float dir)
{
    if ((state & ControlMask) && fl->enabled)
    {
        fl->delta_radius += BM_FL_DELTA_RADIUS * dir;
    }
    else
    {
        cam->delta_scale += BM_SCROLL_SPEED * dir;
        cam->scale_pivot = mouse->curr;
    }
}

static float bm_dt(struct timespec *prev)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    float dt = (float)(now.tv_sec - prev->tv_sec)
             + (float)(now.tv_nsec - prev->tv_nsec) * 1e-9f;
    *prev = now;
    if (dt > 1.0f / 30.0f) dt = 1.0f / 30.0f;
    return dt;
}

void bm_get_error(bm_error_t err)
{
    #define BM_LOG_RETURN(msg)              \
    {                                       \
            fprintf(stderr, "%s\n", msg);   \
            return;                         \
    }

    switch(err)
    {
        case BM_SUCCESS: { return; }
        case BM_DPY_ERR: BM_LOG_RETURN("Could not initalize X11 display");
        case BM_ROOT_WIN_ERR: BM_LOG_RETURN("Could not initalize X11 root window");
        case BM_APP_WIN_ERR: BM_LOG_RETURN("Could not initalize X11 application window");
        case BM_GLX_VISUAL_ERR: BM_LOG_RETURN("Could not initalize GLX ChooseVisual failed");
        case BM_GLEW_ERR: BM_LOG_RETURN((const char *)glewGetErrorString(glew_err));
    }

    #undef BM_LOG_RETURN
}

static void bm_usage(void)
{
    fprintf(stderr,
            "Usage: boomer [OPTIONS]\n"
            "  -d <seconds>  delay startup by <seconds>\n"
            "  -w            windowed mode instead of fullscreen\n"
            "  -s <dir>      load vert.glsl/frag.glsl from <dir> instead of embedded shaders\n"
            "  -h            show this help and exit\n");
    exit(1);
}

int main(int argc, char **argv)
{
    boomer_t b = {0};
    double delay_sec = 0.0;
    const char *shader_dir = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-w") == 0)
        {
            b.windowed = true;
        }
        else if (strcmp(argv[i], "-d") == 0)
        {
            if (i + 1 >= argc) bm_usage();
            delay_sec = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            if (i + 1 >= argc) bm_usage();
            shader_dir = argv[++i];
        }
        else
        {
            bm_usage();
        }
    }

    if (delay_sec > 0.0)
    {
        struct timespec ts = {
            .tv_sec = (time_t)delay_sec,
            .tv_nsec = (long)((delay_sec - (double)(time_t)delay_sec) * 1e9),
        };
        nanosleep(&ts, NULL);
    }

    BM_ERROR_CHECK(bm_initialize_x11(&b));

    XImage *boomer_captured = bm_capture_screenshot(&b);
    BM_ERROR_CHECK(bm_initialize_glx(&b));

    bm_string_builder_t vert_sb = {0};
    bm_string_builder_t frag_sb = {0};
    if (shader_dir)
    {
        bm_string_builder_t path_sb = {0};
        bm_append_to_builder_s(&path_sb, shader_dir, strlen(shader_dir));
        bm_append_to_builder_s(&path_sb, "/vert.glsl", strlen("/vert.glsl"));
        bm_read_shader(&vert_sb, path_sb.items);
        path_sb.count = 0;
        bm_append_to_builder_s(&path_sb, shader_dir, strlen(shader_dir));
        bm_append_to_builder_s(&path_sb, "/frag.glsl", strlen("/frag.glsl"));
        bm_read_shader(&frag_sb, path_sb.items);
        free(path_sb.items);
    }
    else
    {
        bm_append_to_builder_s(&vert_sb, BM_VERT_SHADER_SRC, strlen(BM_VERT_SHADER_SRC));
        bm_append_to_builder_s(&frag_sb, BM_FRAG_SHADER_SRC, strlen(BM_FRAG_SHADER_SRC));
    }

    GLuint prog = bm_make_program(&vert_sb, &frag_sb);
    BM_ASSERT(prog != 0);
    glUseProgram(prog);

    bm_uniforms_t u = {0};
    bm_get_uniforms(prog, &u);

    GLuint verticies = bm_make_quad_cord(&b);
    glBindVertexArray(verticies);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    float shot_w = (float)boomer_captured->width;
    float shot_h = (float)boomer_captured->height;
    GLuint tex = bm_upload_raw_image(boomer_captured);
    BM_UNUSED(tex);
    XDestroyImage(boomer_captured);

#ifdef BM_DEBUG_STDOUT_LOG
    BOOMER_LOG_STDOUT("Dpy %p", (void *)b.dpy);
    BOOMER_LOG_STDOUT("Root %ld",b.root);
    BOOMER_LOG_STDOUT("Screen %d", b.screen);
    BOOMER_LOG_STDOUT("Width %d", b.width);
    BOOMER_LOG_STDOUT("Height %d", b.height);
#endif

    bm_camera_t camera = { .scale = 1.0f };
    bm_flashlight_t flashlight = { .radius = BM_FL_RADIUS };
    bm_mouse_t mouse = {0};
    bool quit = false;
    bool mirror = false;

    // Remember where focus was so quitting hands it back (boomer.nim TODO #78 style).
    Window origin_focus = None;
    int revert_to = 0;
    XGetInputFocus(b.dpy, &origin_focus, &revert_to);

    struct timespec frame_clock;
    clock_gettime(CLOCK_MONOTONIC, &frame_clock);

    while (!quit)
    {
        if (!b.windowed)
        {
            XSetInputFocus(b.dpy, b.app, RevertToParent, CurrentTime);
        }

        XWindowAttributes wa;
        XGetWindowAttributes(b.dpy, b.app, &wa);
        glViewport(0, 0, wa.width, wa.height);
        bm_vec2f_t window_size = { (float)wa.width, (float)wa.height };

        if (!bm_is_animating(&camera, &flashlight, &mouse) && !XPending(b.dpy))
        {
            XEvent peek;
            XPeekEvent(b.dpy, &peek);
            clock_gettime(CLOCK_MONOTONIC, &frame_clock);
        }

        while (XPending(b.dpy))
        {
            XEvent e;
            XNextEvent(b.dpy, &e);
            switch (e.type)
            {
                case Expose: break;

                case ClientMessage:
                {
                    if ((Atom)e.xclient.data.l[0] == b.wm_delete) quit = true;
                } break;

                case KeyPress:
                {
                    KeySym key = XLookupKeysym(&e.xkey, 0);
                    switch (key)
                    {
                        case XK_q:
                        case XK_Escape: quit = true; break;
                        case XK_equal: bm_scroll(&camera, &flashlight, &mouse, e.xkey.state,  1.0f); break;
                        case XK_minus: bm_scroll(&camera, &flashlight, &mouse, e.xkey.state, -1.0f); break;
                        case XK_0:
                        {
                            camera.scale = 1.0f;
                            camera.delta_scale = 0.0f;
                            camera.pos = (bm_vec2f_t){0, 0};
                            camera.velocity = (bm_vec2f_t){0, 0};
                            mirror = false;
                        } break;
                        case XK_f: flashlight.enabled = !flashlight.enabled; break;
                        case XK_m:
                        {
                            camera.pos.x += shot_w / camera.scale
                                          - 2.0f * (mouse.curr.x / camera.scale + camera.pos.x);
                            mirror = !mirror;
                        } break;
                        default: break;
                    }
                } break;

                case ButtonPress:
                {
                    switch (e.xbutton.button)
                    {
                        case Button1:
                        {
                            mouse.prev = mouse.curr;
                            mouse.drag = true;
                            camera.velocity = (bm_vec2f_t){0, 0};
                        } break;
                        case Button4: bm_scroll(&camera, &flashlight, &mouse, e.xbutton.state,  1.0f); break;
                        case Button5: bm_scroll(&camera, &flashlight, &mouse, e.xbutton.state, -1.0f); break;
                        default: break;
                    }
                } break;

                case ButtonRelease:
                {
                    if (e.xbutton.button == Button1) mouse.drag = false;
                } break;

                case MotionNotify:
                {
                    mouse.curr.x = (float)e.xmotion.x;
                    mouse.curr.y = (float)e.xmotion.y;
                    if (mouse.drag)
                    {
                        bm_vec2f_t delta = {
                            (mouse.prev.x - mouse.curr.x) / camera.scale,
                            (mouse.prev.y - mouse.curr.y) / camera.scale,
                        };
                        camera.pos.x += delta.x;
                        camera.pos.y += delta.y;
                        camera.velocity.x = delta.x * 60.0f;
                        camera.velocity.y = delta.y * 60.0f;
                    }
                    mouse.prev = mouse.curr;
                } break;

                default: break;
            }
        }

        float dt = bm_dt(&frame_clock);
        bm_camera_update(&camera, dt, &mouse, window_size);
        bm_flashlight_update(&flashlight, dt);

        glClear(GL_COLOR_BUFFER_BIT);
        glUniform2f(u.camera_pos, camera.pos.x, camera.pos.y);
        glUniform1f(u.camera_scale, camera.scale);
        glUniform2f(u.window_size, window_size.x, window_size.y);
        glUniform2f(u.screenshot_size, shot_w, shot_h);
        glUniform2f(u.cursor_pos, mouse.curr.x, mouse.curr.y);
        glUniform1f(u.fl_shadow, flashlight.shadow);
        glUniform1f(u.fl_radius, flashlight.radius);
        glUniform1i(u.mirror, mirror);
        glUniform1i(u.tex, 0);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glXSwapBuffers(b.dpy, b.app);
        glFinish();
    }

    free(vert_sb.items);
    free(frag_sb.items);
    XSetInputFocus(b.dpy, origin_focus, RevertToParent, CurrentTime);
    XSync(b.dpy, False);
    glXMakeCurrent(b.dpy, None, NULL);
    glXDestroyContext(b.dpy, b.context);
    XDestroyWindow(b.dpy, b.app);
    XCloseDisplay(b.dpy);
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
