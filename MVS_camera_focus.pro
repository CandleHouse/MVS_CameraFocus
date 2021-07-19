QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Opencv库加载路径，需修改为本机的配置环境
INCLUDEPATH += D:\opencv\build\include\opencv \
               D:\opencv\build\include\opencv2 \
               D:\opencv\build\include

LIBS += D:\opencv\mingw64-build\lib\libopencv*.a

# MvCameraControl.lib静态库的路径，无需修改
unix|win32: LIBS += -L$$PWD/Includes/ -lMvCameraControl

# 头文件路径，无需修改
INCLUDEPATH += $$PWD/Includes
DEPENDPATH += $$PWD/Includes
