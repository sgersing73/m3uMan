#-------------------------------------------------
#
# Project created by QtCreator 2023-10-12T13:36:11
#
#-------------------------------------------------

QMAKE_CXXFLAGS = -Wno-unused-parameter -Wno-attributes

QT       += sql core gui network testlib

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

win32 {
QT          += winextras
LIBS        += -L../m3uMan/win32/VLC-Qt_1.1.0_win32_mingw/lib
INCLUDEPATH += ../m3uMan/win32/VLC-Qt_1.1.0_win32_mingw/include
}

TARGET = QtM3uMan
TEMPLATE = app

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
#        downloadmanager.cpp \
        main.cpp \
        mainwindow.cpp \
        dbmanager.cpp \
        filedownloader.cpp

HEADERS += \
        EqualizerDialog.h \
 #       downloadmanager.h \
        mainwindow.h \
        dbmanager.h \
        filedownloader.h

FORMS += \
        EqualizerDialog.ui \
        mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    m3uMan.qrc

DISTFILES += \
    docs/license.txt \
    docs/resources.txt \
    "playlists/Radio Absolut.m3u" \
    "playlists/Telekom Magenta TV.m3u" \
    "playlists/Planet Plus.m3u" \
    "playlists/Radio Antenne Bayern.m3u" \
    stylsheets/Adaptic/Adaptic.png \
    stylsheets/Adaptic/Adaptic.qss \
    stylsheets/Adaptic/License.txt \
    stylsheets/Combinear/Combinear.png \
    stylsheets/Combinear/Combinear.qss \
    stylsheets/Combinear/License.txt \
    stylsheets/DeepBox/DeepBox.png \
    stylsheets/DeepBox/DeepBox.qss \
    stylsheets/DeepBox/License.txt \
    stylsheets/Irrorater/Irrorater.png \
    stylsheets/Irrorater/Irrorater.qss \
    stylsheets/Irrorater/License.txt \
    stylsheets/application.qss \
    stylsheets/nostyle.qss
