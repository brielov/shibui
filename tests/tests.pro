QT += core concurrent gui qml quick widgets testlib

CONFIG += testcase c++17
CONFIG -= app_bundle

TEMPLATE = app
TARGET = shibui-tests

SOURCES += \
    test_core.cpp \
    ../src/filesystemmodel.cpp \
    ../src/filetransfer.cpp \
    ../src/filetrash.cpp \
    ../src/filerestore.cpp \
    ../src/placesmodel.cpp \
    ../src/searchmodel.cpp \
    ../src/previewmodel.cpp \
    ../src/openwithmodel.cpp \
    ../src/propertiesmodel.cpp \
    ../src/networkmodel.cpp \
    ../src/archivemodel.cpp \
    ../src/recentmodel.cpp \
    ../src/bulkrenamemodel.cpp \
    ../src/templatemodel.cpp \
    ../src/fileiconprovider.cpp \
    ../src/thememanager.cpp

HEADERS += \
    ../src/filesystemmodel.h \
    ../src/filetransfer.h \
    ../src/filetrash.h \
    ../src/filerestore.h \
    ../src/placesmodel.h \
    ../src/searchmodel.h \
    ../src/previewmodel.h \
    ../src/openwithmodel.h \
    ../src/propertiesmodel.h \
    ../src/networkmodel.h \
    ../src/archivemodel.h \
    ../src/recentmodel.h \
    ../src/bulkrenamemodel.h \
    ../src/templatemodel.h \
    ../src/fileiconprovider.h \
    ../src/thememanager.h

RESOURCES += ../resources.qrc

INCLUDEPATH += ../src

QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
