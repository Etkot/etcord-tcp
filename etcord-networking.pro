TEMPLATE = lib
TARGET = StaticLib

CONFIG += staticlib
CONFIG += console c++11

QMAKE_CXXFLAGS += -std=c++0x -pthread
LIBS += -pthread

SOURCES += \
    tcp/common.cpp \
    tcp/server.cpp \
    tcp/client.cpp

HEADERS += \
    tcp/common.h \
    tcp/server.h \
    tcp/client.h \
    tcp/safequeue.h \
    tcp/packet.h \
    tcp/sockets.h
