#pragma once
#include <QObject>
#include <QTimer>
#include <QPointer>
#include <QPushButton>
// Recognizes repeated mouse/touch presses; the host handles motion commands.
class LongPressRepeatFilter : public QObject {
    Q_OBJECT
public:
    explicit LongPressRepeatFilter(QObject* parent=nullptr);
    bool eventFilter(QObject* obj,QEvent* event) override;
    void touchEndEvent();
    QTimer timer;
    QPointer<QPushButton> senderPushButton;
    int jogKeepCount=0;
public slots:
    void handleLongPress_slot();
signals:
    void stopRequested();
};
