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

#include "Colour3DPlotLayer.h"

#include "base/View.h"
#include "base/Profiler.h"

#include <QPainter>
#include <QImage>
#include <QRect>

#include <iostream>

#include <cassert>


Colour3DPlotLayer::Colour3DPlotLayer() :
    Layer(),
    m_model(0),
    m_cache(0)
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
    size_t modelWindow = m_model->getWindowSize();

    int sx0 = modelWindow *
	int((v->getFrameForX(x) - long(modelStart)) / long(modelWindow));
    int sx1 = sx0 + modelWindow;

    float binHeight = float(v->height()) / m_model->getYBinCount();
    int sy = (v->height() - y) / binHeight;

    float value = m_model->getBinValue(sx0, sy);
    
    QString binName = m_model->getBinName(sy);
    if (binName == "") binName = QString("[%1]").arg(sy + 1);
    else binName = QString("%1 [%2]").arg(binName).arg(sy + 1);

    QString text = tr("Time:\t%1 - %2\nBin:\t%3\nValue:\t%4")
	.arg(RealTime::frame2RealTime(sx0, m_model->getSampleRate())
	     .toText(true).c_str())
	.arg(RealTime::frame2RealTime(sx1, m_model->getSampleRate())
	     .toText(true).c_str())
	.arg(binName)
	.arg(value);

    return text;
}

int
Colour3DPlotLayer::getVerticalScaleWidth(View *v, QPainter &paint) const
{
    if (!m_model) return 0;

    QString sampleText = QString("[%1]").arg(m_model->getYBinCount());
    int tw = paint.fontMetrics().width(sampleText);
    bool another = false;

    for (size_t i = 0; i < m_model->getYBinCount(); ++i) {
	if (m_model->getBinName(i).length() > sampleText.length()) {
	    sampleText = m_model->getBinName(i);
            another = true;
	}
    }
    if (another) {
	tw = std::max(tw, paint.fontMetrics().width(sampleText));
    }

    return tw + 13;
}

void
Colour3DPlotLayer::paintVerticalScale(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model) return;

    int h = rect.height(), w = rect.width();
    float binHeight = float(v->height()) / m_model->getYBinCount();

    int count = v->height() / paint.fontMetrics().height();
    int step = m_model->getYBinCount() / count;
    if (step == 0) step = 1;

    for (size_t i = 0; i < m_model->getYBinCount(); ++i) {

        if ((i % step) != 0) continue;

	int y0 = v->height() - (i * binHeight) - 1;
	
	QString text = m_model->getBinName(i);
	if (text == "") text = QString("[%1]").arg(i + 1);

	paint.drawLine(0, y0, w, y0);

	int cy = y0 - (step * binHeight)/2;
	int ty = cy + paint.fontMetrics().ascent()/2;

	paint.drawText(10, ty, text);
    }
}

void
Colour3DPlotLayer::paint(View *v, QPainter &paint, QRect rect) const
{
//    Profiler profiler("Colour3DPlotLayer::paint");
//    std::cerr << "Colour3DPlotLayer::paint(): m_model is " << m_model << ", zoom level is " << v->getZoomLevel() << std::endl;

    //!!! This doesn't yet accommodate the fact that the model may
    //have a different sample rate from an underlying model.  At the
    //moment our paint mechanism assumes all models have the same
    //sample rate.  If that isn't the case, they won't align and the
    //time ruler will match whichever model was used to construct it.
    //Obviously it is not going to be the case in general that models
    //will have the same samplerate, so we need a pane samplerate as
    //well which we trivially realign to.  (We can probably require
    //the waveform and spectrogram layers to display at the pane
    //samplerate.)

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
    size_t modelWindow = m_model->getWindowSize();

    size_t cacheWidth = (modelEnd - modelStart) / modelWindow + 1;
    size_t cacheHeight = m_model->getYBinCount();

    if (m_cache &&
	(m_cache->width() != cacheWidth ||
	 m_cache->height() != cacheHeight)) {

	delete m_cache;
	m_cache = 0;
    }

    if (!m_cache) {

	m_cache = new QImage(cacheWidth, cacheHeight, QImage::Format_Indexed8);

	m_cache->setNumColors(256);
	DenseThreeDimensionalModel::BinValueSet values;

	float min = m_model->getMinimumLevel();
	float max = m_model->getMaximumLevel();

	if (max == min) max = min + 1.0;

	for (int value = 0; value < 256; ++value) {
	    int hue = 256 - value;
	    QColor colour = QColor::fromHsv(hue, value/2 + 128, value);
	    m_cache->setColor(value, qRgba(colour.red(), colour.green(), colour.blue(), 80));
	}

	m_cache->fill(min);

	for (size_t f = modelStart; f <= modelEnd; f += modelWindow) {
	
	    values.clear();
	    m_model->getBinValues(f, values);
	    
	    for (size_t y = 0; y < m_model->getYBinCount(); ++y) {

		float value = min;
		if (y < values.size()) value = values[y];

		int pixel = int(((value - min) * 256) / (max - min));
                if (pixel < 0) pixel = 0;
		if (pixel > 255) pixel = 255;

		m_cache->setPixel(f / modelWindow, y, pixel);
	    }
	}
    }

    if (m_model->getYBinCount() >= v->height()) {
        paintDense(v, paint, rect);
        return;
    }

    int x0 = rect.left();
    int x1 = rect.right() + 1;

    int w = x1 - x0;
    int h = v->height();

    // The cache is from the model's start frame to the model's end
    // frame at the model's window increment frames per pixel.  We
    // want to draw from our start frame + x0 * zoomLevel to our start
    // frame + x1 * zoomLevel at zoomLevel frames per pixel.

    //!!! Strictly speaking we want quite different paint mechanisms
    //for models that have more than one bin per pixel in either
    //direction.  This one is only really appropriate for models with
    //far fewer bins in both directions.

    int sx0 = int((v->getFrameForX(x0) - long(modelStart)) / long(modelWindow));
    int sx1 = int((v->getFrameForX(x1) - long(modelStart)) / long(modelWindow));
    int sw = sx1 - sx0;
    int sh = m_model->getYBinCount();

/*
    std::cerr << "Colour3DPlotLayer::paint: w " << w << ", h " << h << ", sx0 " << sx0 << ", sx1 " << sx1 << ", sw " << sw << ", sh " << sh << std::endl;
    std::cerr << "Colour3DPlotLayer: sample rate is " << m_model->getSampleRate() << ", window size " << m_model->getWindowSize() << std::endl;
*/

    QPoint illuminatePos;
    bool illuminate = v->shouldIlluminateLocalFeatures(this, illuminatePos);
    char labelbuf[10];

    for (int sx = sx0 - 1; sx <= sx1; ++sx) {

	int fx = sx * int(modelWindow);

	if (fx + modelWindow < int(modelStart) ||
	    fx > int(modelEnd)) continue;

	int rx0 = v->getXForFrame(fx + int(modelStart));
	int rx1 = v->getXForFrame(fx + int(modelStart) + int(modelWindow));

	int w = rx1 - rx0;
	if (w < 1) w = 1;

	bool showLabel = (w > 10 &&
			  paint.fontMetrics().width("0.000000") < w - 3 &&
			  paint.fontMetrics().height() < (h / sh));
        
	for (int sy = 0; sy < sh; ++sy) {

	    int ry0 = h - (sy * h) / sh - 1;
	    QRgb pixel = qRgb(255, 255, 255);
	    if (sx >= 0 && sx < m_cache->width() &&
		sy >= 0 && sy < m_cache->height()) {
		pixel = m_cache->pixel(sx, sy);
	    }

	    QColor pen(255, 255, 255, 80);
	    QColor brush(pixel);
	    brush.setAlpha(160);
	    paint.setPen(Qt::NoPen);
	    paint.setBrush(brush);

	    QRect r(rx0, ry0 - h / sh - 1, w, h / sh + 1);

	    if (illuminate) {
		if (r.contains(illuminatePos)) {
		    paint.setPen(Qt::black);//!!!
		}
	    }
            
//            std::cout << "rect " << rx0 << "," << (ry0 - h / sh - 1) << " "
//                      << w << "x" << (h / sh + 1) << std::endl;

	    paint.drawRect(r);

	    if (showLabel) {
		if (sx >= 0 && sx < m_cache->width() &&
		    sy >= 0 && sy < m_cache->height()) {
		    float value = m_model->getBinValue(fx, sy);
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
    size_t modelWindow = m_model->getWindowSize();

    int x0 = rect.left();
    int x1 = rect.right() + 1;

    int w = x1 - x0;
    int h = v->height();
    int sh = m_model->getYBinCount();

    QImage img(w, h, QImage::Format_RGB32);

    for (int x = x0; x < x1; ++x) {

        long xf = v->getFrameForX(x);
        if (xf < 0) {
            for (int y = 0; y < h; ++y) {
                img.setPixel(x - x0, y, m_cache->color(0));
            }
            continue;
        }

        float sx0 = (float(xf) - modelStart) / modelWindow;
        float sx1 = (float(v->getFrameForX(x+1)) - modelStart) / modelWindow;
            
        int sx0i = int(sx0 + 0.001);
        int sx1i = int(sx1);

        for (int y = 0; y < h; ++y) {

            float sy0 = (float(h - y - 1) * sh) / h;
            float sy1 = (float(h - y) * sh) / h;
            
            int sy0i = int(sy0 + 0.001);
            int sy1i = int(sy1);

            float mag = 0.0, div = 0.0;

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
                    div += prop;
                }
            }

            if (div != 0) mag /= div;
            if (mag < 0) mag = 0;
            if (mag > 255) mag = 255;

            img.setPixel(x - x0, y, m_cache->color(int(mag + 0.001)));
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

    resolution = m_model->getWindowSize();
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


