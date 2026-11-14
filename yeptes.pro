QT += core gui qml quick quickcontrols2 dbus network

CONFIG += c++17 release
TARGET = yeptes
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/diff.h \
    src/filepicker.h \
    src/highlighter.h \
    src/jsonmodel.h \
    src/tablemodel.h \
    src/offline.h \
    src/portalfilepicker.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/diff.cpp \
    src/highlighter.cpp \
    src/jsonmodel.cpp \
    src/tablemodel.cpp \
    src/portalfilepicker.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
