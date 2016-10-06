/*********************************************************************************************************************/
/*                                                  /===-_---~~~~~~~~~------____                                     */
/*                                                 |===-~___                _,-'                                     */
/*                  -==\\                         `//~\\   ~~~~`---.___.-~~                                          */
/*              ______-==|                         | |  \\           _-~`                                            */
/*        __--~~~  ,-/-==\\                        | |   `\        ,'                                                */
/*     _-~       /'    |  \\                      / /      \      /                                                  */
/*   .'        /       |   \\                   /' /        \   /'                                                   */
/*  /  ____  /         |    \`\.__/-~~ ~ \ _ _/'  /          \/'                                                     */
/* /-'~    ~~~~~---__  |     ~-/~         ( )   /'        _--~`                                                      */
/*                   \_|      /        _)   ;  ),   __--~~                                                           */
/*                     '~~--_/      _-~/-  / \   '-~ \                                                               */
/*                    {\__--_/}    / \\_>- )<__\      \                                                              */
/*                    /'   (_/  _-~  | |__>--<__|      |                                                             */
/*                   |0  0 _/) )-~     | |__>--<__|     |                                                            */
/*                   / /~ ,_/       / /__>---<__/      |                                                             */
/*                  o o _//        /-~_>---<__-~      /                                                              */
/*                  (^(~          /~_>---<__-      _-~                                                               */
/*                 ,/|           /__>--<__/     _-~                                                                  */
/*              ,//('(          |__>--<__|     /                  .----_                                             */
/*             ( ( '))          |__>--<__|    |                 /' _---_~\                                           */
/*          `-)) )) (           |__>--<__|    |               /'  /     ~\`\                                         */
/*         ,/,'//( (             \__>--<__\    \            /'  //        ||                                         */
/*       ,( ( ((, ))              ~-__>--<_~-_  ~--____---~' _/'/        /'                                          */
/*     `~/  )` ) ,/|                 ~-_~>--<_/-__       __-~ _/                                                     */
/*   ._-~//( )/ )) `                    ~~-'_/_/ /~~~~~~~__--~                                                       */
/*    ;'( ')/ ,)(                              ~~~~~~~~~~                                                            */
/*   ' ') '( (/                                                                                                      */
/*     '   '  `                                                                                                      */
/*********************************************************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assets/assetload.h>
#include <glad/glad.h>
#include <tinycthread.h>
#include "mainloop.h"
#include "window.h"
#include <emproc/filter.h>

/* Convinience macro */
#define GLSL(src) "#version 330 core\n" #src

static const char* preview_vshdr = GLSL(
    layout (location = 0) in vec3 position;

    void main()
    {
        gl_Position = vec4(position, 1.0f);
    }
);

static const char* preview_fshdr = GLSL(
    out vec4 color;

    uniform ivec2 gScreenSize;
    uniform sampler2D tex;

    void main()
    {
        vec2 UVCoords = gl_FragCoord.xy / gScreenSize;
        color = vec4(texture(tex, UVCoords).rgb, 1.0);
    }
);

static GLuint build_preview_shader()
{
    GLuint vid = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vid, 1, &preview_vshdr, 0);
    glCompileShader(vid);

    GLuint fid = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fid, 1, &preview_fshdr, 0);
    glCompileShader(fid);

    GLuint pid = glCreateProgram();
    glAttachShader(pid, vid);
    glAttachShader(pid, fid);
    glLinkProgram(pid);

    glDeleteShader(vid);
    glDeleteShader(fid);
    return pid;
}

static void render_quad()
{
    GLfloat quadVert[] =
    {
       -1.0f,  1.0f, 0.0f,
       -1.0f, -1.0f, 0.0f,
        1.0f,  1.0f, 0.0f,
        1.0f, -1.0f, 0.0f
    };

    GLuint quadVao;
    glGenVertexArrays(1, &quadVao);
    glBindVertexArray(quadVao);

    GLuint quadVbo;
    glGenBuffers(1, &quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVert), &quadVert, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &quadVbo);
    glDeleteVertexArrays(1, &quadVao);
}

static void APIENTRY gl_debug_proc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* user_param)
{
    (void) source;
    (void) id;
    (void) severity;
    (void) length;
    (void) user_param;

    if (type == GL_DEBUG_TYPE_ERROR) {
        fprintf(stderr, "%s", message);
        exit(1);
    }
}

struct context {
    int* should_terminate;
    struct window* wnd;
    struct image* in;
    struct image* out;
    int processing;
};

static void on_key(struct window* wnd, int key, int scancode, int action, int mods)
{
    (void)scancode; (void)mods;
    struct context* ctx = window_get_userdata(wnd);
    if (action == KEY_ACTION_RELEASE && key == KEY_ESCAPE)
        *(ctx->should_terminate) = 1;
}

static int filter_thrd(void* arg)
{
    struct context* ctx = (struct context*) arg;
    ctx->processing = 1;
    irradiance_filter(ctx->out->width, ctx->out->height, ctx->out->channels, ctx->in->data, ctx->out->data);
    ctx->processing = 0;
    return 0;
}

static void init(struct context* ctx)
{
    /* Create window */
    const char* title = "demo";
    int width = 800, height = 600, mode = 0;
    ctx->wnd = window_create(title, width, height, mode);

    /* Associate context to be accessed from callback functions */
    window_set_userdata(ctx->wnd, ctx);

    /* Set event callbacks */
    struct window_callbacks wnd_callbacks;
    memset(&wnd_callbacks, 0, sizeof(struct window_callbacks));
    wnd_callbacks.key_cb = on_key;
    window_set_callbacks(ctx->wnd, &wnd_callbacks);

    /* Setup OpenGL debug handler */
    if (glDebugMessageCallback)
        glDebugMessageCallback(gl_debug_proc, ctx);

    /* Set current processing state to idle */
    ctx->processing = 0;

    /* Load input image */
    ctx->in = image_from_file("ext/stormydays_large.jpg");
    /* Create empty output image */
    ctx->out = image_blank(ctx->in->width, ctx->in->height, ctx->in->channels);
    int h = ctx->out->height;
    int w = ctx->out->width;
    unsigned char* data = ctx->out->data;
    for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
            unsigned char* pix = data + (i * w + j) * 3;
            pix[0] = i * 255 / h;
            pix[1] = j * 255 / w;
            pix[2] = i * j * 255 / (w * h);
        }
    }
}

static void update(void* userdata, float dt)
{
    (void) dt;
    struct context* ctx = userdata;
    /* Process input events */
    window_update(ctx->wnd);
}

static void render(void* userdata, float interpolation)
{
    (void) interpolation;
    struct context* ctx = userdata;

    if (!ctx->processing)
        return;

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Create texture in GPU */
    struct image* img = ctx->out;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(
        GL_TEXTURE_2D, 0,
        img->channels == 4 ? GL_RGBA : GL_RGB,
        img->width, img->height, 0,
        img->channels == 4 ? GL_RGBA : GL_RGB,
        GL_UNSIGNED_BYTE, img->data);

    /* Create and setup preview shader */
    GLuint sh = build_preview_shader();
    glUseProgram(sh);
    glUniform1i(glGetUniformLocation(sh, "tex"), 0);
    glUniform2i(glGetUniformLocation(sh, "gScreenSize"), 800, 600);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    /* Render quad */
    render_quad();

    /* Free GPU resources */
    glUseProgram(0);
    glDeleteProgram(sh);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tex);

    /* Show rendered contents from the backbuffer */
    window_swap_buffers(ctx->wnd);
}

static void shutdown(struct context* ctx)
{
    image_delete(ctx->out);
    image_delete(ctx->in);

    /* Destroy window */
    window_destroy(ctx->wnd);
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    /* Startup */
    struct context ctx;
    init(&ctx);

    /* Launch processing thread */
    thrd_t rt;
    thrd_create(&rt, filter_thrd, &ctx);
    thrd_detach(rt);

    /* Setup mainloop parameters */
    struct mainloop_data mld;
    memset(&mld, 0, sizeof(struct mainloop_data));
    mld.max_frameskip = 5;
    mld.updates_per_second = 60;
    mld.update_callback = update;
    mld.render_callback = render;
    mld.userdata = &ctx;
    ctx.should_terminate = &mld.should_terminate;

    /* Run mainloop */
    mainloop(&mld);

    /* Dealloc resources */
    shutdown(&ctx);

    return 0;
}