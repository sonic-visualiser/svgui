
TEMPLATE = lib

include(config.pri)

CONFIG += staticlib qt thread warn_on stl rtti exceptions
QT += network xml gui widgets

TARGET = svgui

DEPENDPATH += . ../svcore
INCLUDEPATH += . ../svcore
OBJECTS_DIR = o
MOC_DIR = o

win32-g++ {
    INCLUDEPATH += ../sv-dependency-builds/win32-mingw/include
}
win32-msvc* {
    INCLUDEPATH += ../sv-dependency-builds/win32-msvc/include
}

HEADERS += layer/Colour3DPlotLayer.h \
	   layer/ColourDatabase.h \
	   layer/ColourMapper.h \
           layer/ImageLayer.h \
           layer/ImageRegionFinder.h \
           layer/Layer.h \
           layer/LayerFactory.h \
           layer/NoteLayer.h \
           layer/PaintAssistant.h \
           layer/RegionLayer.h \
           layer/SingleColourLayer.h \
           layer/SliceableLayer.h \
           layer/SliceLayer.h \
           layer/SpectrogramLayer.h \
           layer/SpectrumLayer.h \
           layer/TextLayer.h \
           layer/TimeInstantLayer.h \
           layer/TimeRulerLayer.h \
           layer/TimeValueLayer.h \
           layer/WaveformLayer.h
SOURCES += layer/Colour3DPlotLayer.cpp \
	   layer/ColourDatabase.cpp \
	   layer/ColourMapper.cpp \
           layer/ImageLayer.cpp \
           layer/ImageRegionFinder.cpp \
           layer/Layer.cpp \
           layer/LayerFactory.cpp \
           layer/NoteLayer.cpp \
           layer/PaintAssistant.cpp \
           layer/RegionLayer.cpp \
           layer/SingleColourLayer.cpp \
           layer/SliceLayer.cpp \
           layer/SpectrogramLayer.cpp \
           layer/SpectrumLayer.cpp \
           layer/TextLayer.cpp \
           layer/TimeInstantLayer.cpp \
           layer/TimeRulerLayer.cpp \
           layer/TimeValueLayer.cpp \
           layer/WaveformLayer.cpp

HEADERS += view/Overview.h \
           view/Pane.h \
           view/PaneStack.h \
           view/View.h \
           view/ViewManager.h
SOURCES += view/Overview.cpp \
           view/Pane.cpp \
           view/PaneStack.cpp \
           view/View.cpp \
           view/ViewManager.cpp

HEADERS += widgets/ActivityLog.h \
           widgets/AudioDial.h \
           widgets/ClickableLabel.h \
           widgets/ColourNameDialog.h \
           widgets/CommandHistory.h \
           widgets/CSVFormatDialog.h \
           widgets/Fader.h \
           widgets/InteractiveFileFinder.h \
           widgets/IconLoader.h \
           widgets/ImageDialog.h \
           widgets/ItemEditDialog.h \
           widgets/KeyReference.h \
           widgets/LabelCounterInputDialog.h \
           widgets/LayerTree.h \
           widgets/LayerTreeDialog.h \
           widgets/LEDButton.h \
           widgets/ListInputDialog.h \
           widgets/MIDIFileImportDialog.h \
           widgets/ModelDataTableDialog.h \
           widgets/NotifyingCheckBox.h \
           widgets/NotifyingComboBox.h \
           widgets/NotifyingPushButton.h \
           widgets/NotifyingTabBar.h \
           widgets/Panner.h \
           widgets/PluginParameterBox.h \
           widgets/PluginParameterDialog.h \
           widgets/ProgressDialog.h \
           widgets/PropertyBox.h \
           widgets/PropertyStack.h \
           widgets/RangeInputDialog.h \
           widgets/SelectableLabel.h \
           widgets/SubdividingMenu.h \
           widgets/TextAbbrev.h \
           widgets/Thumbwheel.h \
           widgets/TipDialog.h \
           widgets/TransformFinder.h \
           widgets/WindowShapePreview.h \
           widgets/WindowTypeSelector.h
SOURCES += widgets/ActivityLog.cpp \
           widgets/AudioDial.cpp \
           widgets/ColourNameDialog.cpp \
           widgets/CommandHistory.cpp \
           widgets/CSVFormatDialog.cpp \
           widgets/Fader.cpp \
           widgets/InteractiveFileFinder.cpp \
           widgets/IconLoader.cpp \
           widgets/ImageDialog.cpp \
           widgets/ItemEditDialog.cpp \
           widgets/KeyReference.cpp \
           widgets/LabelCounterInputDialog.cpp \
           widgets/LayerTree.cpp \
           widgets/LayerTreeDialog.cpp \
           widgets/LEDButton.cpp \
           widgets/ListInputDialog.cpp \
           widgets/MIDIFileImportDialog.cpp \
           widgets/ModelDataTableDialog.cpp \
           widgets/NotifyingCheckBox.cpp \
           widgets/NotifyingComboBox.cpp \
           widgets/NotifyingPushButton.cpp \
           widgets/NotifyingTabBar.cpp \
           widgets/Panner.cpp \
           widgets/PluginParameterBox.cpp \
           widgets/PluginParameterDialog.cpp \
           widgets/ProgressDialog.cpp \
           widgets/PropertyBox.cpp \
           widgets/PropertyStack.cpp \
           widgets/RangeInputDialog.cpp \
           widgets/SelectableLabel.cpp \
           widgets/SubdividingMenu.cpp \
           widgets/TextAbbrev.cpp \
           widgets/Thumbwheel.cpp \
           widgets/TipDialog.cpp \
           widgets/TransformFinder.cpp \
           widgets/WindowShapePreview.cpp \
           widgets/WindowTypeSelector.cpp
