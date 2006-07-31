TEMPLATE = lib

SV_UNIT_PACKAGES =
load(../sv.prf)

CONFIG += sv staticlib qt thread warn_on stl rtti exceptions
QT += xml

TARGET = svview

DEPENDPATH += . ..
INCLUDEPATH += . ..
OBJECTS_DIR = tmp_obj
MOC_DIR = tmp_moc

# Input
HEADERS += Pane.h PaneStack.h Panner.h View.h ViewManager.h
SOURCES += Pane.cpp PaneStack.cpp Panner.cpp View.cpp ViewManager.cpp
