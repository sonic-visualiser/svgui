/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _FLEXINOTE_LAYER_H_
#define _FLEXINOTE_LAYER_H_

#define NOTE_HEIGHT 16

#include "SingleColourLayer.h"
#include "VerticalScaleLayer.h"

#include "data/model/FlexiNoteModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;
class SparseTimeValueModel;

class FlexiNoteLayer : public SingleColourLayer,
                       public VerticalScaleLayer
{
    Q_OBJECT

public:
    FlexiNoteLayer();

    virtual void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const;

    virtual int getVerticalScaleWidth(LayerGeometryProvider *v, bool, QPainter &) const;
    virtual void paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const;

    virtual bool snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                    int &resolution,
                    SnapType snap) const;

    virtual void drawStart(LayerGeometryProvider *v, QMouseEvent *);
    virtual void drawDrag(LayerGeometryProvider *v, QMouseEvent *);
    virtual void drawEnd(LayerGeometryProvider *v, QMouseEvent *);

    virtual void eraseStart(LayerGeometryProvider *v, QMouseEvent *);
    virtual void eraseDrag(LayerGeometryProvider *v, QMouseEvent *);
    virtual void eraseEnd(LayerGeometryProvider *v, QMouseEvent *);

    virtual void editStart(LayerGeometryProvider *v, QMouseEvent *);
    virtual void editDrag(LayerGeometryProvider *v, QMouseEvent *);
    virtual void editEnd(LayerGeometryProvider *v, QMouseEvent *);

    virtual void splitStart(LayerGeometryProvider *v, QMouseEvent *);
    virtual void splitEnd(LayerGeometryProvider *v, QMouseEvent *);
    
    virtual void addNote(LayerGeometryProvider *v, QMouseEvent *e);

    virtual void mouseMoveEvent(LayerGeometryProvider *v, QMouseEvent *);

    virtual bool editOpen(LayerGeometryProvider *v, QMouseEvent *);

    virtual void moveSelection(Selection s, sv_frame_t newStartFrame);
    virtual void resizeSelection(Selection s, Selection newSize);
    virtual void deleteSelection(Selection s);
    virtual void deleteSelectionInclusive(Selection s);

    virtual void copy(LayerGeometryProvider *v, Selection s, Clipboard &to);
    virtual bool paste(LayerGeometryProvider *v, const Clipboard &from, sv_frame_t frameOffset,
                       bool interactive);

    void splitNotesAt(LayerGeometryProvider *v, sv_frame_t frame);
    void snapSelectedNotesToPitchTrack(LayerGeometryProvider *v, Selection s);
    void mergeNotes(LayerGeometryProvider *v, Selection s, bool inclusive);

    virtual const Model *getModel() const { return m_model; }
    void setModel(FlexiNoteModel *model);

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
                      int value) const;
    virtual void setProperty(const PropertyName &, int value);

    enum VerticalScale {
        AutoAlignScale,
        LinearScale,
        LogScale,
        MIDIRangeScale
    };
    
    //GF: Tonioni: context sensitive note edit actions (denoted clockwise from top).
    enum EditMode {
        DragNote,
        RightBoundary,
        SplitNote,
        LeftBoundary
    };
    
    void setIntelligentActions(bool on) { m_intelligentActions=on; }

    void setVerticalScale(VerticalScale scale);
    VerticalScale getVerticalScale() const { return m_verticalScale; }

    virtual bool isLayerScrollable(const LayerGeometryProvider *v) const;

    virtual bool isLayerEditable() const { return true; }

    virtual int getCompletion(LayerGeometryProvider *) const { return m_model->getCompletion(); }

    virtual bool getValueExtents(double &min, double &max,
                                 bool &log, QString &unit) const;

    virtual bool getDisplayExtents(double &min, double &max) const;
    virtual bool setDisplayExtents(double min, double max);

    virtual int getVerticalZoomSteps(int &defaultStep) const;
    virtual int getCurrentVerticalZoomStep() const;
    virtual void setVerticalZoomStep(int);
    virtual RangeMapper *getNewVerticalZoomRangeMapper() const;

    /**
     * Add a note-on.  Used when recording MIDI "live".  The note will
     * not be finally added to the layer until the corresponding
     * note-off.
     */
    void addNoteOn(sv_frame_t frame, int pitch, int velocity);
    
    /**
     * Add a note-off.  This will cause a note to appear, if and only
     * if there is a matching pending note-on.
     */
    void addNoteOff(sv_frame_t frame, int pitch);

    /**
     * Abandon all pending note-on events.
     */
    void abandonNoteOns();

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);
    
    void setVerticalRangeToNoteRange(LayerGeometryProvider *v);

    /// VerticalScaleLayer methods
    virtual int getYForValue(LayerGeometryProvider *v, double value) const;
    virtual double getValueForY(LayerGeometryProvider *v, int y) const;
    virtual QString getScaleUnits() const;

protected:
    void getScaleExtents(LayerGeometryProvider *, double &min, double &max, bool &log) const;
    bool shouldConvertMIDIToHz() const;

    virtual int getDefaultColourHint(bool dark, bool &impose);

    FlexiNoteModel::PointList getLocalPoints(LayerGeometryProvider *v, int) const;

    bool getPointToDrag(LayerGeometryProvider *v, int x, int y, FlexiNoteModel::Point &) const;
    bool getNoteToEdit(LayerGeometryProvider *v, int x, int y, FlexiNoteModel::Point &) const;
    void getRelativeMousePosition(LayerGeometryProvider *v, FlexiNoteModel::Point &note, int x, int y, bool &closeToLeft, bool &closeToRight, bool &closeToTop, bool &closeToBottom) const;
    SparseTimeValueModel *getAssociatedPitchModel(LayerGeometryProvider *v) const;
    bool updateNoteValue(LayerGeometryProvider *v, FlexiNoteModel::Point &note) const;
    void splitNotesAt(LayerGeometryProvider *v, sv_frame_t frame, QMouseEvent *e);

    FlexiNoteModel *m_model;
    bool m_editing;
    bool m_intelligentActions;
    int m_dragPointX;
    int m_dragPointY;
    int m_dragStartX;
    int m_dragStartY;
    FlexiNoteModel::Point m_originalPoint;
    FlexiNoteModel::Point m_editingPoint;
    sv_frame_t m_greatestLeftNeighbourFrame;
    sv_frame_t m_smallestRightNeighbourFrame;
    FlexiNoteModel::EditCommand *m_editingCommand;
    VerticalScale m_verticalScale;
    EditMode m_editMode;

    typedef std::set<FlexiNoteModel::Point, FlexiNoteModel::Point::Comparator> FlexiNoteSet;
    FlexiNoteSet m_pendingNoteOns;

    mutable double m_scaleMinimum;
    mutable double m_scaleMaximum;

    bool shouldAutoAlign() const;

    void finish(FlexiNoteModel::EditCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

