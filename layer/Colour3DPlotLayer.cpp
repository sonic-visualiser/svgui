/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#include "Colour3DPlotLayer.h"

#include "base/View.h"
#include "base/Profiler.h"

#include <QPainter>
#include <QImage>
#include <QRect>

#include <iostream>

#include <cassert>


Colour3DPlotLayer::Colour3DPlotLayer(View *w) :
    Layer(w),
    m_model(0),
    m_cache(0)
{
    m_view->addLayer(this);
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

void
Colour3DPlotLayer::paint(QPainter &paint, QRect rect) const
{
//    Profiler profiler("Colour3DPlotLayer::paint");
//    std::cerr << "Colour3DPlotLayer::paint(): m_model is " << m_model << ", zoom level is " << m_view->getZoomLevel() << std::endl;

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
	    paint.fillRect(0, 10, m_view->width() * completion / 100,
			   10, QColor(120, 120, 120));
	}
	return;
    }

    long startFrame = m_view->getStartFrame();
    int zoomLevel = m_view->getZoomLevel();

    size_t modelStart = m_model->getStartFrame();
    size_t modelEnd = m_model->getEndFrame();
    size_t modelWindow = m_model->getWindowSize();

    if (!m_cache) {

	m_cache = new QImage((modelEnd - modelStart) / modelWindow + 1,
			     m_model->getYBinCount(),
			     QImage::Format_Indexed8);

	m_cache->setNumColors(256);
	DenseThreeDimensionalModel::BinValueSet values;
/*
	for (int pixel = 0; pixel < 256; ++pixel) {
	    int hue = 256 - pixel;
//	    int hue = 220 - pixel;
//	    if (hue < 0) hue += 360;
	    QColor color = QColor::fromHsv(hue, pixel/2 + 128, pixel);
	    m_cache->setColor(pixel, qRgb(color.red(), color.green(), color.blue()));
	}
*/

	float min = m_model->getMinimumLevel();
	float max = m_model->getMaximumLevel();

	if (max == min) max = min + 1.0;

//	int min = lrintf(m_model->getMinimumLevel());
//	int max = lrintf(m_model->getMaximumLevel());
	for (int value = 0; value < 256; ++value) {
//	    int spread = ((value - min) * 256) / (max - min);
//	    int hue = 256 - spread;
//	    QColor color = QColor::fromHsv(hue, spread/2 + 128, spread);
	    int hue = 256 - value;
	    QColor color = QColor::fromHsv(hue, value/2 + 128, value);
	    m_cache->setColor(value, qRgba(color.red(), color.green(), color.blue(), 80));
//	    std::cerr << "Colour3DPlotLayer: Index " << value << ": hue " << hue << std::endl;
	}

	m_cache->fill(min);

	for (size_t f = modelStart; f <= modelEnd; f += modelWindow) {
	
	    values.clear();
	    m_model->getBinValues(f, values);
	    
	    for (size_t y = 0; y < m_model->getYBinCount(); ++y) {

		float value = min;
		if (y < values.size()) value = values[y];

		//!!! divide-by-zero!
		int pixel = int(((value - min) * 256) / (max - min));

		m_cache->setPixel(f / modelWindow, y, pixel);
	    }
	}
    }

    int x0 = rect.left();
    int x1 = rect.right() + 1;

//    int y0 = rect.top();
//    int y1 = rect.bottom();
    int w = x1 - x0;
    int h = m_view->height();

    // The cache is from the model's start frame to the model's end
    // frame at the model's window increment frames per pixel.  We
    // want to draw from our start frame + x0 * zoomLevel to our start
    // frame + x1 * zoomLevel at zoomLevel frames per pixel.

    //!!! Strictly speaking we want quite different paint mechanisms
    //for models that have more than one bin per pixel in either
    //direction.  This one is only really appropriate for models with
    //far fewer bins in both directions.

    int sx0 = ((startFrame + x0 * zoomLevel) - int(modelStart)) /
	int(modelWindow);
    int sx1 = ((startFrame + x1 * zoomLevel) - int(modelStart)) / 
	int(modelWindow);
    int sw = sx1 - sx0;
    int sh = m_model->getYBinCount();

/*
    std::cerr << "Colour3DPlotLayer::paint: w " << w << ", h " << h << ", sx0 " << sx0 << ", sx1 " << sx1 << ", sw " << sw << ", sh " << sh << std::endl;
    std::cerr << "Colour3DPlotLayer: sample rate is " << m_model->getSampleRate() << ", window size " << m_model->getWindowSize() << std::endl;
*/

    for (int sx = sx0 - 1; sx <= sx1; ++sx) {

	int fx = sx * int(modelWindow);

	if (fx + modelWindow < int(modelStart) ||
	    fx > int(modelEnd)) continue;

	for (int sy = 0; sy < sh; ++sy) {

	    int rx0 = ((fx + int(modelStart))
		       - int(startFrame)) / zoomLevel;
	    int rx1 = ((fx + int(modelWindow) + int(modelStart))
		       - int(startFrame)) / zoomLevel;

	    int ry0 = h - (sy * h) / sh - 1;
	    int ry1 = h - ((sy + 1) * h) / sh - 2;
	    QRgb pixel = qRgb(255, 255, 255);
	    if (sx >= 0 && sx < m_cache->width() &&
		sy >= 0 && sy < m_cache->height()) {
		pixel = m_cache->pixel(sx, sy);
	    }

	    QColor pen(255, 255, 255, 80);
//	    QColor pen(pixel);
	    QColor brush(pixel);
	    brush.setAlpha(160);
//	    paint.setPen(pen);
	    paint.setPen(Qt::NoPen);
	    paint.setBrush(brush);

	    int w = rx1 - rx0;
	    if (w < 1) w = 1;
	    paint.drawRect(rx0, ry0 - h / sh - 1, w, h / sh + 1);

	    if (sx >= 0 && sx < m_cache->width() &&
		sy >= 0 && sy < m_cache->height()) {
		int dv = m_cache->pixelIndex(sx, sy);
		if (dv != 0 && paint.fontMetrics().height() < (h / sh)) {
		    QString text = QString("%1").arg(dv);
		    if (paint.fontMetrics().width(text) < w - 3) {
			paint.setPen(Qt::white);
			paint.drawText(rx0 + 2,
				       ry0 - h / sh - 1 + 2 + paint.fontMetrics().ascent(),
				       QString("%1").arg(dv));
		    }
		}
	    }
	}
    }
    
/*
    QRect targetRect(x0, 0, w, h);
    QRect sourceRect(sx0, 0, sw, sh);

    QImage scaled(w, h, QImage::Format_RGB32);

    for (int x = 0; x < w; ++x) {
	for (int y = 0; y < h; ++y) {

	    

	    int sx = sx0 + (x * sw) / w;
	    int sy = sh - (y * sh) / h - 1;
//	    std::cerr << "Colour3DPlotLayer::paint: sx " << sx << ", sy " << sy << ", cache w " << m_cache->width() << ", cache h " << m_cache->height() << std::endl;
	    if (sx >= 0 && sy >= 0 &&
		sx < m_cache->width() && sy < m_cache->height()) {
		scaled.setPixel(x, y, m_cache->pixel(sx, sy));
	    } else {
		scaled.setPixel(x, y, qRgba(255, 255, 255, 80));
	    }
	}
    }

    paint.drawImage(x0, 0, scaled);
*/
}

#ifdef INCLUDE_MOCFILES
#include "Colour3DPlotLayer.moc.cpp"
#endif


