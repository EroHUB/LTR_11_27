#ifndef COLLECTOR_H
#define COLLECTOR_H

#include <QObject>

class CollectData; // Предварительное объявление класса CollectData

class Collector : public QObject
{
    Q_OBJECT

public:
    explicit Collector(QObject *parent = nullptr); // Конструктор

public slots:
    void collect(); // Слот для запуска сбора данных

signals:
    void finished(bool); // Сигнал завершения

private:
    CollectData* col_data; // Указатель на объект CollectData
};

#endif // COLLECTOR_H
