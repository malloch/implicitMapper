#include "display.h"

#include <QPainter>

Display::Display(QWidget *parent, t_ioValue *data)
    : QWidget(parent)
{
    dataref = data;
}

void Display::animate()
{

    update();
}

void Display::paintEvent(QPaintEvent *event)
{
    if (!dataref || !dataref->size)
        return;

    float height = this->height();
    float width = this->width() / dataref->size;
    QPainter painter;
    painter.begin(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // paint
    painter.setPen(QColor(0, 0, 0, 0));
    painter.setBrush(QColor(0, 0, 0, 100));
    for (int i = 0; i < dataref->size; i++) {
        // calculate normalized value
        float value = ((dataref->value[i] - dataref->minima[i])
                       * dataref->multiplier[i] * height);
        painter.drawRect(i * width, height - value, width, value);
    }
    painter.end();
}
