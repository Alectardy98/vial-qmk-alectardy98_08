# Copyright 2020 Purdea Andrei
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

#-------------------------------------------------
#
# Project created by QtCreator 2020-05-02T21:08:53
#
#-------------------------------------------------

QT += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = util
TEMPLATE = app

# Emit warnings when deprecated Qt APIs are used.
DEFINES += QT_DEPRECATED_WARNINGS

CONFIG += c++11

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    monitorwindow.cpp \
    communication.cpp \
    device.cpp \
    hidthread.cpp \
    kbd_defs.cpp \
    signal_level.cpp \
    columntester.cpp \
    rowdactester.cpp

HEADERS += \
    mainwindow.h \
    monitorwindow.h \
    communication.h \
    device.h \
    hidthread.h \
    kbd_defs.h \
    signal_level.h \
    columntester.h \
    rowdactester.h

FORMS += \
    mainwindow.ui \
    monitorwindow.ui \
    signal_level.ui \
    columntester.ui \
    rowdactester.ui

# Embedded Qt resources, including Cap-Util.png.
RESOURCES += resources.qrc


# ============================================================
# Linux
# ============================================================

unix:!macx {
    LIBS += -lhidapi-hidraw
    INCLUDEPATH += /usr/include/hidapi

    # Automatically package the completed Linux build as an AppImage.
    appimage.target = appimage
    appimage.depends = $(TARGET)
    appimage.commands = $$PWD/build_appimage.sh
    QMAKE_EXTRA_TARGETS += appimage

    QMAKE_POST_LINK += $$PWD/build_appimage.sh;
}


# ============================================================
# macOS
# ============================================================

macx {
    TARGET = Cap-Util

    # Apple Silicon Homebrew
    exists(/opt/homebrew/opt/hidapi) {
        INCLUDEPATH += /opt/homebrew/opt/hidapi/include/hidapi
        LIBS += -L/opt/homebrew/opt/hidapi/lib -lhidapi
    }

    # Intel Homebrew
    exists(/usr/local/opt/hidapi) {
        INCLUDEPATH += /usr/local/opt/hidapi/include/hidapi
        LIBS += -L/usr/local/opt/hidapi/lib -lhidapi
    }

    # Native macOS application icon.
    ICON = Cap-Util.icns
}


# ============================================================
# Windows
# ============================================================

win32 {
    TARGET = Cap-Util

    # Build HIDAPI's native Windows backend directly with MinGW.
    INCLUDEPATH += C:/hidapi-src/hidapi
    SOURCES += C:/hidapi-src/windows/hid.c

    # Windows libraries required by HIDAPI.
    LIBS += -lsetupapi -lcfgmgr32

    # Embed Cap-Util.ico directly into Cap-Util.exe.
    RC_ICONS = Cap-Util.ico
}


# ============================================================
# Installation
# ============================================================

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin

!isEmpty(target.path): INSTALLS += target