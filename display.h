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
    Display(QWidget *parent, implicitMapperData data, int isSrc);

public slots:
    void animate();

protected:
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;

private:
    implicitMapperData data;
    int isSource;
};

#endif // DISPLAY_H
