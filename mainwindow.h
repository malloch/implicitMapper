#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "logic.h"
#include "display.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void poll();
//    void on_clearButton_clicked();
    void on_muteButton_clicked();
    void on_processButton_clicked();
    void on_randomizeButton_clicked();
    void on_snapshotButton_clicked();

private:
    Ui::MainWindow *ui;
    implicitMapperData data;
    Display *srcDisplay;
    Display *dstDisplay;
};

#endif // MAINWINDOW_H
