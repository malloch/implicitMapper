#-------------------------------------------------
#
# Project created by QtCreator 2014-11-07T16:55:18
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ImplicitMapper
TEMPLATE = app

CONFIG += c++11

SOURCES += main.cpp\
        mainwindow.cpp \
    display.cpp \
    logic.cpp \
    RumelhartNetwork.c

HEADERS  += mainwindow.h \
    display.h \
    logic.h \
    RumelhartNetwork.h

FORMS    += mainwindow.ui


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../../../usr/local/lib/release/ -lmapper
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../../../usr/local/lib/debug/ -lmapper
else:unix: LIBS += -L$$PWD/../../../../../../usr/local/lib/ -lmapper

INCLUDEPATH += $$PWD/../../../../../../usr/local/include/mapper
DEPENDPATH += $$PWD/../../../../../../usr/local/include/mapper

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../../../usr/local/lib/release/ -llo
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../../../usr/local/lib/debug/ -llo
else:unix: LIBS += -L$$PWD/../../../../../../usr/local/lib/ -llo

INCLUDEPATH += $$PWD/../../../../../../usr/local/include
DEPENDPATH += $$PWD/../../../../../../usr/local/include
