
/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _LAYER_H_
#define _LAYER_H_

#include "base/PropertyContainer.h"
#include "base/XmlExportable.h"
#include "base/Selection.h"

#include <QObject>
#include <QRect>
#include <QXmlAttributes>
#include <QMutex>

#include <map>

class ZoomConstraint;
class Model;
class QPainter;
class View;
class QMouseEvent;
class Clipboard;
class RangeMapper;

/**
 * The base class for visual representations of the data found in a
 * Model.  Layers are expected to be able to draw themselves onto a
 * View, and may also be editable.
 */

class Layer : public PropertyContainer,
	      public XmlExportable
{
    Q_OBJECT

public:
    Layer();
    virtual ~Layer();

    virtual const Model *getModel() const = 0;
    virtual Model *getModel() {
	return const_cast<Model *>(const_cast<const Layer *>(this)->getModel());
    }

    /**
     * Return a zoom constraint object defining the supported zoom
     * levels for this layer.  If this returns zero, the layer will
     * support any integer zoom level.
     */
    virtual const ZoomConstraint *getZoomConstraint() const { return 0; }

    /**
     * Return true if this layer can handle zoom levels other than
     * those supported by its zoom constraint (presumably less
     * efficiently or accurately than the officially supported zoom
     * levels).  If true, the layer will unenthusistically accept any
     * integer zoom level from 1 to the maximum returned by its zoom
     * constraint.
     */
    virtual bool supportsOtherZoomLevels() const { return true; }

    virtual void paint(View *, QPainter &, QRect) const = 0;   

    enum VerticalPosition {
	PositionTop, PositionMiddle, PositionBottom
    };
    virtual VerticalPosition getPreferredTimeRulerPosition() const {
	return PositionMiddle;
    }
    virtual VerticalPosition getPreferredFrameCountPosition() const {
	return PositionBottom;
    }
    virtual bool hasLightBackground() const {
        return true;
    }

    virtual QString getPropertyContainerIconName() const;

    virtual QString getPropertyContainerName() const {
	return objectName();
    }

    virtual QString getLayerPresentationName() const;

    virtual int getVerticalScaleWidth(View *, QPainter &) const { return 0; }
    virtual void paintVerticalScale(View *, QPainter &, QRect) const { }

    virtual bool getCrosshairExtents(View *, QPainter &, QPoint /* cursorPos */,
                                     std::vector<QRect> &) const {
        return false;
    }
    virtual void paintCrosshairs(View *, QPainter &, QPoint) const { }

    virtual void paintMeasurementRects(View *, QPainter &) const;

    virtual QString getFeatureDescription(View *, QPoint &) const {
	return "";
    }

    enum SnapType {
	SnapLeft,
	SnapRight,
	SnapNearest,
	SnapNeighbouring
    };

    /**
     * Adjust the given frame to snap to the nearest feature, if
     * possible.
     *
     * If snap is SnapLeft or SnapRight, adjust the frame to match
     * that of the nearest feature in the given direction regardless
     * of how far away it is.  If snap is SnapNearest, adjust the
     * frame to that of the nearest feature in either direction.  If
     * snap is SnapNeighbouring, adjust the frame to that of the
     * nearest feature if it is close, and leave it alone (returning
     * false) otherwise.  SnapNeighbouring should always choose the
     * same feature that would be used in an editing operation through
     * calls to editStart etc.
     *
     * Return true if a suitable feature was found and frame adjusted
     * accordingly.  Return false if no suitable feature was available
     * (and leave frame unmodified).  Also return the resolution of
     * the model in this layer in sample frames.
     */
    virtual bool snapToFeatureFrame(View *   /* v */,
				    int &    /* frame */,
				    size_t &resolution,
				    SnapType /* snap */) const {
	resolution = 1;
	return false;
    }

    // Draw and edit modes:
    //
    // Layer needs to get actual mouse events, I guess.  Draw mode is
    // probably the easier.

    virtual void drawStart(View *, QMouseEvent *) { }
    virtual void drawDrag(View *, QMouseEvent *) { }
    virtual void drawEnd(View *, QMouseEvent *) { }

    virtual void editStart(View *, QMouseEvent *) { }
    virtual void editDrag(View *, QMouseEvent *) { }
    virtual void editEnd(View *, QMouseEvent *) { }

    // Measurement rectangle (or equivalent).  Unlike draw and edit,
    // the base Layer class can provide working implementations of
    // these for most situations.
    //
    virtual void measureStart(View *, QMouseEvent *);
    virtual void measureDrag(View *, QMouseEvent *);
    virtual void measureEnd(View *, QMouseEvent *);

    /**
     * Open an editor on the item under the mouse (e.g. on
     * double-click).  If there is no item or editing is not
     * supported, return false.
     */
    virtual bool editOpen(View *, QMouseEvent *) { return false; }

    virtual void moveSelection(Selection, size_t /* newStartFrame */) { }
    virtual void resizeSelection(Selection, Selection /* newSize */) { }
    virtual void deleteSelection(Selection) { }

    virtual void copy(Selection, Clipboard & /* to */) { }

    /**
     * Paste from the given clipboard onto the layer at the given
     * frame offset.  If interactive is true, the layer may ask the
     * user about paste options through a dialog if desired, and may
     * return false if the user cancelled the paste operation.  This
     * function should return true if a paste actually occurred.
     */
    virtual bool paste(const Clipboard & /* from */,
                       int /* frameOffset */,
                       bool /* interactive */) { return false; }

    // Text mode:
    //
    // Label nearest feature.  We need to get the feature coordinates
    // and current label from the layer, and then the pane can pop up
    // a little text entry dialog at the right location.  Or we edit
    // in place?  Probably the dialog is easier.

    /**
     * This should return true if the layer can safely be scrolled
     * automatically by a given view (simply copying the existing data
     * and then refreshing the exposed area) without altering its
     * meaning.  For the view widget as a whole this is usually not
     * possible because of invariant (non-scrolling) material
     * displayed over the top, but the widget may be able to optimise
     * scrolling better if it is known that individual views can be
     * scrolled safely in this way.
     */
    virtual bool isLayerScrollable(const View *) const { return true; }

    /**
     * This should return true if the layer completely obscures any
     * underlying layers.  It's used to determine whether the view can
     * safely draw any selection rectangles under the layer instead of
     * over it, in the case where the layer is not scrollable and
     * therefore needs to be redrawn each time (so that the selection
     * rectangle can be cached).
     */
    virtual bool isLayerOpaque() const { return false; }

    /**
     * This should return true if the layer uses colours to indicate
     * meaningful information (as opposed to just using a single
     * colour of the user's choice).  If this is the case, the view
     * will show selections using unfilled rectangles instead of
     * translucent filled rectangles, so as not to disturb the colours
     * underneath.
     */
    virtual bool isLayerColourSignificant() const { return false; }

    /**
     * This should return true if the layer can be edited by the user.
     * If this is the case, the appropriate edit tools may be made
     * available by the application and the layer's drawStart/Drag/End
     * and editStart/Drag/End methods should be implemented.
     */
    virtual bool isLayerEditable() const { return false; }

    /**
     * Return the proportion of background work complete in drawing
     * this view, as a percentage -- in most cases this will be the
     * value returned by pointer from a call to the underlying model's
     * isReady(int *) call.  The view may choose to show a progress
     * meter if it finds that this returns < 100 at any given moment.
     */
    virtual int getCompletion(View *) const { return 100; }

    virtual void setObjectName(const QString &name);

    /**
     * Convert the layer's data (though not those of the model it
     * refers to) into an XML string for file output.  This class
     * implements the basic name/type/model-id output; subclasses will
     * typically call this superclass implementation with extra
     * attributes describing their particular properties.
     */
    virtual QString toXmlString(QString indent = "",
				QString extraAttributes = "") const;

    /**
     * Set the particular properties of a layer (those specific to the
     * subclass) from a set of XML attributes.  This is the effective
     * inverse of the toXmlString method.
     */
    virtual void setProperties(const QXmlAttributes &) = 0;

    /**
     * Indicate that a layer is not currently visible in the given
     * view and is not expected to become visible in the near future
     * (for example because the user has explicitly removed or hidden
     * it).  The layer may respond by (for example) freeing any cache
     * memory it is using, until next time its paint method is called,
     * when it should set itself un-dormant again.
     *
     * A layer class that overrides this function must also call this
     * class's implementation.
     */
    virtual void setLayerDormant(const View *v, bool dormant);

    /**
     * Return whether the layer is dormant (i.e. hidden) in the given
     * view.
     */
    virtual bool isLayerDormant(const View *v) const;

    virtual PlayParameters *getPlayParameters();

    virtual bool needsTextLabelHeight() const { return false; }

    virtual bool hasTimeXAxis() const { return true; }

    /**
     * Return the minimum and maximum values for the y axis of the
     * model in this layer, as well as whether the layer is configured
     * to use a logarithmic y axis display.  Also return the unit for
     * these values if known.
     *
     * This function returns the "normal" extents for the layer, not
     * necessarily the extents actually in use in the display.
     */
    virtual bool getValueExtents(float &min, float &max,
                                 bool &logarithmic, QString &unit) const = 0;

    /**
     * Return the minimum and maximum values within the displayed
     * range for the y axis, if only a subset of the whole range of
     * the model (returned by getValueExtents) is being displayed.
     * Return false if the layer is not imposing a particular display
     * extent (using the normal layer extents or deferring to whatever
     * is in use for the same units elsewhere in the view).
     */
    virtual bool getDisplayExtents(float & /* min */,
                                   float & /* max */) const {
        return false;
    }

    /**
     * Set the displayed minimum and maximum values for the y axis to
     * the given range, if supported.  Return false if not supported
     * on this layer (and set nothing).  In most cases, layers that
     * return false for getDisplayExtents should also return false for
     * this function.
     */
    virtual bool setDisplayExtents(float /* min */,
                                   float /* max */) {
        return false;
    }

    /**
     * Return the value and unit at the given x coordinate in the
     * given view.  This is for descriptive purposes using the
     * measurement tool.  The default implementation works correctly
     * if the layer hasTimeXAxis().
     */
    virtual bool getXScaleValue(const View *v, int x,
                                float &value, QString &unit) const;

    /** 
     * Return the value and unit at the given y coordinate in the
     * given view.
     */
    virtual bool getYScaleValue(const View *, int /* y */,
                                float &/* value */, QString &/* unit */) const {
        return false;
    }

    /**
     * Get the number of vertical zoom steps available for this layer.
     * If vertical zooming is not available, return 0.  The meaning of
     * "zooming" is entirely up to the layer -- changing the zoom
     * level may cause the layer to reset its display extents or
     * change another property such as display gain.  However, layers
     * are advised for consistency to treat smaller zoom steps as
     * "more distant" or "zoomed out" and larger ones as "closer" or
     * "zoomed in".
     * 
     * Layers that provide this facility should also emit the
     * verticalZoomChanged signal if their vertical zoom changes
     * due to factors other than setVerticalZoomStep being called.
     */
    virtual int getVerticalZoomSteps(int & /* defaultStep */) const { return 0; }

    /**
     * Get the current vertical zoom step.  A layer may support finer
     * control over ranges etc than is available through the integer
     * zoom step mechanism; if this one does, it should just return
     * the nearest of the available zoom steps to the current settings.
     */
    virtual int getCurrentVerticalZoomStep() const { return 0; }

    /**
     * Set the vertical zoom step.  The meaning of "zooming" is
     * entirely up to the layer -- changing the zoom level may cause
     * the layer to reset its display extents or change another
     * property such as display gain.
     */
    virtual void setVerticalZoomStep(int) { }

    /**
     * Create and return a range mapper for vertical zoom step values.
     * See the RangeMapper documentation for more details.  The
     * returned value is allocated on the heap and will be deleted by
     * the caller.
     */
    virtual RangeMapper *getNewVerticalZoomRangeMapper() const { return 0; }

public slots:
    void showLayer(View *, bool show);

signals:
    void modelChanged();
    void modelCompletionChanged();
    void modelChanged(size_t startFrame, size_t endFrame);
    void modelReplaced();

    void layerParametersChanged();
    void layerParameterRangesChanged();
    void layerNameChanged();

    void verticalZoomChanged();

protected:
    struct MeasureRect {
        mutable QRect pixrect;
        long startFrame; // only valid for a layer that hasTimeXAxis
        long endFrame;   // ditto
    };

    typedef std::vector<MeasureRect> MeasureRectList; // should be x-ordered
    MeasureRectList m_measureRectList;
    MeasureRect m_draggingRect;
    bool m_haveDraggingRect;

private:
    mutable QMutex m_dormancyMutex;
    mutable std::map<const void *, bool> m_dormancy;
};

#endif

