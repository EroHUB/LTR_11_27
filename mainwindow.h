#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qcustomplot.h"

#define ACQ_BLOCK_SIZE (50)
#define GRAPH_SIZE (6000000)
#define NSAMPLES (2*LTR27_MEZZANINE_NUMBER*1024)
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


    void setupDataTimer();

private slots:
    void on_pushButton_startLtr11_clicked();
    void on_pushButton_meassure_clicked();
    void on_pushButton_ltr27_clicked();

    void on_termistr_2_clicked();

    void on_pushButton_clicked();

public:
    Ui::MainWindow *ui;
    QCustomPlot *customplot;    // Объявляем графическое полотно
    QCPGraph *graphic;
    QTimer dataTimer;
        // Объявляем график

};

class waiter : public QObject{
    Q_OBJECT
public:
    void Waiting();

signals:
    void finished(bool);
};

class Drawing : public QObject{
    Q_OBJECT
public:
    void draw(Ui::MainWindow *ui);
    void draw_ltr27(Ui::MainWindow *ui);    // Добавляем объявление функции draw


signals:
    void finished(bool);

};



class CollectData : public QObject { // класс сбора данных
    Q_OBJECT
public slots:
    void Collect();
signals:
    void finished(bool);
};



class Collector : public QObject { //класс сборщика данных
    Q_OBJECT
    CollectData* col_data;
public slots:
    void collect();

signals:
    void finished(bool);
};

class CollectData_ltr27 : public QObject { // класс сбора данных
    Q_OBJECT
public slots:
    void Collect_from_thermistor();
signals:
    void finished(bool);
};



class Collector_ltr27 : public QObject { //класс сборщика данных
    Q_OBJECT
    CollectData_ltr27* col_data;
public slots:
    void collect();

signals:
    void finished(bool);
};




#endif // MAINWINDOW_H
