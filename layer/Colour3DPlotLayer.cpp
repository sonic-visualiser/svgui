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

#include "Colour3DPlotLayer.h"

#include "view/View.h"
#include "base/Profiler.h"

#include <QPainter>
#include <QImage>
#include <QRect>

#include <iostream>

#include <cassert>

//#define DEBUG_COLOUR_3D_PLOT_LAYER_PAINT 1


Colour3DPlotLayer::Colour3DPlotLayer() :
    Layer(),
    m_model(0),
    m_cache(0),
    m_colourScale(LinearScale)
{
    
}

Colour3DPlotLayer::~Colour3DPlotLayer()
{
}

void
Colour3DPlotLayer::setModel(const DenseThreeDimensionalModel *model)
{
    m_model = model;
    if (!m_model || !m_model->isOK()) return;

    connect(m_model, SIGNAL(modelChanged()), this, SIGNAL(modelChanged()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SIGNAL(modelChanged(size_t, size_t)));

    connect(m_model, SIGNAL(completionChanged()),
	    this, SIGNAL(modelCompletionChanged()));

    connect(m_model, SIGNAL(modelChanged()), this, SLOT(cacheInvalid()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SLOT(cacheInvalid(size_t, size_t)));

    emit modelReplaced();
}

void
Colour3DPlotLayer::cacheInvalid()
{
    delete m_cache; 
    m_cache = 0;
}

void
Colour3DPlotLayer::cacheInvalid(size_t, size_t)
{
    cacheInvalid();
}

Layer::PropertyList
Colour3DPlotLayer::getProperties() const
{
    PropertyList list;
    list.push_back("Colour Scale");
    return list;
}

QString
Colour3DPlotLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour Scale") return tr("Colour Scale");
    return "";
}

Layer::PropertyType
Colour3DPlotLayer::getPropertyType(const PropertyName &name) const
{
    return ValueProperty;
}

QString
Colour3DPlotLayer::getPropertyGroupName(const PropertyName &name) const
{
    return QString();
}

int
Colour3DPlotLayer::getPropertyRangeAndValue(const PropertyName &name,
                                        int *min, int *max) const
{
    int deft = 0;

    int garbage0, garbage1;
    if (!min) min = &garbage0;
    if (!max) max = &garbage1;

    if (name == "Colour Scale") {

	*min = 0;
	*max = 3;

	deft = (int)m_colourScale;

    } else {
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
Colour3DPlotLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == "Colour Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Absolute");
	case 2: return tr("Meter");
	case 3: return tr("dB");
	}
    }
    return tr("<unknown>");
}

void
Colour3DPlotLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Colour Scale") {
	switch (value) {
	default:
	case 0: setColourScale(LinearScale); break;
	case 1: setColourScale(AbsoluteScale); break;
	case 2: setColourScale(MeterScale); break;
	case 3: setColourScale(dBScale); break;
	}
    }
}

void
Colour3DPlotLayer::setColourScale(ColourScale scale)
{
    if (m_colourScale == scale) return;
    m_colourScale = scale;
    cacheInvalid();
    emit layerParametersChanged();
}

bool
Colour3DPlotLayer::isLayerScrollable(const View *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

QString
Colour3DPlotLayer::getFeatureDescription(View *v, QPoint &pos) const
{
    if (!m_model) return "";

    int x = pos.x();
    int y = pos.y();

    size_t modelStart = m_model->getStartFrame();
    size_t modelResolution = m_model->getResolution();

    float srRatio =
        float(v->getViewManager()->getMainModelSampleRate()) /
        float(m_model->getSampleRate());

    int sx0 = int((v->getFrameForX(x) / srRatio - long(modelStart)) /
                  long(modelResolution));

    int f0 = sx0 * modelResolution;
    int f1 =  f0 + modelResolution;

    float binHeight = float(v->height()) / m_model->getHeight();
    int sy = (v->height() - y) / binHeight;

    float value = m_model->getValueAt(sx0, sy);

//    std::cerr << "bin value (" << sx0 << "," << sy << ") is " << value << std::endl;
    
    QString binName = m_model->getBinName(sy);
    if (binName == "") binName = QString("[%1]").arg(sy + 1);
    else binName = QString("%1 [%2]").arg(binName).arg(sy + 1);

    QString text = tr("Time:\t%1 - %2\nBin:\t%3\nValue:\t%4")
	.arg(RealTime::frame2RealTime(f0, m_model->getSampleRate())
	     .toText(true).c_str())
	.arg(RealTime::frame2RealTime(f1, m_model->getSampleRate())
	     .toText(true).c_str())
	.arg(binName)
	.arg(value);

    return text;
}

int
Colour3DPlotLayer::getColourScaleWidth(QPainter &paint) const
{
    int cw = 20;
    return cw;
}

int
Colour3DPlotLayer::getVerticalScaleWidth(View *v, QPainter &paint) const
{
    if (!m_model) return 0;

    QString sampleText = QString("[%1]").arg(m_model->getHeight());
    int tw = paint.fontMetrics().width(sampleText);
    bool another = false;

    for (size_t i = 0; i < m_model->getHeight(); ++i) {
	if (m_model->getBinName(i).length() > sampleText.length()) {
	    sampleText = m_model->getBinName(i);
            another = true;
	}
    }
    if (another) {
	tw = std::max(tw, paint.fontMetrics().width(sampleText));
    }

    return tw + 13 + getColourScaleWidth(paint);
}

void
Colour3DPlotLayer::paintVerticalScale(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model) return;

    int h = rect.height(), w = rect.width();
    float binHeight = float(v->height()) / m_model->getHeight();

    int cw = getColourScaleWidth(paint);
    
    int ch = h - 20;
    if (ch > 20 && m_cache) {

        paint.setPen(Qt::black);
        paint.drawRect(4, 10, cw - 8, ch - 19);

        for (int y = 0; y < ch - 20; ++y) {
            QRgb c = m_cache->color(((ch - 20 - y) * 255) / (ch - 20));
//            std::cerr << "y = " << y << ": rgb " << qRed(c) << "," << qGreen(c) << "," << qBlue(c) << std::endl;
            paint.setPen(QColor(qRed(c), qGreen(c), qBlue(c)));
            paint.drawLine(5, 11 + y, cw - 5, 11 + y);
        }
    }

    paint.setPen(Qt::black);

    int count = v->height() / paint.fontMetrics().height();
    int step = m_model->getHeight() / count;
    if (step == 0) step = 1;

    for (size_t i = 0; i < m_model->getHeight(); ++i) {

        if ((i % step) != 0) continue;

	int y0 = v->height() - (i * binHeight) - 1;
	
	QString text = m_model->getBinName(i);
	if (text == "") text = QString("[%1]").arg(i + 1);

	paint.drawLine(cw, y0, w, y0);

	int cy = y0 - (step * binHeight)/2;
	int ty = cy + paint.fontMetrics().ascent()/2;

	paint.drawText(cw + 5, ty, text);
    }
}

void
Colour3DPlotLayer::paint(View *v, QPainter &paint, QRect rect) const
{
//    Profiler profiler("Colour3DPlotLayer::paint");
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    std::cerr << "Colour3DPlotLayer::paint(): m_model is " << m_model << ", zoom level is " << v->getZoomLevel() << std::endl;
#endif

    int completion = 0;
    if (!m_model || !m_model->isOK() || !m_model->isReady(&completion)) {
	if (completion > 0) {
	    paint.fillRect(0, 10, v->width() * completion / 100,
			   10, QColor(120, 120, 120));
	}
	return;
    }

    size_t modelStart = m_model->getStartFrame();
    size_t modelEnd = m_model->getEndFrame();
    size_t modelResolution = m_model->getResolution();

    size_t cacheWidth = (modelEnd - modelStart) / modelResolution + 1;
    size_t cacheHeight = m_model->getHeight();

    if (m_cache &&
	(m_cache->width() != cacheWidth ||
	 m_cache->height() != cacheHeight)) {

	delete m_cache;
	m_cache = 0;
    }

    if (!m_cache) { 

	m_cache = new QImage(cacheWidth, cacheHeight, QImage::Format_Indexed8);

        std::cerr << "Cache size " << cacheWidth << "x" << cacheHeight << std::endl;

	m_cache->setNumColors(256);
	DenseThreeDimensionalModel::Column values;

	float min = m_model->getMinimumLevel();
	float max = m_model->getMaximumLevel();

	if (max == min) max = min + 1.0;

        int zeroIndex = 0;
        if (min < 0.f) {
            if (m_colourScale == LinearScale) {
                zeroIndex = int(((-min) * 256) / (max - min));
            } else {
                max = std::max(-min, max);
                min = 0;
            }
        }
        if (zeroIndex < 0) zeroIndex = 0;
        if (zeroIndex > 255) zeroIndex = 255;

        //!!! want this and spectrogram to share a colour mapping unit

	for (int index = 0; index < 256; ++index) {
            int effective = abs(((index - zeroIndex) * 255) /
                                std::max(255 - zeroIndex, zeroIndex));
	    int hue = 256 - effective;
            if (zeroIndex > 0) {
                if (index <= zeroIndex) hue = 255;
                else hue = 0;
            }
            while (hue < 0) hue += 255;
            while (hue > 255) hue -= 255;
            int saturation = effective / 2 + 128;
            if (saturation < 0) saturation = -saturation;
            if (saturation > 255) saturation = 255;
            int value = effective;
            if (value < 0) value = -value;
            if (value > 255) value = 255;
//            std::cerr << "min: " << min << ", max: " << max << ", zi " << zeroIndex << ", index " << index << ": " << hue << ", " << saturation << ", " << value << std::endl;
	    QColor colour = QColor::fromHsv(hue, saturation, value);
//            std::cerr << "rgb: " << colour.red() << "," << colour.green() << "," << colour.blue() << std::endl;
	    m_cache->setColor(index, qRgb(colour.red(), colour.green(), colour.blue()));
	}

	m_cache->fill(zeroIndex);

	for (size_t f = modelStart; f <= modelEnd; f += modelResolution) {
	
	    values.clear();
	    m_model->getColumn(f / modelResolution, values);
	    
	    for (size_t y = 0; y < m_model->getHeight(); ++y) {

		float value = min;
		if (y < values.size()) {
                    value = values[y];
                    if (m_colourScale != LinearScale) {
                        value = fabs(value);
                    }
                }

		int pixel = int(((value - min) * 256) / (max - min));
                if (pixel < 0) pixel = 0;
		if (pixel > 255) pixel = 255;

		m_cache->setPixel(f / modelResolution, y, pixel);
	    }
	}
    }

    if (m_model->getHeight() >= v->height() ||
        modelResolution < v->getZoomLevel() / 2) {
        paintDense(v, paint, rect);
        return;
    }

    int x0 = rect.left();
    int x1 = rect.right() + 1;

    int h = v->height();

    // The cache is from the model's start frame to the model's end
    // frame at the model's window increment frames per pixel.  We
    // want to draw from our start frame + x0 * zoomLevel to our start
    // frame + x1 * zoomLevel at zoomLevel frames per pixel.

    //!!! Strictly speaking we want quite different paint mechanisms
    //for models that have more than one bin per pixel in either
    //direction.  This one is only really appropriate for models with
    //far fewer bins in both directions.

    float srRatio =
        float(v->getViewManager()->getMainModelSampleRate()) /
        float(m_model->getSampleRate());

    int sx0 = int((v->getFrameForX(x0) / srRatio - long(modelStart)) / long(modelResolution));
    int sx1 = int((v->getFrameForX(x1) / srRatio - long(modelStart)) / long(modelResolution));
    int sh = m_model->getHeight();

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    std::cerr << "Colour3DPlotLayer::paint: w " << w << ", h " << h << ", sx0 " << sx0 << ", sx1 " << sx1 << ", sw " << sw << ", sh " << sh << std::endl;
    std::cerr << "Colour3DPlotLayer: sample rate is " << m_model->getSampleRate() << ", resolution " << m_model->getResolution() << std::endl;
#endif

    QPoint illuminatePos;
    bool illuminate = v->shouldIlluminateLocalFeatures(this, illuminatePos);
    char labelbuf[10];

    for (int sx = sx0 - 1; sx <= sx1; ++sx) {

	int fx = sx * int(modelResolution);

	if (fx + modelResolution < int(modelStart) ||
	    fx > int(modelEnd)) continue;

	int rx0 = v->getXForFrame((fx + int(modelStart)) * srRatio);
	int rx1 = v->getXForFrame((fx + int(modelStart) + int(modelResolution) + 1) * srRatio);

	int rw = rx1 - rx0;
	if (rw < 1) rw = 1;

	bool showLabel = (rw > 10 &&
			  paint.fontMetrics().width("0.000000") < rw - 3 &&
			  paint.fontMetrics().height() < (h / sh));
        
	for (int sy = 0; sy < sh; ++sy) {

	    int ry0 = h - (sy * h) / sh - 1;
	    QRgb pixel = qRgb(255, 255, 255);
	    if (sx >= 0 && sx < m_cache->width() &&
		sy >= 0 && sy < m_cache->height()) {
		pixel = m_cache->pixel(sx, sy);
	    }

	    QRect r(rx0, ry0 - h / sh - 1, rw, h / sh + 1);

            if (rw == 1) {
                paint.setPen(pixel);
                paint.setBrush(Qt::NoBrush);
                paint.drawLine(r.x(), r.y(), r.x(), r.y() + r.height() - 1);
                continue;
            }

	    QColor pen(255, 255, 255, 80);
	    QColor brush(pixel);

            if (rw > 3 && r.height() > 3) {
                brush.setAlpha(160);
            }

	    paint.setPen(Qt::NoPen);
	    paint.setBrush(brush);

	    if (illuminate) {
		if (r.contains(illuminatePos)) {
		    paint.setPen(Qt::black);//!!!
		}
	    }
            
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
            std::cerr << "rect " << r.x() << "," << r.y() << " "
                      << r.width() << "x" << r.height() << std::endl;
#endif

	    paint.drawRect(r);

	    if (showLabel) {
		if (sx >= 0 && sx < m_cache->width() &&
		    sy >= 0 && sy < m_cache->height()) {
		    float value = m_model->getValueAt(sx, sy);
		    sprintf(labelbuf, "%06f", value);
		    QString text(labelbuf);
		    paint.setPen(Qt::white);
		    paint.drawText(rx0 + 2,
				   ry0 - h / sh - 1 + 2 + paint.fontMetrics().ascent(),
				   text);
		}
	    }
	}
    }
}

void
Colour3DPlotLayer::paintDense(View *v, QPainter &paint, QRect rect) const
{
    long startFrame = v->getStartFrame();
    int zoomLevel = v->getZoomLevel();

    size_t modelStart = m_model->getStartFrame();
    size_t modelEnd = m_model->getEndFrame();
    size_t modelResolution = m_model->getResolution();

    float srRatio =
        float(v->getViewManager()->getMainModelSampleRate()) /
        float(m_model->getSampleRate());

    int x0 = rect.left();
    int x1 = rect.right() + 1;

    int w = x1 - x0;
    int h = v->height();
    int sh = m_model->getHeight();

    QImage img(w, h, QImage::Format_RGB32);

    for (int x = x0; x < x1; ++x) {

        long xf = v->getFrameForX(x) / srRatio;
        if (xf < 0) {
            for (int y = 0; y < h; ++y) {
                img.setPixel(x - x0, y, m_cache->color(0));
            }
            continue;
        }

        float sx0 = (float(xf) - modelStart) / modelResolution;
        float sx1 = (float(v->getFrameForX(x+1) / srRatio) - modelStart) / modelResolution;
            
        int sx0i = int(sx0 + 0.001);
        int sx1i = int(sx1);

        for (int y = 0; y < h; ++y) {

            float sy0 = (float(h - y - 1) * sh) / h;
            float sy1 = (float(h - y) * sh) / h;
            
            int sy0i = int(sy0 + 0.001);
            int sy1i = int(sy1);

            float mag = 0.0, div = 0.0;
            int max = 0;

            for (int sx = sx0i; sx <= sx1i; ++sx) {

                if (sx < 0 || sx >= m_cache->width()) continue;

                for (int sy = sy0i; sy <= sy1i; ++sy) {

                    if (sy < 0 || sy >= m_cache->height()) continue;

                    float prop = 1.0;
                    if (sx == sx0i) prop *= (sx + 1) - sx0;
                    if (sx == sx1i) prop *= sx1 - sx;
                    if (sy == sy0i) prop *= (sy + 1) - sy0;
                    if (sy == sy1i) prop *= sy1 - sy;

                    mag += prop * m_cache->pixelIndex(sx, sy);
                    max = std::max(max, m_cache->pixelIndex(sx, sy));
                    div += prop;
                }
            }

            if (div != 0) mag /= div;
            if (mag < 0) mag = 0;
            if (mag > 255) mag = 255;
            if (max < 0) max = 0;
            if (max > 255) max = 255;

            img.setPixel(x - x0, y, m_cache->color(int(mag + 0.001)));
//            img.setPixel(x - x0, y, m_cache->color(max));
        }
    }

    paint.drawImage(x0, 0, img);
}

bool
Colour3DPlotLayer::snapToFeatureFrame(View *v, int &frame,
				      size_t &resolution,
				      SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();
    int left = (frame / resolution) * resolution;
    int right = left + resolution;

    switch (snap) {
    case SnapLeft:  frame = left;  break;
    case SnapRight: frame = right; break;
    case SnapNearest:
    case SnapNeighbouring:
	if (frame - left > right - frame) frame = right;
	else frame = left;
	break;
    }
    
    return true;
}

#ifdef INCLUDE_MOCFILES
#include "Colour3DPlotLayer.moc.cpp"
#endif


