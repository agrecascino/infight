TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp

LIBS += -lstdc++fs -lcpptk -lpthread -lboost_system -lcrypto  -lssl -lcurlpp -lcurl -ljsoncpp
QMAKE_CXXFLAGS += -pthread -std=c++17
