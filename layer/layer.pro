TEMPLATE = lib

SV_UNIT_PACKAGES = 
load(../sv.prf)

CONFIG += sv staticlib qt thread warn_on stl rtti exceptions
QT += xml

TARGET = svlayer

DEPENDPATH += . ..
INCLUDEPATH += . ..
OBJECTS_DIR = tmp_obj
MOC_DIR = tmp_moc

# Input
HEADERS += Colour3DPlotLayer.h \
           Layer.h \
           LayerFactory.h \
           NoteLayer.h \
           SpectrogramLayer.h \
           SpectrumLayer.h \
           TextLayer.h \
           TimeInstantLayer.h \
           TimeRulerLayer.h \
           TimeValueLayer.h \
           WaveformLayer.h
SOURCES += Colour3DPlotLayer.cpp \
           Layer.cpp \
           LayerFactory.cpp \
           NoteLayer.cpp \
           SpectrogramLayer.cpp \
           SpectrumLayer.cpp \
           TextLayer.cpp \
           TimeInstantLayer.cpp \
           TimeRulerLayer.cpp \
           TimeValueLayer.cpp \
           WaveformLayer.cpp
