#pragma once

#include <functional>
#include <QObject>
#include <QTimer>

class QWidget;

void setWindowStayOnTop(QWidget* widget);

class HearthStoneWindowListener: public QObject{
    Q_OBJECT
  public:
    HearthStoneWindowListener(QObject* parent = nullptr);
    ~HearthStoneWindowListener() override;

  signals:
    void onAppLaunch(QRect rect);
    void onAppGetFocus(QRect rect);
    void onAppLoseFocus();
    void onAppMove(QRect rect);
    void onAppTerminate();

 private:
    void *_observer = nullptr;  // AppLaunchObserver*
};