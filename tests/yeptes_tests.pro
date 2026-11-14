QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase
TARGET = yeptes_tests
TEMPLATE = app

INCLUDEPATH += ../src
# The tests read the syntax rules from the source tree rather than a resource.
DEFINES += SYNTAX_DIR=\\\"$$PWD/../src/syntax\\\"

HEADERS += \
    ../src/backend.h \
    ../src/diff.h \
    ../src/tablemodel.h \
    ../src/filepicker.h \
    ../src/highlighter.h \
    ../src/jsonmodel.h \
    ../src/theme.h \
    ../src/translator.h

SOURCES += \
    yeptes_tests.cpp \
    ../src/backend.cpp \
    ../src/diff.cpp \
    ../src/tablemodel.cpp \
    ../src/highlighter.cpp \
    ../src/jsonmodel.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp
