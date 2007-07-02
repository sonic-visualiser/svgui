TEMPLATE = lib

SV_UNIT_PACKAGES = fftw3f
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
           ColourMapper.h \
           Layer.h \
           LayerFactory.h \
           NoteLayer.h \
           PaintAssistant.h \
           SliceableLayer.h \
           SliceLayer.h \
           SpectrogramLayer.h \
           SpectrumLayer.h \
           TextLayer.h \
           TimeInstantLayer.h \
           TimeRulerLayer.h \
           TimeValueLayer.h \
           WaveformLayer.h
SOURCES += Colour3DPlotLayer.cpp \
           ColourMapper.cpp \
           Layer.cpp \
           LayerFactory.cpp \
           NoteLayer.cpp \
           PaintAssistant.cpp \
           SliceLayer.cpp \
           SpectrogramLayer.cpp \
           SpectrumLayer.cpp \
           TextLayer.cpp \
           TimeInstantLayer.cpp \
           TimeRulerLayer.cpp \
           TimeValueLayer.cpp \
           WaveformLayer.cpp
