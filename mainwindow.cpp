#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "display.h"
#include "logic.h"
#include <QGridLayout>
#include <QLabel>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    data = (implicitMapperData) malloc(sizeof(struct _implicitMapperData));

    data->device = mapper_device_new("implicitmap", 0, 0);
    mapper_device_set_user_data(data->device, data);
    mapper_device_set_map_callback(data->device, mapHandler);

    data->ready = false;
    data->mute = false;
    data->newIn = false;
    data->queryCount = 0;
    data->numSnapshots = 0;
    data->snapshots = 0;
    data->updateLabels = false;

    initIO(&data->input, data);
    initIO(&data->output, data);

    setWindowTitle(tr("Implicit Mapper"));

    // style the buttons
    QString style("color: white;\
                  background-color: rgba(0, 0, 0, 120);\
                  border-width: 6px;\
                  border-radius: 10px;\
                  border-color: rgba(125, 168, 117, 255);\
                  font: 18px;\
                  padding: 6px;");
    ui->randomizeButton->setStyleSheet(style);
    ui->snapshotButton->setStyleSheet(style);
    ui->processButton->setStyleSheet(style);
    ui->muteButton->setStyleSheet(style);

    QPalette* palette = new QPalette();
    QLinearGradient linearGradient(QPointF(250, 0), QPointF(250, 400));
    linearGradient.setColorAt(0, QColor(125, 168, 117));
    linearGradient.setColorAt(1, QColor(55, 60, 54));
    palette->setBrush(QPalette::Window,*(new QBrush(linearGradient)));
    ui->centralWidget->setPalette(*palette);
    ui->centralWidget->show();

    srcDisplay = new Display(this, data, 1);
    dstDisplay = new Display(this, data, 0);

    ui->gridLayout->addWidget(srcDisplay, 3, 0, 1, 2, 0);
    ui->gridLayout->addWidget(dstDisplay, 3, 2, 1, 2, 0);

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(poll()));
    timer->start(50);
}

MainWindow::~MainWindow()
{
    delete ui;

    if (data) {
        if (data->device)
            mapper_device_free(data->device);
        clearSnapshots(data);
        free(data);
    }
}

void MainWindow::poll()
{
    mapper_device_poll(data->device, 0);
    if (!data->ready) {
        if (mapper_device_ready(data->device)) {
            data->ready = true;

            // create a new generic output signal
            data->dummy_output = mapper_device_add_output_signal(data->device,
                                                                 "CONNECT_TO_DESTINATION",
                                                                 1, 'f', 0, 0, 0);

            // create a new generic input signal
            data->dummy_input = mapper_device_add_input_signal(data->device,
                                                               "CONNECT_TO_SOURCE",
                                                               1, 'f', 0, 0, 0, 0, 0);
        }
    }
    if (data->newIn) {
        // update display
        srcDisplay->animate();
        data->newIn = false;
    }
    if (data->newOut) {
        // update display
        dstDisplay->animate();
        data->newOut = false;
    }
    if (data->updateLabels) {
        QString str;
        str.setNum(mapper_device_num_signals(data->device, MAPPER_DIR_INCOMING) - 1);
        ui->srcLabel->setText(str + " source parameters");
        str.setNum(mapper_device_num_signals(data->device, MAPPER_DIR_OUTGOING) - 1);
        ui->destLabel->setText(str + " destination parameters");
    }
    if (data->queryCount) {
        // check if too much time has elapsed
        mapper_timetag_t tt;
        mapper_timetag_now(&tt);
        if (mapper_timetag_difference(tt, data->tt) > 1.)
            queryTimeout(data);
    }
}

//void MainWindow::on_clearButton_clicked()
//{
//    // TODO: clear snapshots in implicit mapping engine
//    clearSnapshots(data);
//}

void MainWindow::on_muteButton_clicked()
{
    if (data->mute)
        data->mute = false;
    else
        data->mute = true;
    // TODO: adjust state of button
}

void MainWindow::on_processButton_clicked()
{
    // TODO: call "process" method of implicit mapping algorithm
    ;
}

void MainWindow::on_randomizeButton_clicked()
{
    if (data->ready)
        randomizeDest(data);
}

void MainWindow::on_snapshotButton_clicked()
{
    if (data->ready && data->input.size && data->output.size)
        takeSnapshot(data);
}
