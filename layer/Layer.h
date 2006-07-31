
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

#ifndef _LAYER_H_
#define _LAYER_H_

#include "PropertyContainer.h"
#include "XmlExportable.h"
#include "Selection.h"

#include <QObject>
#include <QRect>
#include <QXmlAttributes>

#include <map>

class ZoomConstraint;
class Model;
class QPainter;
class View;
class QMouseEvent;
class Clipboard;

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

    virtual const ZoomConstraint *getZoomConstraint() const { return 0; }
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
     * accordingly.  Return false if no suitable feature was
     * available.  Also return the resolution of the model in this
     * layer in sample frames.
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

    virtual void editOpen(View *, QMouseEvent *) { } // on double-click

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
     * isReady(int *) call.  The widget may choose to show a progress
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
     */
    virtual void setLayerDormant(const View *v, bool dormant) {
	m_dormancy[v] = dormant;
    }

    /**
     * Return whether the layer is dormant (i.e. hidden) in the given
     * view.
     */
    virtual bool isLayerDormant(const View *v) const {
	if (m_dormancy.find(v) == m_dormancy.end()) return false;
	return m_dormancy.find(v)->second;
    }

    virtual PlayParameters *getPlayParameters();

    virtual bool needsTextLabelHeight() const { return false; }

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

public slots:
    void showLayer(View *, bool show);

signals:
    void modelChanged();
    void modelCompletionChanged();
    void modelChanged(size_t startFrame, size_t endFrame);
    void modelReplaced();

    void layerParametersChanged();
    void layerNameChanged();

protected:
    mutable std::map<const void *, bool> m_dormancy;
};

#endif

