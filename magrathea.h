#ifndef MAGRATHEA_H
#define MAGRATHEA_H

#include <QWidget>
#include <QTextEdit>

namespace Ui {
class Magrathea;
}

class QCamera;
class QCameraViewfinder;
class QCameraImageCapture;
class QVBoxLayout;
class MotionHandler;

class Magrathea : public QWidget
{
    Q_OBJECT

public:
    explicit Magrathea(QWidget *parent = 0);
    ~Magrathea();

    static QTextEdit *outputLogTextEdit;

private slots:

    //camera
    void enableCameraBoxClicked(bool checked);
    void focusButtonClicked();
    void captureButtonClicked();

    //gantry
    void connectGantryBoxClicked(bool checked);
    void enableAxesBoxClicked(bool checked);

    //joystick
    void enableJoystickFreeRun(bool checked);
    void freeRun();
    void enableJoystickStepMotion(bool checked);
    void stepMotion();

    //autorepeat
    void xAxisStepContinousBoxClicked(bool checked);
    void yAxisStepContinousBoxClicked(bool checked);
    void zAxisStepContinousBoxClicked(bool checked);
    void uAxisStepContinousBoxClicked(bool checked);

private:
    Ui::Magrathea *ui;

    QCamera *mCamera;
    QCameraViewfinder *mCameraViewfinder;
    QCameraImageCapture *mCameraImageCapture;
    QVBoxLayout *mCameraLayout;
    MotionHandler *mMotionHandler;

    int autoRepeatDelay;
    int autoRepeatInterval;
};

#endif // MAGRATHEA_H