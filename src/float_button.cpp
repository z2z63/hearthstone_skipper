#include "float_button.h"
#include "app.h"
#include "window_listener.h"
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

FloatButton::FloatButton() : QPushButton("一键拔线"), _logger(spdlog::get("skipper")) {
    windowListener = new HearthStoneWindowListener(this);

    QFont font;
    font.setFamily("LiShu");
    font.setPixelSize(42);
    setFont(font);

    setStyleSheet("background: transparent; color: white;");

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

void FloatButton::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect().adjusted(2, 2, -2, -2);
    QPainterPath path;
    path.addText(rect.bottomLeft(), font(), text());

    // 先画黑色描边
    painter.setPen(QPen(Qt::black, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    // 再画白色填充
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawPath(path);
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
    SPDLOG_LOGGER_INFO(_logger, "float_button_clicked");
    connect(
        App::skipper, &Skipper::skipFinished, this,
        [this](bool success) {
            setText(success ? "拔线成功" : "拔线失败");
            QTimer::singleShot(1000, this, [this]() { setText("一键拔线"); });
        },
        Qt::SingleShotConnection);
    App::skipper->skip();
}
