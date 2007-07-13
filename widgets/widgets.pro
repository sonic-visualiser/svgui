TEMPLATE = lib

SV_UNIT_PACKAGES = vamp-hostsdk fftw3f
load(../sv.prf)

CONFIG += sv staticlib qt thread warn_on stl rtti exceptions
QT += xml

TARGET = svwidgets

DEPENDPATH += . ..
INCLUDEPATH += . ..
OBJECTS_DIR = tmp_obj
MOC_DIR = tmp_moc

# Input
HEADERS += AudioDial.h \
           ColourNameDialog.h \
           Fader.h \
           IconLoader.h \
           ItemEditDialog.h \
           KeyReference.h \
           LayerTree.h \
           LEDButton.h \
           ListInputDialog.h \
           NotifyingCheckBox.h \
           NotifyingComboBox.h \
           NotifyingPushButton.h \
           NotifyingTabBar.h \
           Panner.h \
           PluginParameterBox.h \
           PluginParameterDialog.h \
           PropertyBox.h \
           PropertyStack.h \
           RangeInputDialog.h \
           SubdividingMenu.h \
           Thumbwheel.h \
           TipDialog.h \
           WindowShapePreview.h \
           WindowTypeSelector.h
SOURCES += AudioDial.cpp \
           ColourNameDialog.cpp \
           Fader.cpp \
           IconLoader.cpp \
           ItemEditDialog.cpp \
           KeyReference.cpp \
           LayerTree.cpp \
           LEDButton.cpp \
           ListInputDialog.cpp \
           NotifyingCheckBox.cpp \
           NotifyingComboBox.cpp \
           NotifyingPushButton.cpp \
           NotifyingTabBar.cpp \
           Panner.cpp \
           PluginParameterBox.cpp \
           PluginParameterDialog.cpp \
           PropertyBox.cpp \
           PropertyStack.cpp \
           RangeInputDialog.cpp \
           SubdividingMenu.cpp \
           Thumbwheel.cpp \
           TipDialog.cpp \
           WindowShapePreview.cpp \
           WindowTypeSelector.cpp
