/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

/*
    This is a modified version of a source file from the KDE
    libraries.  Copyright (c) 1998-2004 Jörg Habenicht, Richard J
    Moore, Chris Cannam and others, distributed under the GNU Lesser
    General Public License.

    Ported to Qt4 by Chris Cannam.
*/


#include "LEDButton.h"

#include <QPainter>
#include <QImage>
#include <QColor>


class LEDButton::LEDButtonPrivate
{
    friend class LEDButton;

    int dark_factor;
    QColor offcolor;
    QPixmap *off_map;
    QPixmap *on_map;
};


LEDButton::LEDButton(QWidget *parent) :
    QWidget(parent),
    led_state(On)
{
    QColor col(Qt::green);
    d = new LEDButton::LEDButtonPrivate;
    d->dark_factor = 300;
    d->offcolor = col.dark(300);
    d->off_map = 0;
    d->on_map = 0;
    
    setColor(col);
}


LEDButton::LEDButton(const QColor& col, QWidget *parent) :
    QWidget(parent),
    led_state(On)
{
    d = new LEDButton::LEDButtonPrivate;
    d->dark_factor = 300;
    d->offcolor = col.dark(300);
    d->off_map = 0;
    d->on_map = 0;

    setColor(col);
}

LEDButton::LEDButton(const QColor& col, LEDButton::State state, QWidget *parent) :
    QWidget(parent),
    led_state(state)
{
    d = new LEDButton::LEDButtonPrivate;
    d->dark_factor = 300;
    d->offcolor = col.dark(300);
    d->off_map = 0;
    d->on_map = 0;

    setColor(col);
}

LEDButton::~LEDButton()
{
    delete d->off_map;
    delete d->on_map;
    delete d;
}

void
LEDButton::paintEvent(QPaintEvent *)
{
    QPainter paint;
    QColor color;
    QBrush brush;
    QPen pen;
		
    // First of all we want to know what area should be updated
    // Initialize coordinates, width, and height of the LED
    int	width = this->width();

    // Make sure the LED is round!
    if (width > this->height())
	width = this->height();
    width -= 2; // leave one pixel border
    if (width < 0) 
	width = 0;

    // maybe we could stop HERE, if width <=0 ?

    int scale = 1;
    QPixmap *tmpMap = 0;
    bool smooth = true;

    if (smooth) {
	if (led_state) {
	    if (d->on_map) {
		paint.begin(this);
		paint.drawPixmap(0, 0, *d->on_map);
		paint.end();
		return;
	    }
	} else {
	    if (d->off_map) {
		paint.begin(this);
		paint.drawPixmap(0, 0, *d->off_map);
		paint.end();
		return;
	    }
	}

	scale = 3;
	width *= scale;

	tmpMap = new QPixmap(width, width);
	tmpMap->fill(palette().background().color());
	paint.begin(tmpMap);

    } else {
	paint.begin(this);
    }

    paint.setRenderHint(QPainter::Antialiasing, false);

    // Set the color of the LED according to given parameters
    color = (led_state) ? led_color : d->offcolor;

    // Set the brush to SolidPattern, this fills the entire area
    // of the ellipse which is drawn first
    brush.setStyle(Qt::SolidPattern);
    brush.setColor(color);
    paint.setBrush(brush);

    // Draws a "flat" LED with the given color:
    paint.drawEllipse( scale, scale, width - scale*2, width - scale*2 );

    // Draw the bright light spot of the LED now, using modified "old"
    // painter routine taken from KDEUI´s LEDButton widget:

    // Setting the new width of the pen is essential to avoid "pixelized"
    // shadow like it can be observed with the old LED code
    pen.setWidth( 2 * scale );

    // shrink the light on the LED to a size about 2/3 of the complete LED
    int pos = width/5 + 1;
    int light_width = width;
    light_width *= 2;
    light_width /= 3;
	
    // Calculate the LED´s "light factor":
    int light_quote = (130*2/(light_width?light_width:1))+100;

    // Now draw the bright spot on the LED:
    while (light_width) {
	color = color.light( light_quote );                      // make color lighter
	pen.setColor( color );                                   // set color as pen color
	paint.setPen( pen );                                     // select the pen for drawing
	paint.drawEllipse( pos, pos, light_width, light_width ); // draw the ellipse (circle)
	light_width--;
	if (!light_width)
	    break;
	paint.drawEllipse( pos, pos, light_width, light_width );
	light_width--;
	if (!light_width)
	    break;
	paint.drawEllipse( pos, pos, light_width, light_width );
	pos++; light_width--;
    }

    // Drawing of bright spot finished, now draw a thin border
    // around the LED which resembles a shadow with light coming
    // from the upper left.

    pen.setWidth( 2 * scale + 1 ); // ### shouldn't this value be smaller for smaller LEDs?
    brush.setStyle(Qt::NoBrush);
    paint.setBrush(brush); // This avoids filling of the ellipse

    // Set the initial color value to colorGroup().light() (bright) and start
    // drawing the shadow border at 45° (45*16 = 720).

    int angle = -720;
    color = palette().light().color();
    
    for (int arc = 120; arc < 2880; arc += 240) {
	pen.setColor(color);
	paint.setPen(pen);
	int w = width - pen.width()/2 - scale + 1;
	paint.drawArc(pen.width()/2, pen.width()/2, w, w, angle + arc, 240);
	paint.drawArc(pen.width()/2, pen.width()/2, w, w, angle - arc, 240);
	color = color.dark(110); //FIXME: this should somehow use the contrast value
    }	// end for ( angle = 720; angle < 6480; angle += 160 )

    paint.end();
    //
    // painting done

    if (smooth) {
	QPixmap *&dest = led_state ? d->on_map : d->off_map;
	QImage i = tmpMap->toImage();
	width /= 3;
	i = i.scaled(width, width, 
		     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	delete tmpMap;
	dest = new QPixmap(QPixmap::fromImage(i));
	paint.begin(this);
	paint.drawPixmap(0, 0, *dest);
	paint.end();
    }
}

LEDButton::State
LEDButton::state() const
{
    return led_state;
}

QColor
LEDButton::color() const
{
    return led_color;
}

void
LEDButton::setState( State state )
{
    if (led_state != state)
    {
	led_state = state;
	update();
    }
}

void
LEDButton::toggleState()
{
    led_state = (led_state == On) ? Off : On;
    // setColor(led_color);
    update();
}

void
LEDButton::setColor(const QColor& col)
{
    if(led_color!=col) {
	led_color = col;
	d->offcolor = col.dark(d->dark_factor);
	delete d->on_map;
	d->on_map = 0;
	delete d->off_map;
	d->off_map = 0;
	update();
    }
}

void
LEDButton::setDarkFactor(int darkfactor)
{
    if (d->dark_factor != darkfactor) {
	d->dark_factor = darkfactor;
	d->offcolor = led_color.dark(darkfactor);
	update();
    }
}

int
LEDButton::darkFactor() const
{
    return d->dark_factor;
}

void
LEDButton::toggle()
{
    toggleState();
}

void
LEDButton::on()
{
    setState(On);
}

void
LEDButton::off()
{
    setState(Off);
}

QSize
LEDButton::sizeHint() const
{
    return QSize(16, 16);
}

QSize
LEDButton::minimumSizeHint() const
{
    return QSize(16, 16 );
}

