/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _COLOUR_3D_PLOT_H_
#define _COLOUR_3D_PLOT_H_

#include "base/Layer.h"

#include "model/DenseThreeDimensionalModel.h"

class View;
class QPainter;
class QImage;

/**
 * This is a view that displays dense 3-D data (time, some sort of
 * binned y-axis range, value) as a colour plot with value mapped to
 * colour range.  Its source is a DenseThreeDimensionalModel.
 *
 * This was the original implementation for the spectrogram view, but
 * it was replaced with a more efficient implementation that derived
 * the spectrogram itself from a DenseTimeValueModel instead of using
 * a three-dimensional model.  This class is retained in case it
 * becomes useful, but it will probably need some cleaning up if it's
 * ever actually used.
 */

class Colour3DPlotLayer : public Layer
{
    Q_OBJECT

public:
    Colour3DPlotLayer();
    ~Colour3DPlotLayer();

    virtual const ZoomConstraint *getZoomConstraint() const { return m_model; }
    virtual const Model *getModel() const { return m_model; }
    virtual void paint(View *v, QPainter &paint, QRect rect) const;

    virtual int getVerticalScaleWidth(View *v, QPainter &) const;
    virtual void paintVerticalScale(View *v, QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(View *v, QPoint &) const;

    virtual bool snapToFeatureFrame(View *v, int &frame, 
				    size_t &resolution,
				    SnapType snap) const;

    virtual bool isLayerScrollable(const View *v) const;

    void setModel(const DenseThreeDimensionalModel *model);

    virtual int getCompletion() const { return m_model->getCompletion(); }


/*
    virtual PropertyList getProperties() const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
					   int *min, int *max) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);
*/

    void setProperties(const QXmlAttributes &attributes) { }
    
protected slots:
    void cacheInvalid();
    void cacheInvalid(size_t startFrame, size_t endFrame);

protected:
    const DenseThreeDimensionalModel *m_model; // I do not own this
    
    mutable QImage *m_cache;
};

#endif
