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

    data->admin = mapper_admin_new(0, 0, 0);  // TODO: allow choosing interface
    data->device = mdev_new("implicitmap", 0, data->admin);
    data->monitor = mapper_monitor_new(data->admin, 0);
    data->db = mapper_monitor_get_db(data->monitor);
    mapper_db_add_link_callback(data->db, linkHandler, data);
    mapper_db_add_connection_callback(data->db, connectHandler, data);

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

    QPalette* palette = new QPalette();
    QLinearGradient linearGradient(QPointF(250, 0), QPointF(250, 400));
    linearGradient.setColorAt(0, QColor(125, 168, 117));
    linearGradient.setColorAt(1, QColor(55, 60, 54));
    palette->setBrush(QPalette::Window,*(new QBrush(linearGradient)));
    ui->centralWidget->setPalette(*palette);
    ui->centralWidget->show();

    srcDisplay = new Display(this, &data->input);
    dstDisplay = new Display(this, &data->output);

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
            mdev_free(data->device);
        if (data->db) {
            mapper_db_remove_connection_callback(data->db, connectHandler, data);
            mapper_db_remove_link_callback(data->db, linkHandler, data);
        }
        if (data->monitor)
            mapper_monitor_free(data->monitor);
        if (data->admin)
            mapper_admin_free(data->admin);
        clearSnapshots(data);
        free(data);
    }
}

void MainWindow::poll()
{
    mdev_poll(data->device, 0);
    mapper_monitor_poll(data->monitor, 0);
    if (!data->ready) {
        if (mdev_ready(data->device)) {
            data->ready = true;

            // create a new generic output signal
            mdev_add_output(data->device, "/CONNECT_HERE", 1, 'f', 0, 0, 0);

            // create a new generic input signal
            mdev_add_input(data->device, "/CONNECT_HERE", 1, 'f', 0, 0, 0, 0, data);

            // subscribe to ourself
            mapper_monitor_subscribe(data->monitor, mdev_name(data->device),
                                     SUB_DEVICE_LINKS | SUB_DEVICE_CONNECTIONS,
                                     -1);
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
        str.setNum(mdev_num_inputs(data->device) - 1);
        ui->srcLabel->setText(str + " source parameters");
        str.setNum(mdev_num_outputs(data->device) - 1);
        ui->destLabel->setText(str + " destination parameters");
    }
}

void MainWindow::on_clearButton_clicked()
{
    // TODO: clear snapshots in implicit mapping engine
    clearSnapshots(data);
}

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
    int i, j;
    float rand_val;
    mapper_db_signal props;
    if (data->ready) {
        mdev_now(data->device, &data->tt);
        mdev_start_queue(data->device, data->tt);
        mapper_signal *psig = mdev_get_outputs(data->device);
        for (i = 1; i < mdev_num_outputs(data->device); i ++) {
            props = msig_properties(psig[i]);
            t_signal_ref *ref = (t_signal_ref*)props->user_data;
            if (props->type == 'f') {
                float v[props->length];
                float *min = (float*)props->minimum, *max = (float*)props->maximum;
                for (j = 0; j < props->length; j++) {
                    rand_val = (float)rand() / (float)RAND_MAX;
                    if (min && max) {
                        v[j] = rand_val * (max[j] - min[j]) + min[j];
                    }
                    else {
                        // if ranges have not been declared, assume normalized between 0 and 1
                        v[j] = rand_val;
                    }
                    data->output.value[ref->offset+j] = v[j];
                }
                msig_update(psig[i], v, 1, data->tt);
            }
            else if (props->type == 'i') {
                int v[props->length];
                int *min = (int*)props->minimum, *max = (int*)props->maximum;
                for (j = 0; j < props->length; j++) {
                    rand_val = (float)rand() / (float)RAND_MAX;
                    if (props->minimum && props->maximum) {
                        v[j] = (int) (rand_val * (max[j] - min[j]) + min[j]);
                    }
                    else {
                        // if ranges have not been declared, assume normalized between 0 and 1
                        v[j] = (int) rand_val;
                    }
                    data->output.value[ref->offset+j] = (float)v[j];
                }
                msig_update(psig[i], v, 1, data->tt);
            }
            else if (props->type == 'd') {
                double v[props->length];
                double *min = (double*)props->minimum, *max = (double*)props->maximum;
                for (j = 0; j < props->length; j++) {
                    rand_val = (float)rand() / (float)RAND_MAX;
                    if (props->minimum && props->maximum) {
                        v[j] = rand_val * (max[j] - min[j]) + min[j];
                    }
                    else {
                        // if ranges have not been declared, assume normalized between 0 and 1
                        v[j] = rand_val;
                    }
                    data->output.value[ref->offset+j] = (float)v[j];
                }
                msig_update(psig[i], v, 1, data->tt);
            }
        }
        mdev_send_queue(data->device, data->tt);
        data->newOut = 1;
    }
}

void MainWindow::on_snapshotButton_clicked()
{
    ;
}
