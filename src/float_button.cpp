#include "float_button.h"
#include "app.h"
#include "window_listener.h"
#include <QMouseEvent>
#include <QTimer>

FloatButton::FloatButton() : QPushButton("一键拔线"), _logger(spdlog::get("skipper")) {
    windowListener = new HearthStoneWindowListener(this);
    setStyleSheet("background: transparent;font-size:30px;color:red");

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowStayOnTop(this);
    connect(windowListener, &HearthStoneWindowListener::onAppLaunch, this, [this](QRect rect) {
        moveToWindow(rect);
        setWindowStayOnTop(this);
        setVisible(true);
        qDebug() << "app launch rect=" << rect;
    });
    connect(windowListener, &HearthStoneWindowListener::onAppGetFocus, this, [this](QRect rect) {
        moveToWindow(rect);
        setWindowStayOnTop(this);
        setVisible(true);
    });
    connect(windowListener, &HearthStoneWindowListener::onAppLoseFocus, this, [this]() {
        // 延迟一小段时间再隐藏，避免点击悬浮按钮时因焦点切换导致按钮消失
        QTimer::singleShot(100, this, [this]() {
            if (!underMouse()) {
                setVisible(false);
            }
        });
    });
    connect(windowListener, &HearthStoneWindowListener::onAppMove, this, [this](QRect rect) {
        moveToWindow(rect);
        setWindowStayOnTop(this);
        setVisible(true);
    });
    connect(windowListener, &HearthStoneWindowListener::onAppTerminate, this, [this]() { setVisible(false); });
}

void FloatButton::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        onBtnClicked();
    }
}

void FloatButton::moveToWindow(const QRect &windowRect) {
    static constexpr int kPaddingRight = 20;
    static constexpr int kPaddingTop = 40;
    ensurePolished();
    int btnWidth = sizeHint().width();
    int x = windowRect.right() - btnWidth - kPaddingRight;
    int y = windowRect.y() + kPaddingTop;
    resize(sizeHint());
    move(x, y);
}
void FloatButton::onBtnClicked() {
    SPDLOG_LOGGER_INFO(_logger,"clicked");
    connect(
        App::skipper, &Skipper::skipFinished, this,
        [this](bool success) {
            setText(success ? "拔线成功" : "拔线失败");
            QTimer::singleShot(1000, this, [this]() { setText("一键拔线"); });
        },
        Qt::SingleShotConnection);
    App::skipper->skip();
}
