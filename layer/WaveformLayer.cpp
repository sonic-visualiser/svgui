/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
   
    This is experimental software.  Not for distribution.
*/

#include "WaveformLayer.h"

#include "base/AudioLevel.h"
#include "base/View.h"
#include "base/Profiler.h"

#include <QPainter>
#include <QPixmap>

#include <iostream>
#include <cmath>

//#define DEBUG_WAVEFORM_PAINT 1

using std::cerr;
using std::endl;

WaveformLayer::WaveformLayer(View *w) :
    Layer(w),
    m_model(0),
    m_gain(1.0f),
    m_colour(Qt::black),
    m_showMeans(true),
    m_greyscale(true),
    m_channelMode(SeparateChannels),
    m_channel(-1),
    m_scale(LinearScale),
    m_aggressive(false),
    m_cache(0),
    m_cacheValid(false)
{
    m_view->addLayer(this);
}

WaveformLayer::~WaveformLayer()
{
    delete m_cache;
}

void
WaveformLayer::setModel(const RangeSummarisableTimeValueModel *model)
{
    m_model = model;
    m_cacheValid = false;
    if (!m_model || !m_model->isOK()) return;

    connect(m_model, SIGNAL(modelChanged()), this, SIGNAL(modelChanged()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SIGNAL(modelChanged(size_t, size_t)));

    connect(m_model, SIGNAL(completionChanged()),
	    this, SIGNAL(modelCompletionChanged()));

    emit modelReplaced();
}

Layer::PropertyList
WaveformLayer::getProperties() const
{
    PropertyList list;
    list.push_back(tr("Colour"));
    list.push_back(tr("Scale"));
    list.push_back(tr("Gain"));
    list.push_back(tr("Merge Channels"));
    return list;
}

Layer::PropertyType
WaveformLayer::getPropertyType(const PropertyName &name) const
{
    if (name == tr("Gain")) return RangeProperty;
    if (name == tr("Colour")) return ValueProperty;
    if (name == tr("Merge Channels")) return ToggleProperty;
    if (name == tr("Scale")) return ValueProperty;
    return InvalidProperty;
}

QString
WaveformLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == tr("Gain") ||
	name == tr("Scale")) return tr("Scale");
    return QString();
}

int
WaveformLayer::getPropertyRangeAndValue(const PropertyName &name,
					 int *min, int *max) const
{
    int deft = 0;

    int throwaway;
    if (!min) min = &throwaway;
    if (!max) max = &throwaway;

    if (name == tr("Gain")) {

	*min = -50;
	*max = 50;

	deft = lrint(log10(m_gain) * 20.0);
	if (deft < *min) deft = *min;
	if (deft > *max) deft = *max;

    } else if (name == tr("Colour")) {

	*min = 0;
	*max = 5;

	if (m_colour == Qt::black) deft = 0;
	else if (m_colour == Qt::darkRed) deft = 1;
	else if (m_colour == Qt::darkBlue) deft = 2;
	else if (m_colour == Qt::darkGreen) deft = 3;
	else if (m_colour == QColor(200, 50, 255)) deft = 4;
	else if (m_colour == QColor(255, 150, 50)) deft = 5;

    } else if (name == tr("Merge Channels")) {

	deft = ((m_channelMode == MergeChannels) ? 1 : 0);

    } else if (name == tr("Scale")) {

	*min = 0;
	*max = 2;

	deft = (int)m_scale;

    } else {
	deft = Layer::getPropertyRangeAndValue(name, min, max);
    }

    return deft;
}

QString
WaveformLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == tr("Colour")) {
	switch (value) {
	default:
	case 0: return tr("Black");
	case 1: return tr("Red");
	case 2: return tr("Blue");
	case 3: return tr("Green");
	case 4: return tr("Purple");
	case 5: return tr("Orange");
	}
    }
    if (name == tr("Scale")) {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Meter");
	case 2: return tr("dB");
	}
    }
    return tr("<unknown>");
}

void
WaveformLayer::setProperty(const PropertyName &name, int value)
{
    if (name == tr("Gain")) {
	setGain(pow(10, float(value)/20.0));
    } else if (name == tr("Colour")) {
	switch (value) {
	default:
	case 0:	setBaseColour(Qt::black); break;
	case 1: setBaseColour(Qt::darkRed); break;
	case 2: setBaseColour(Qt::darkBlue); break;
	case 3: setBaseColour(Qt::darkGreen); break;
	case 4: setBaseColour(QColor(200, 50, 255)); break;
	case 5: setBaseColour(QColor(255, 150, 50)); break;
	}
    } else if (name == tr("Merge Channels")) {
	setChannelMode(value ? MergeChannels : SeparateChannels);
    } else if (name == tr("Scale")) {
	switch (value) {
	default:
	case 0: setScale(LinearScale); break;
	case 1: setScale(MeterScale); break;
	case 2: setScale(dBScale); break;
	}
    }
}

/*

int
WaveformLayer::getProperty(const PropertyName &name)
{
    if (name == "Gain") {
	return int((getGain() - 1.0) * 10.0 + 0.01);
    }
    if (name == "Colour") {
	
*/

void
WaveformLayer::setGain(float gain) //!!! inadequate for floats!
{
    if (m_gain == gain) return;
    m_gain = gain;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setBaseColour(QColor colour)
{
    if (m_colour == colour) return;
    m_colour = colour;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setShowMeans(bool showMeans)
{
    if (m_showMeans == showMeans) return;
    m_showMeans = showMeans;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setUseGreyscale(bool useGreyscale)
{
    if (m_greyscale == useGreyscale) return;
    m_greyscale = useGreyscale;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setChannelMode(ChannelMode channelMode)
{
    if (m_channelMode == channelMode) return;
    m_channelMode = channelMode;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setChannel(int channel)
{
    std::cerr << "WaveformLayer::setChannel(" << channel << ")" << std::endl;

    if (m_channel == channel) return;
    m_channel = channel;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setScale(Scale scale)
{
    if (m_scale == scale) return;
    m_scale = scale;
    m_cacheValid = false;
    emit layerParametersChanged();
}

void
WaveformLayer::setAggressiveCacheing(bool aggressive)
{
    if (m_aggressive == aggressive) return;
    m_aggressive = aggressive;
    m_cacheValid = false;
    emit layerParametersChanged();
}

int
WaveformLayer::getCompletion() const
{
    int completion = 100;
    if (!m_model || !m_model->isOK()) return completion;
    if (m_model->isReady(&completion)) return 100;
    return completion;
}

int
WaveformLayer::dBscale(float sample, int m) const
{
    if (sample < 0.0) return -dBscale(-sample, m);
    float dB = AudioLevel::multiplier_to_dB(sample);
    if (dB < -50.0) return 0;
    if (dB > 0.0) return m;
    return int(((dB + 50.0) * m) / 50.0 + 0.1);
}

size_t
WaveformLayer::getChannelArrangement(size_t &min, size_t &max, bool &merging)
    const
{
    if (!m_model || !m_model->isOK()) return 0;

    size_t channels = m_model->getChannelCount();
    if (channels == 0) return 0;

    size_t rawChannels = channels;

    if (m_channel == -1) {
	min = 0;
	if (m_channelMode == MergeChannels) {
	    max = 0;
	    channels = 1;
	} else {
	    max = channels - 1;
	}
    } else {
	min = m_channel;
	max = m_channel;
	rawChannels = 1;
	channels = 1;
    }

    merging = (m_channelMode == MergeChannels && rawChannels > 1);

//    std::cerr << "WaveformLayer::getChannelArrangement: min " << min << ", max " << max << ", merging " << merging << ", channels " << channels << std::endl;

    return channels;
}    

void
WaveformLayer::paint(QPainter &viewPainter, QRect rect) const
{
    if (!m_model || !m_model->isOK()) {
	return;
    }
  
    long startFrame = m_view->getStartFrame();
    int zoomLevel = m_view->getZoomLevel();

#ifdef DEBUG_WAVEFORM_PAINT
    Profiler profiler("WaveformLayer::paint", true);
    std::cerr << "WaveformLayer::paint (" << rect.x() << "," << rect.y()
	      << ") [" << rect.width() << "x" << rect.height() << "]: zoom " << zoomLevel << ", start " << startFrame << std::endl;
#endif

    size_t channels = 0, minChannel = 0, maxChannel = 0;
    bool mergingChannels = false;

    channels = getChannelArrangement(minChannel, maxChannel, mergingChannels);
    if (channels == 0) return;

    int w = m_view->width();
    int h = m_view->height();

    bool ready = m_model->isReady();
    QPainter *paint;

    if (m_aggressive) {

	if (m_cacheValid && (zoomLevel != m_cacheZoomLevel)) {
	    m_cacheValid = false;
	}

	if (m_cacheValid) {
	    viewPainter.drawPixmap(rect, *m_cache, rect);
	    return;
	}

	if (!m_cache || m_cache->width() != w || m_cache->height() != h) {
	    delete m_cache;
	    m_cache = new QPixmap(w, h);
	}

	paint = new QPainter(m_cache);

	paint->setPen(Qt::NoPen);
	paint->setBrush(m_view->palette().background());
	paint->drawRect(rect);

	paint->setPen(Qt::black);
	paint->setBrush(Qt::NoBrush);

    } else {
	paint = &viewPainter;
    }

    paint->setRenderHint(QPainter::Antialiasing, false);

    int x0 = 0, x1 = w - 1;
    int y0 = 0, y1 = h - 1;

    x0 = rect.left();
    x1 = rect.right();
    y0 = rect.top();
    y1 = rect.bottom();

    if (x0 > 0) --x0;
    if (x1 < m_view->width()) ++x1;

    long frame0 = getFrameForX(x0);
    long frame1 = getFrameForX(x1 + 1);
     
#ifdef DEBUG_WAVEFORM_PAINT
    std::cerr << "Painting waveform from " << frame0 << " to " << frame1 << " (" << (x1-x0+1) << " pixels at zoom " << zoomLevel << ")" <<  std::endl;
#endif

    RangeSummarisableTimeValueModel::RangeBlock ranges;
    RangeSummarisableTimeValueModel::RangeBlock otherChannelRanges;
    RangeSummarisableTimeValueModel::Range range;
    
    QColor greys[3];
    if (m_colour == Qt::black) {
	for (int i = 0; i < 3; ++i) { // 0 lightest, 2 darkest
	    int level = 192 - 64 * i;
	    greys[i] = QColor(level, level, level);
	}
    } else {
	int h, s, v;
	m_colour.getHsv(&h, &s, &v);
	for (int i = 0; i < 3; ++i) { // 0 lightest, 2 darkest
	    if (m_view->hasLightBackground()) {
		greys[i] = QColor::fromHsv(h, s * (i + 1) / 4, v);
	    } else {
		greys[i] = QColor::fromHsv(h, s * (3 - i) / 4, v);
	    }
	}
    }
        
    QColor midColour = m_colour;
    if (midColour == Qt::black) {
	midColour = Qt::gray;
    } else if (m_view->hasLightBackground()) {
	midColour = midColour.light(150);
    } else {
	midColour = midColour.light(50);
    }

    for (size_t ch = minChannel; ch <= maxChannel; ++ch) {

	int prevRangeBottom = -1, prevRangeTop = -1;
	QColor prevRangeBottomColour = m_colour, prevRangeTopColour = m_colour;

	int m = (h / channels) / 2;
	int my = m + (((ch - minChannel) * h) / channels);
	
//	std::cerr << "ch = " << ch << ", channels = " << channels << ", m = " << m << ", my = " << my << ", h = " << h << std::endl;

	if (my - m > y1 || my + m < y0) continue;

	paint->setPen(greys[0]);
	paint->drawLine(x0, my, x1, my);

	if (frame1 <= 0) continue;

	size_t modelZoomLevel = zoomLevel;

	ranges = m_model->getRanges
	    (ch, frame0 < 0 ? 0 : frame0, frame1, modelZoomLevel);
        
	if (mergingChannels) {
	    otherChannelRanges = m_model->getRanges
		(1, frame0 < 0 ? 0 : frame0, frame1, modelZoomLevel);
	}

	for (int x = x0; x <= x1; ++x) {

	    range = RangeSummarisableTimeValueModel::Range();
	    size_t index = x - x0;
	    size_t maxIndex = index;

	    if (frame0 < 0) {
		if (index < size_t(-frame0 / zoomLevel)) {
		    continue;
		} else {
		    index -= -frame0 / zoomLevel;
		    maxIndex = index;
		}
	    }
            
	    if (int(modelZoomLevel) != zoomLevel) {

		index = size_t((double(index) * zoomLevel) / modelZoomLevel);

		if (int(modelZoomLevel) < zoomLevel) {
		    // Peaks may be missed!  The model should avoid
		    // this by rounding zoom levels up rather than
		    // down, but we'd better cope in case it doesn't
		    maxIndex = index;
		} else {
		    maxIndex = size_t((double(index + 1) * zoomLevel)
				      / modelZoomLevel) - 1;
		}
	    }

	    if (index < ranges.size()) {

		range = ranges[index];

		if (maxIndex > index && maxIndex < ranges.size()) {
		    range.max = std::max(range.max, ranges[maxIndex].max);
		    range.min = std::min(range.min, ranges[maxIndex].min);
		    range.absmean = (range.absmean +
				     ranges[maxIndex].absmean) / 2;
		}

	    } else {
		continue;
	    }

	    int rangeBottom = 0, rangeTop = 0, meanBottom = 0, meanTop = 0;

	    if (mergingChannels) {

		if (index < otherChannelRanges.size()) {

		    range.max = fabsf(range.max);
		    range.min = -fabsf(otherChannelRanges[index].max);
		    range.absmean = (range.absmean +
				     otherChannelRanges[index].absmean) / 2;

		    if (maxIndex > index && maxIndex < ranges.size()) {
			// let's not concern ourselves about the mean
			range.min = std::min
			    (range.min,
			     -fabsf(otherChannelRanges[maxIndex].max));
		    }
		}
	    }

	    int greyLevels = 1;
	    if (m_greyscale && (m_scale == LinearScale)) greyLevels = 4;

	    switch (m_scale) {

	    case LinearScale:
		rangeBottom = int( m * greyLevels * range.min * m_gain);
		rangeTop    = int( m * greyLevels * range.max * m_gain);
		meanBottom  = int(-m * range.absmean * m_gain);
		meanTop     = int( m * range.absmean * m_gain);
		break;

	    case dBScale:
		rangeBottom =  dBscale(range.min * m_gain, m * greyLevels);
		rangeTop    =  dBscale(range.max * m_gain, m * greyLevels);
		meanBottom  = -dBscale(range.absmean * m_gain, m);
		meanTop     =  dBscale(range.absmean * m_gain, m);
		break;

	    case MeterScale:
		rangeBottom =  AudioLevel::multiplier_to_preview(range.min * m_gain, m * greyLevels);
		rangeTop    =  AudioLevel::multiplier_to_preview(range.max * m_gain, m * greyLevels);
		meanBottom  = -AudioLevel::multiplier_to_preview(range.absmean * m_gain, m);
		meanTop     =  AudioLevel::multiplier_to_preview(range.absmean * m_gain, m);
	    }

	    rangeBottom = my * greyLevels - rangeBottom;
	    rangeTop    = my * greyLevels - rangeTop;
	    meanBottom  = my - meanBottom;
	    meanTop     = my - meanTop;

	    int topFill = (rangeTop % greyLevels);
	    if (topFill > 0) topFill = greyLevels - topFill;

	    int bottomFill = (rangeBottom % greyLevels);

	    rangeTop = rangeTop / greyLevels;
	    rangeBottom = rangeBottom / greyLevels;

	    bool clipped = false;

	    if (rangeTop < my - m) { rangeTop = my - m; }
	    if (rangeTop > my + m) { rangeTop = my + m; }
	    if (rangeBottom < my - m) { rangeBottom = my - m; }
	    if (rangeBottom > my + m) { rangeBottom = my + m; }

	    if (range.max * m_gain <= -1.0 ||
		range.max * m_gain >= 1.0) clipped = true;
	    
	    if (meanBottom > rangeBottom) meanBottom = rangeBottom;
	    if (meanTop < rangeTop) meanTop = rangeTop;

	    bool drawMean = m_showMeans;
	    if (meanTop == rangeTop) {
		if (meanTop < meanBottom) ++meanTop;
		else drawMean = false;
	    }
	    if (meanBottom == rangeBottom) {
		if (meanBottom > meanTop) --meanBottom;
		else drawMean = false;
	    }

	    if (x != x0 && prevRangeBottom != -1) {
		if (prevRangeBottom > rangeBottom &&
		    prevRangeTop    > rangeBottom) {
//		    paint->setPen(midColour);
		    paint->setPen(m_colour);
		    paint->drawLine(x-1, prevRangeTop, x, rangeBottom);
		    paint->setPen(prevRangeTopColour);
		    paint->drawPoint(x-1, prevRangeTop);
		} else if (prevRangeBottom < rangeTop &&
			   prevRangeTop    < rangeTop) {
//		    paint->setPen(midColour);
		    paint->setPen(m_colour);
		    paint->drawLine(x-1, prevRangeBottom, x, rangeTop);
		    paint->setPen(prevRangeBottomColour);
		    paint->drawPoint(x-1, prevRangeBottom);
		}
	    }

	    if (ready) {
		if (clipped ||
		    range.min * m_gain <= -1.0 ||
		    range.max * m_gain >=  1.0) {
		    paint->setPen(Qt::red);
		} else {
		    paint->setPen(m_colour);
		}
	    } else {
		paint->setPen(midColour);
	    }

	    paint->drawLine(x, rangeBottom, x, rangeTop);

	    prevRangeTopColour = m_colour;
	    prevRangeBottomColour = m_colour;

	    if (m_greyscale && (m_scale == LinearScale) && ready) {
		if (!clipped) {
		    if (rangeTop < rangeBottom) {
			if (topFill > 0 &&
			    (!drawMean || (rangeTop < meanTop - 1))) {
			    paint->setPen(greys[topFill - 1]);
			    paint->drawPoint(x, rangeTop);
			    prevRangeTopColour = greys[topFill - 1];
			}
			if (bottomFill > 0 && 
			    (!drawMean || (rangeBottom > meanBottom + 1))) {
			    paint->setPen(greys[bottomFill - 1]);
			    paint->drawPoint(x, rangeBottom);
			    prevRangeBottomColour = greys[bottomFill - 1];
			}
		    }
		}
	    }
        
	    if (drawMean) {
		paint->setPen(midColour);
		paint->drawLine(x, meanBottom, x, meanTop);
	    }
        
	    prevRangeBottom = rangeBottom;
	    prevRangeTop = rangeTop;
	}
    }

    if (m_aggressive) {

	if (ready && rect == m_view->rect()) {
	    m_cacheValid = true;
	    m_cacheZoomLevel = zoomLevel;
	}
	paint->end();
	delete paint;
	viewPainter.drawPixmap(rect, *m_cache, rect);
    }
}

QString
WaveformLayer::getFeatureDescription(QPoint &pos) const
{
    int x = pos.x();

    if (!m_model || !m_model->isOK()) return "";

    long f0 = getFrameForX(x);
    long f1 = getFrameForX(x + 1);

    if (f0 < 0) f0 = 0;
    if (f1 <= f0) return "";
    
    QString text;

    RealTime rt0 = RealTime::frame2RealTime(f0, m_model->getSampleRate());
    RealTime rt1 = RealTime::frame2RealTime(f1, m_model->getSampleRate());

    if (f1 != f0 + 1 && (rt0.sec != rt1.sec || rt0.msec() != rt1.msec())) {
	text += tr("Time:\t%1 - %2")
	    .arg(rt0.toText(true).c_str())
	    .arg(rt1.toText(true).c_str());
    } else {
	text += tr("Time:\t%1")
	    .arg(rt0.toText(true).c_str());
    }

    size_t channels = 0, minChannel = 0, maxChannel = 0;
    bool mergingChannels = false;

    channels = getChannelArrangement(minChannel, maxChannel, mergingChannels);
    if (channels == 0) return "";

    for (size_t ch = minChannel; ch <= maxChannel; ++ch) {

	size_t blockSize = m_view->getZoomLevel();
	RangeSummarisableTimeValueModel::RangeBlock ranges =
	    m_model->getRanges(ch, f0, f1, blockSize);

	if (ranges.empty()) continue;
	
	RangeSummarisableTimeValueModel::Range range = ranges[0];
	
	QString label = tr("Level:");
	if (minChannel != maxChannel) {
	    if (ch == 0) label = tr("Left:");
	    else if (ch == 1) label = tr("Right:");
	    else label = tr("Channel %1").arg(ch + 1);
	}

	int min = int(range.min * 1000);
	int max = int(range.max * 1000);
	int db = int(AudioLevel::multiplier_to_dB(std::max(fabsf(range.min),
							   fabsf(range.max)))
		     * 100);

	if (min != max) {
	    text += tr("\n%1\t%2 - %3 (%4 dB peak)")
		.arg(label).arg(float(min)/1000).arg(float(max)/1000).arg(float(db)/100);
	} else {
	    text += tr("\n%1\t%2 (%3 dB peak)")
		.arg(label).arg(float(min)/1000).arg(float(db)/100);
	}
    }

    return text;
}

int
WaveformLayer::getVerticalScaleWidth(QPainter &paint) const
{
    if (m_scale == LinearScale) {
	return paint.fontMetrics().width("0.0") + 13;
    } else {
	return std::max(paint.fontMetrics().width(tr("0dB")),
			paint.fontMetrics().width(tr("-Inf"))) + 13;
    }
}

void
WaveformLayer::paintVerticalScale(QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) {
	return;
    }

    size_t channels = 0, minChannel = 0, maxChannel = 0;
    bool mergingChannels = false;

    channels = getChannelArrangement(minChannel, maxChannel, mergingChannels);
    if (channels == 0) return;

    int h = rect.height(), w = rect.width();
    int textHeight = paint.fontMetrics().height();
    int toff = -textHeight/2 + paint.fontMetrics().ascent() + 1;

    for (size_t ch = minChannel; ch <= maxChannel; ++ch) {

	int m = (h / channels) / 2;
	int my = m + (((ch - minChannel) * h) / channels);
	int py = -1;

	for (int i = 0; i <= 10; ++i) {

	    int vy = 0;
	    QString text = "";

	    if (m_scale == LinearScale) {

		vy = int((m * i * m_gain) / 10);

		text = QString("%1").arg(float(i) / 10.0);
		if (i == 0) text = "0.0";
		if (i == 10) text = "1.0";

	    } else {

		int db;
		bool minvalue = false;
		
		if (m_scale == MeterScale) {
		    static int dbs[] = { -50, -40, -30, -20, -15,
					 -10, -5, -3, -2, -1, 0 };
		    db = dbs[i];
		    if (db == -50) minvalue = true;
		    vy = AudioLevel::multiplier_to_preview
			(AudioLevel::dB_to_multiplier(db) * m_gain, m);
		} else {
		    db = -100 + i * 10;
		    if (db == -100) minvalue = true;
		    vy = dBscale
			(AudioLevel::dB_to_multiplier(db) * m_gain, m);
		}

		text = QString("%1").arg(db);
		if (db == 0) text = tr("0dB");
		if (minvalue) {
		    text = tr("-Inf");
		    vy = 0;
		}
	    }

	    if (vy < 0) vy = -vy;
	    if (vy >= m - 1) continue;

	    if (py >= 0 && (vy - py) < textHeight - 1) {
		paint.drawLine(w - 4, my - vy, w, my - vy);
		if (vy > 0) paint.drawLine(w - 4, my + vy, w, my + vy);
		continue;
	    }

	    paint.drawLine(w - 7, my - vy, w, my - vy);
	    if (vy > 0) paint.drawLine(w - 7, my + vy, w, my + vy);

	    int tx = 3;
	    if (m_scale != LinearScale) {
		tx = w - 10 - paint.fontMetrics().width(text);
	    }
	    
	    paint.drawText(tx, my - vy + toff, text);
	    if (vy > 0) paint.drawText(tx, my + vy + toff, text);
		
	    py = vy;
	}
    }
}

QString
WaveformLayer::toXmlString(QString indent, QString extraAttributes) const
{
    QString s;
    
    s += QString("gain=\"%1\" "
		 "colour=\"%2\" "
		 "showMeans=\"%3\" "
		 "greyscale=\"%4\" "
		 "channelMode=\"%5\" "
		 "channel=\"%6\" "
		 "scale=\"%7\" "
		 "aggressive=\"%8\"")
	.arg(m_gain)
	.arg(encodeColour(m_colour))
	.arg(m_showMeans)
	.arg(m_greyscale)
	.arg(m_channelMode)
	.arg(m_channel)
	.arg(m_scale)
	.arg(m_aggressive);

    return Layer::toXmlString(indent, extraAttributes + " " + s);
}

void
WaveformLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false;

    float gain = attributes.value("gain").toFloat(&ok);
    if (ok) setGain(gain);

    QString colourSpec = attributes.value("colour");
    if (colourSpec != "") {
	QColor colour(colourSpec);
	if (colour.isValid()) {
	    setBaseColour(QColor(colourSpec));
	}
    }

    bool showMeans = (attributes.value("showMeans") == "1" ||
		      attributes.value("showMeans") == "true");
    setShowMeans(showMeans);

    bool greyscale = (attributes.value("greyscale") == "1" ||
		      attributes.value("greyscale") == "true");
    setUseGreyscale(greyscale);

    ChannelMode channelMode = (ChannelMode)
	attributes.value("channelMode").toInt(&ok);
    if (ok) setChannelMode(channelMode);

    int channel = attributes.value("channel").toInt(&ok);
    if (ok) setChannel(channel);

    Scale scale = (Scale)
	attributes.value("scale").toInt(&ok);
    if (ok) setScale(scale);

    bool aggressive = (attributes.value("aggressive") == "1" ||
		       attributes.value("aggressive") == "true");
    setUseGreyscale(aggressive);
}

#ifdef INCLUDE_MOCFILES
#include "WaveformLayer.moc.cpp"
#endif

