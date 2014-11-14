#ifndef DISPLAY_H
#define DISPLAY_H

#include <QWidget>
//#include "mainwindow.h"
#include "logic.h"

class Display;

class Display : public QWidget
{
    Q_OBJECT

public:
    Display(QWidget *parent, t_ioValue *data);

public slots:
    void animate();

protected:
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;

private:
    t_ioValue *dataref;
};

#endif // DISPLAY_H
