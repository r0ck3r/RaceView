#pragma once

#include <QOpenGLWidget>
#include <mpv/render_gl.h>

class MpvPlayer;

class MpvWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit MpvWidget(MpvPlayer *player, QWidget *parent = nullptr);
    ~MpvWidget() override;

    bool isRenderReady() const { return m_render != nullptr; }

signals:
    void doubleClicked();

protected:
    void initializeGL() override;
    void paintGL() override;
    void mouseDoubleClickEvent(QMouseEvent *ev) override;

private:
    static void *getProcAddress(void *ctx, const char *name);
    static void onUpdate(void *ctx);

    MpvPlayer *m_player;
    mpv_render_context *m_render = nullptr;
};
