#include "MpvWidget.h"
#include "MpvPlayer.h"

#include <QOpenGLContext>
#include <QMetaObject>
#include <QMouseEvent>
#include <qopengl.h>
#include <cstdio>

MpvWidget::MpvWidget(MpvPlayer *player, QWidget *parent)
    : QOpenGLWidget(parent), m_player(player) {}

MpvWidget::~MpvWidget() {
    makeCurrent();
    if (m_render) {
        mpv_render_context_set_update_callback(m_render, nullptr, nullptr);
        mpv_render_context_free(m_render);
        m_render = nullptr;
    }
    doneCurrent();
}

void *MpvWidget::getProcAddress(void *ctx, const char *name) {
    return reinterpret_cast<void *>(
        static_cast<QOpenGLContext *>(ctx)->getProcAddress(name));
}

void MpvWidget::onUpdate(void *ctx) {
    QMetaObject::invokeMethod(static_cast<MpvWidget *>(ctx), "update",
                              Qt::QueuedConnection);
}

void MpvWidget::initializeGL() {
    if (qEnvironmentVariableIsSet("RACEVIEW_HEADLESS"))
        return;

    mpv_opengl_init_params glInit{};
    glInit.get_proc_address = &MpvWidget::getProcAddress;
    glInit.get_proc_address_ctx = QOpenGLContext::currentContext();

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE,
         const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    if (mpv_render_context_create(&m_render, m_player->handle(), params) >= 0)
        mpv_render_context_set_update_callback(m_render, &MpvWidget::onUpdate,
                                               this);
    else {
        m_render = nullptr;
        fprintf(stderr,
                "MpvWidget: не удалось создать mpv render context "
                "(проблема с OpenGL/драйвером) — видео показываться не будет\n");
    }
}

void MpvWidget::paintGL() {
    if (!m_render)
        return;

    mpv_opengl_fbo fbo{};
    fbo.fbo = static_cast<int>(defaultFramebufferObject());
    fbo.w = width();
    fbo.h = height();
    fbo.internal_format = GL_RGBA8;

    int flipY = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flipY},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    mpv_render_context_render(m_render, params);
}

void MpvWidget::mouseDoubleClickEvent(QMouseEvent *ev) {
    if (ev->button() == Qt::LeftButton)
        emit doubleClicked();
    QOpenGLWidget::mouseDoubleClickEvent(ev);
}
