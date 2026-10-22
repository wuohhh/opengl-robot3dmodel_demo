#include "robot3d/shell/input/LongPressRepeatFilter.h"
#include <QEvent>
LongPressRepeatFilter::LongPressRepeatFilter(QObject* parent) : QObject(parent) { timer.setInterval(100); connect(&timer,&QTimer::timeout,this,&LongPressRepeatFilter::handleLongPress_slot); }
bool LongPressRepeatFilter::eventFilter(QObject* obj,QEvent* event) {
    if(event->type()==QEvent::TouchBegin || event->type()==QEvent::MouseButtonPress) { senderPushButton=qobject_cast<QPushButton*>(obj); jogKeepCount=0; timer.start(); }
    if(event->type()==QEvent::TouchEnd || event->type()==QEvent::TouchCancel || event->type()==QEvent::MouseButtonRelease || event->type()==QEvent::Hide) touchEndEvent();
    return QObject::eventFilter(obj,event);
}
void LongPressRepeatFilter::handleLongPress_slot() { if(senderPushButton) { senderPushButton->click(); ++jogKeepCount; } }
void LongPressRepeatFilter::touchEndEvent() { timer.stop(); if(jogKeepCount>2) emit stopRequested(); jogKeepCount=0; senderPushButton=nullptr; }
