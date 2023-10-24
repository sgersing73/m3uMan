#-------------------------------------------------
#
# Project created by QtCreator 2023-10-12T13:36:11
#
#-------------------------------------------------

QMAKE_CXXFLAGS = -Wno-unused-parameter

QT       += sql core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = m3uMan
TEMPLATE = app

LIBS       += -L../m3uMan/win32/VLC-Qt_1.1.0_win32_mingw/lib -lVLCQtCore -lVLCQtWidgets
INCLUDEPATH += ../m3uMan/win32/VLC-Qt_1.1.0_win32_mingw/include
LIBS       += -lVLCQtCore -lVLCQtWidgets

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        EqualizerDialog.cpp \
        main.cpp \
        mainwindow.cpp \
        dbmanager.cpp \
        filedownloader.cpp \
        gatherdata.cpp

HEADERS += \
        EqualizerDialog.h \
        mainwindow.h \
        dbmanager.h \
        filedownloader.h \
        gatherdata.h

FORMS += \
        EqualizerDialog.ui \
        mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    m3uMan.qrc
