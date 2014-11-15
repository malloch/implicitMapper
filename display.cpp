#include "display.h"

#include <QPainter>

Display::Display(QWidget *parent, implicitMapperData ptr, int isSrc)
    : QWidget(parent)
{
    data = ptr;
    isSource = isSrc;
}

void Display::animate()
{
    update();
}

void Display::paintEvent(QPaintEvent *event)
{
    if (!data)
        return;
    t_ioValue *ref = isSource ? &data->input : &data->output;
    if (!ref->size)
        return;

    float height = this->height();
    float width = this->width() / ref->size;
    QPainter painter;
    painter.begin(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // paint value bars
    painter.setPen(QColor(0, 0, 0, 0));
    painter.setBrush(QColor(0, 0, 0, 100));
    for (int i = 0; i < ref->size; i++) {
        // calculate normalized value
        float value = ((ref->value[i] - ref->minima[i])
                       * ref->multiplier[i] * height);
        painter.drawRect(i * width, height - value, width, value);
    }

    // draw snapshots
    int i;
    snapshot s = data->snapshots;
    while (s) {
        float *values = isSource ? s->inputs : s->outputs;
        QPainterPath *path = new QPainterPath();
        float next, last = height - ((values[0] - ref->minima[0])
                                     * ref->multiplier[0] * height);
        path->moveTo(0, last);
        path->lineTo(width * 0.5, last);
        for (i = 1; i < ref->size; i++) {
            next = height - ((values[i] - ref->minima[i])
                             * ref->multiplier[i] * height);
            path->cubicTo(width * i, last, width * i,
                    next, width * (i + 0.5), next);
            last = next;
        }
        path->lineTo(width * i, last);
        // TODO: set color from snapshot id
        painter.setPen(QColor(255, 255, 255));
        painter.strokePath(*path, painter.pen());

        s = s->next;
    }

    painter.end();
}
