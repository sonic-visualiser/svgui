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
#include "base/LogRange.h"
#include "ColourMapper.h"

#include <QPainter>
#include <QImage>
#include <QRect>

#include <iostream>

#include <cassert>

//#define DEBUG_COLOUR_3D_PLOT_LAYER_PAINT 1


Colour3DPlotLayer::Colour3DPlotLayer() :
    m_model(0),
    m_cache(0),
    m_cacheStart(0),
    m_colourScale(LinearScale),
    m_colourMap(0),
    m_normalizeColumns(false),
    m_normalizeVisibleArea(false)
{
    
}

Colour3DPlotLayer::~Colour3DPlotLayer()
{
}

void
Colour3DPlotLayer::setModel(const DenseThreeDimensionalModel *model)
{
    if (m_model == model) return;
    const DenseThreeDimensionalModel *oldModel = m_model;
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
    emit sliceableModelReplaced(oldModel, model);
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
    list.push_back("Colour");
    list.push_back("Colour Scale");
    list.push_back("Normalize Columns");
    list.push_back("Normalize Visible Area");
    return list;
}

QString
Colour3DPlotLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    if (name == "Colour Scale") return tr("Scale");
    if (name == "Normalize Columns") return tr("Normalize Columns");
    if (name == "Normalize Visible Area") return tr("Normalize Visible Area");
    return "";
}

Layer::PropertyType
Colour3DPlotLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Normalize Columns") return ToggleProperty;
    if (name == "Normalize Visible Area") return ToggleProperty;
    return ValueProperty;
}

QString
Colour3DPlotLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Normalize Columns" ||
        name == "Normalize Visible Area" ||
	name == "Colour Scale") return tr("Scale");
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
	*max = 2;

	deft = (int)m_colourScale;

    } else if (name == "Colour") {

	*min = 0;
	*max = ColourMapper::getColourMapCount() - 1;

	deft = m_colourMap;

    } else if (name == "Normalize Columns") {
	
	deft = (m_normalizeColumns ? 1 : 0);

    } else if (name == "Normalize Visible Area") {
	
	deft = (m_normalizeVisibleArea ? 1 : 0);

    } else {
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
Colour3DPlotLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == "Colour") {
        return ColourMapper::getColourMapName(value);
    }
    if (name == "Colour Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear Scale");
	case 1: return tr("Log Scale");
	case 2: return tr("+/-1 Scale");
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
	case 1: setColourScale(LogScale); break;
	case 2: setColourScale(PlusMinusOneScale); break;
	}
    } else if (name == "Colour") {
        setColourMap(value);
    } else if (name == "Normalize Columns") {
	setNormalizeColumns(value ? true : false);
    } else if (name == "Normalize Visible Area") {
	setNormalizeVisibleArea(value ? true : false);
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

void
Colour3DPlotLayer::setColourMap(int map)
{
    if (m_colourMap == map) return;
    m_colourMap = map;
    cacheInvalid();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setNormalizeColumns(bool n)
{
    if (m_normalizeColumns == n) return;
    m_normalizeColumns = n;
    cacheInvalid();
    emit layerParametersChanged();
}

bool
Colour3DPlotLayer::getNormalizeColumns() const
{
    return m_normalizeColumns;
}

void
Colour3DPlotLayer::setNormalizeVisibleArea(bool n)
{
    if (m_normalizeVisibleArea == n) return;
    m_normalizeVisibleArea = n;
    cacheInvalid();
    emit layerParametersChanged();
}

bool
Colour3DPlotLayer::getNormalizeVisibleArea() const
{
    return m_normalizeVisibleArea;
}

bool
Colour3DPlotLayer::isLayerScrollable(const View *v) const
{
    if (m_normalizeVisibleArea) return false;
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
Colour3DPlotLayer::getColumn(size_t col,
                             DenseThreeDimensionalModel::Column &values) const
{
    m_model->getColumn(col, values);

    float colMax = 0.f;

    float min = 0.f, max = 0.f;
    if (m_normalizeColumns) {
        min = m_model->getMinimumLevel();
        max = m_model->getMaximumLevel();
    }

    if (m_normalizeColumns) {
        for (size_t y = 0; y < values.size(); ++y) {
            if (values[y] > colMax || y == 0) colMax = values[y];
        }
        if (m_colourScale == LogScale) {
            colMax = LogRange::map(colMax);
        }
    }
    
    for (size_t y = 0; y < values.size(); ++y) {
        
        float value = min;

        value = values[y];
        if (m_colourScale == LogScale) {
            value = LogRange::map(value);
        }

        if (m_normalizeColumns) {
            if (colMax != 0) {
                value = max * (value / colMax);
            } else {
                value = 0;
            }
        }

        values[y] = value;
    }    
}
    
void
Colour3DPlotLayer::fillCache(size_t firstBin, size_t lastBin) const
{
    size_t modelStart = m_model->getStartFrame();
    size_t modelEnd = m_model->getEndFrame();
    size_t modelResolution = m_model->getResolution();

    std::cerr << "Colour3DPlotLayer::fillCache: " << firstBin << " -> " << lastBin << std::endl;

    if (!m_normalizeVisibleArea || m_normalizeColumns) {
        firstBin = modelStart / modelResolution;
        lastBin = modelEnd / modelResolution;
    }

    size_t cacheWidth = lastBin - firstBin + 1;
    size_t cacheHeight = m_model->getHeight();

    if (m_cache &&
	(m_cacheStart != firstBin ||
         m_cache->width() != cacheWidth ||
	 m_cache->height() != cacheHeight)) {

	delete m_cache;
	m_cache = 0;
    }

    if (m_cache) return;

    m_cache = new QImage(cacheWidth, cacheHeight, QImage::Format_Indexed8);
    m_cacheStart = firstBin;

    std::cerr << "Cache size " << cacheWidth << "x" << cacheHeight << " starting " << m_cacheStart << std::endl;

    m_cache->setNumColors(256);
    DenseThreeDimensionalModel::Column values;

    float min = m_model->getMinimumLevel();
    float max = m_model->getMaximumLevel();

    if (m_colourScale == LogScale) {
        LogRange::mapRange(min, max);
    } else if (m_colourScale == PlusMinusOneScale) {
        min = -1.f;
        max = 1.f;
    }
    
    if (max == min) max = min + 1.0;
    
    ColourMapper mapper(m_colourMap, 0.f, 255.f);
    
    for (int index = 0; index < 256; ++index) {
        
        QColor colour = mapper.map(index);
        m_cache->setColor(index, qRgb(colour.red(), colour.green(), colour.blue()));
    }
    
    m_cache->fill(0);

    float visibleMax = 0.f;

    if (m_normalizeVisibleArea && !m_normalizeColumns) {
        
        for (size_t c = firstBin; c <= lastBin; ++c) {
	
            values.clear();
            getColumn(c, values);

            float colMax = 0.f;

            for (size_t y = 0; y < m_model->getHeight(); ++y) {
                if (y >= values.size()) break;
                if (y == 0 || values[y] > colMax) colMax = values[y];
            }

            if (c == firstBin || colMax > visibleMax) visibleMax = colMax;
        }
    }
    
    for (size_t c = firstBin; c <= lastBin; ++c) {
	
        values.clear();
        getColumn(c, values);

        for (size_t y = 0; y < m_model->getHeight(); ++y) {

            float value = min;
            if (y < values.size()) {
                value = values[y];
            }
            
            if (m_normalizeVisibleArea && !m_normalizeColumns) {
                if (visibleMax != 0) {
                    value = max * (value / visibleMax);
                }
            }

            int pixel = int(((value - min) * 256) / (max - min));
            if (pixel < 0) pixel = 0;
            if (pixel > 255) pixel = 255;

            m_cache->setPixel(c - firstBin, y, pixel);
        }
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

    if (m_normalizeVisibleArea && !m_normalizeColumns) rect = v->rect();

    size_t modelStart = m_model->getStartFrame();
    size_t modelEnd = m_model->getEndFrame();
    size_t modelResolution = m_model->getResolution();

    // The cache is from the model's start frame to the model's end
    // frame at the model's window increment frames per pixel.  We
    // want to draw from our start frame + x0 * zoomLevel to our start
    // frame + x1 * zoomLevel at zoomLevel frames per pixel.

    //  We have quite different paint mechanisms for rendering "large"
    //  bins (more than one bin per pixel in both directions) and
    //  "small".  This is "large"; see paintDense below for "small".

    int x0 = rect.left();
    int x1 = rect.right() + 1;

    int h = v->height();

    float srRatio =
        float(v->getViewManager()->getMainModelSampleRate()) /
        float(m_model->getSampleRate());

    int sx0 = int((v->getFrameForX(x0) / srRatio - long(modelStart))
                  / long(modelResolution));
    int sx1 = int((v->getFrameForX(x1) / srRatio - long(modelStart))
                  / long(modelResolution));
    int sh = m_model->getHeight();

    if (sx0 > 0) --sx0;
    fillCache(sx0 < 0 ? 0 : sx0,
              sx1 < 0 ? 0 : sx1);

    if (m_model->getHeight() >= v->height() ||
        modelResolution < v->getZoomLevel() / 2) {
        paintDense(v, paint, rect);
        return;
    }

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    std::cerr << "Colour3DPlotLayer::paint: w " << w << ", h " << h << ", sx0 " << sx0 << ", sx1 " << sx1 << ", sw " << sw << ", sh " << sh << std::endl;
    std::cerr << "Colour3DPlotLayer: sample rate is " << m_model->getSampleRate() << ", resolution " << m_model->getResolution() << std::endl;
#endif

    QPoint illuminatePos;
    bool illuminate = v->shouldIlluminateLocalFeatures(this, illuminatePos);
    char labelbuf[10];

    for (int sx = sx0; sx <= sx1; ++sx) {

        int scx = 0;
        if (sx > m_cacheStart) scx = sx - m_cacheStart;
        
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
	    if (scx >= 0 && scx < m_cache->width() &&
		sy >= 0 && sy < m_cache->height()) {
		pixel = m_cache->pixel(scx, sy);
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
		if (scx >= 0 && scx < m_cache->width() &&
		    sy >= 0 && sy < m_cache->height()) {
		    float value = m_model->getValueAt(scx, sy);
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

                int scx = 0;
                if (sx > m_cacheStart) scx = sx - m_cacheStart;
        
                if (scx < 0 || scx >= m_cache->width()) continue;

                for (int sy = sy0i; sy <= sy1i; ++sy) {

                    if (sy < 0 || sy >= m_cache->height()) continue;

                    float prop = 1.0;
                    if (sx == sx0i) prop *= (sx + 1) - sx0;
                    if (sx == sx1i) prop *= sx1 - sx;
                    if (sy == sy0i) prop *= (sy + 1) - sy0;
                    if (sy == sy1i) prop *= sy1 - sy;

                    mag += prop * m_cache->pixelIndex(scx, sy);
                    max = std::max(max, m_cache->pixelIndex(scx, sy));
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

QString
Colour3DPlotLayer::toXmlString(QString indent, QString extraAttributes) const
{
    QString s;
    
    s += QString("scale=\"%1\" "
                 "colourScheme=\"%2\" "
		 "normalizeColumns=\"%3\" "
                 "normalizeVisibleArea=\"%4\"")
	.arg((int)m_colourScale)
        .arg(m_colourMap)
        .arg(m_normalizeColumns ? "true" : "false")
        .arg(m_normalizeVisibleArea ? "true" : "false");

    return Layer::toXmlString(indent, extraAttributes + " " + s);
}

void
Colour3DPlotLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false;

    ColourScale scale = (ColourScale)attributes.value("scale").toInt(&ok);
    if (ok) setColourScale(scale);

    int colourMap = attributes.value("colourScheme").toInt(&ok);
    if (ok) setColourMap(colourMap);

    bool normalizeColumns =
        (attributes.value("normalizeColumns").trimmed() == "true");
    setNormalizeColumns(normalizeColumns);

    bool normalizeVisibleArea =
        (attributes.value("normalizeVisibleArea").trimmed() == "true");
    setNormalizeVisibleArea(normalizeVisibleArea);
}

#ifdef INCLUDE_MOCFILES
#include "Colour3DPlotLayer.moc.cpp"
#endif


