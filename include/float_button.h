#pragma once

#include "logger.h"
#include "window_listener.h"
#include <QPushButton>

class FloatButton : public QPushButton {
    Q_OBJECT
  public:
    FloatButton();

  protected:
    void mousePressEvent(QMouseEvent *event) override;

  public slots:
    void onBtnClicked();

  private:
    void moveToWindow(const QRect &windowRect);

  private:
    HearthStoneWindowListener *windowListener;
    std::shared_ptr<spdlog::logger> _logger;
};