#include "focus_finder.h"
#include "interpolation.h"

#include <opencv2/opencv.hpp>
#include "stdafx.h"
#include <QThread>
#include <math.h>
#include <QApplication>
#include <QMessageBox>
#ifdef VANCOUVER
#include <AerotechMotionhandler.h>
#elif VALENCIA
#include <ACSCMotionHandler.h>
#endif

//add thresholds and indeces as Valencia and Vancouver variables

//from exaple http://www.pyimagesearch.com/2015/09/07/blur-detection-with-opencv/

Focus_finder::Focus_finder(QObject *parent) : QObject(parent)
{}

Focus_finder::~Focus_finder()
{}

void Focus_finder::Set_gantry(MotionHandler *m_gantry){
    gantry = m_gantry;
}

void Focus_finder::Set_log(QTextEdit *m_log){
    log = m_log;
}

void Focus_finder::Set_camera(const cv::VideoCapture &m_cap){
    cap = m_cap;
}

double Focus_finder::eval_stddev(cv::Mat input_image)
{
    cv::Scalar  mean_t;
    cv::Scalar  stddev_t;
    cv::meanStdDev(input_image,mean_t,stddev_t);
    return  stddev_t[0];
}

void Focus_finder::find_focus(double &focus_height)
{
    //Function that return the focus z coordinate
    //https://en.wikipedia.org/wiki/Parabola
    //https://rechneronline.de/function-graphs/
    //http://doc.qt.io/qt-4.8/signalsandslots.html
    qInfo("Auto-focus start");
    double z_step = 0.5;// mm //to be changed according the units of your gantry and shape of focus-height distribution
    if(z_step>1){//to be changed according the units of your gantry and shape of focus-height distribution
        qWarning("Step is too big.");
        return;
    }
    double z_temp = 1.;
    double z_from_outside;
    cv::Mat mat_from_outside;
    if(!gantry->gantryConnected){
        QMessageBox::critical(nullptr, tr("Error"), tr("Gantry not connected!! Can not perform autofocus."));
        return;
    }

    bool Is_Max      = false;
    bool Focus_found = false;
    double direction = 1.;
    double StdDev_t  = 1.1;
    double StdDev_MAX = 1.1;
    double Z_MAX     = 1.;
    int Iterations   = 1.;
    double threshold = 0.02;
    qInfo("Start while loop.");
    while(!Focus_found){
        qInfo("Iteration : %i",Iterations);
        //add user defined condition to tell software if focus is forward or backward wrt current position
        for(int i=0; i<measure_points;i++){
            gantry->moveZBy(direction*z_step,1.);
            if (cap.isOpened()){
                cap.read(mat_from_outside);
                Sleeper::msleep(100);
                cap.read(mat_from_outside);
                //cap >> mat_from_outside;
            } else {
                qWarning("Error : Not able to open camera.");
                return;
            }
            z_from_outside = gantry->whereAmI(1).at(z_pos_index);
            StdDev_t = eval_stddev(mat_from_outside);
            if(StdDev_t > StdDev_MAX){
                StdDev_MAX = StdDev_t;
                Z_MAX = z_from_outside;
            }
            qInfo("i : %i ; z_step : %3.4f ; z : %3.4f ; stddev : %5.5f ;; z max : %3.4f ; stddev_MAX: %5.5f",
                  i,z_step,z_from_outside,StdDev_t,Z_MAX,StdDev_MAX);
            if((0.33 * z_step) <= threshold){
                //this has to be the last iteration
                x[i] = z_from_outside;
                y[i] = StdDev_t;
            }
        }// for 6
        Is_Max = (StdDev_MAX > 45);//adjust this value acording to the performance of your camera
        if(!Is_Max){
            qWarning("Error: No max is found. Try changing search range or step size.");
            return;
        }
        z_step = 0.33 * z_step; //if step start as 0.5 it should reach 0.02 in 3 iterations
        Iterations++;
        if(z_step <= threshold)
            Focus_found = true;
        z_temp = Z_MAX;//destination
        if(!Focus_found)
            z_temp = z_temp - z_step*4; //4 steps correction, only if focus is not found, otherwise move to focus position
        z_temp = z_temp - gantry->whereAmI(1).at(z_pos_index); // relative distance from current position
        gantry->moveZBy(z_temp,1.);
    }//while (!focus not found)
    qInfo("Focus: max z : %3.4f ;  stddev_MAX : %5.5f ",Z_MAX,StdDev_MAX);
    double Z_out = 0.;
    perform_fit(Z_out);
    qInfo("Focus fit z : %3.4f",Z_out);
    focus_height = Z_out;
    z_temp = Z_out - gantry->whereAmI(1).at(z_pos_index); // relative distance from current position
    gantry->moveZBy(z_temp,1.);
}

void Focus_finder::perform_fit(double &z_output)
{
    //WARNING: these array need to be of size == measure_points
    alglib::real_1d_array al_x = "[0,0,0,0,0,0]";
    alglib::real_1d_array al_y = "[0,0,0,0,0,0]";
    for(int i=0;i<measure_points;i++){
        al_x[i] = x[i];
        al_y[i] = y[i];
    }

    alglib::ae_int_t m = 3;
    alglib::ae_int_t info;
    alglib::barycentricinterpolant p;
    alglib::polynomialfitreport rep;

    // NOTE: result is returned as barycentricinterpolant structure.
    //       if you want to get representation in the power basis,
    //       you can use barycentricbar2pow() function to convert
    //       from barycentric to power representation (see docs for
    //       POLINT subpackage for more info).
    //
    alglib::polynomialfit(al_x, al_y, m, info, p, rep);

    alglib::real_1d_array a2;
    alglib::polynomialbar2pow(p, a2);
    log->append("Coefficients p_1 = a[0]: "+QString::number(a2[0])+" ;a[1]: "+QString::number(a2[1])+" ; a[2]: "+QString::number(a2[2]));
    log->append("Vertex : ( "+QString::number(EvalVertex_x(a2[2],a2[1],a2[0]))+" , "+QString::number(EvalVertex_y(a2[2],a2[1],a2[0]))+" )");
    z_output = EvalVertex_x(a2[2],a2[1],a2[0]);
}

double Focus_finder::EvalVertex_x(double a,double b, double c){
    //y = ax^2 + bx + c
    return -b/(2*a);
}

double Focus_finder::EvalVertex_y(double a,double b, double c){
    //y = ax^2 + bx + c
    double num =0.;
    num = 4*a*c - b*b;
    double den =0.;
    den = 4*a;
    return num/den;
}

void Focus_finder::Eval_syst_scan(){
    int numb_steps = 20;
    double z_temp = gantry->whereAmI(1).at(z_pos_index);
    double z_step = 0.2;// to be changed according the units of your gantry and shape of focus-heught distribution
    qInfo("Performing systematic scan near the focus position : %5.5f",z_temp);
    gantry->moveZBy(-z_step*numb_steps*0.6,1.);
    cv::Mat mat_from_outside;
    for(int i=0; i<numb_steps;i++){
        gantry->moveZBy(z_step,1.);//1 mm/s
        if (cap.isOpened()){
            cap.read(mat_from_outside);
            Sleeper::msleep(100);
            cap.read(mat_from_outside);
            //cap >> mat_from_outside;
        } else {
            log->append("Error : Not able to open camera.");
            return;
        }
        //SetImage(mat_from_outside);
        int center_rows = mat_from_outside.rows/2.0; //Defining the center of the image
        int center_cols = mat_from_outside.cols/2.0;
        cv::Rect regione_interessante(center_cols-(window_size*0.5),center_rows-(window_size*0.5),window_size,window_size); //Rectangle that will be the RegionOfInterest (ROI)
        cv::Mat RoiImage = mat_from_outside(regione_interessante);
        double StdDev_t = eval_stddev(RoiImage);
        cv::imshow("image ROI",RoiImage);
        qInfo("i : %i ; z : %5.5f ; Std. dev. : %5.5f",i,gantry->whereAmI(1).at(z_pos_index),StdDev_t);
    }
}

void Focus_finder::Eval_syst_time(){
    double z_temp = gantry->whereAmI().at(z_pos_index);
    log->append("Performing systematic scan in time (2 sec intervals) at position : "
                    +QString::number(z_temp));
    cv::Mat mat_from_outside;
    for(int i=0; i<7;i++){
        Sleeper::sleep(2);
        if (cap.isOpened()){
            cap >> mat_from_outside;
        } else {
            log->append("Error : Not able to open camera.");
            return;
        }

        double StdDev_t = eval_stddev(mat_from_outside);
        log->append("i : "+QString::number(i)+
                    " ; z : "+QString::number(gantry->whereAmI().at(z_pos_index))+
                    " ; std dev : "+QString::number(StdDev_t));
    }
}

void::Focus_finder::Eval_syst_moving(){
    int numb_steps = 10;
    cv::Mat mat_from_outside;
    double z_temp = gantry->whereAmI().at(z_pos_index);
    log->append("Performing systematic fwd-bkwd stability scan at position : "
                    +QString::number(z_temp));
    double z_step = 1.5; // to be changed according the units of your gantry and shape of focus-height distribution
    for(int i=0; i<numb_steps;i++){
        gantry->moveZBy(-z_step,1.);
        Sleeper::sleep(1);
        if (cap.isOpened()){
            cap >> mat_from_outside;
        } else {
            log->append("Error : Not able to open camera.");
            return;
        }
        double StdDev_t = eval_stddev(mat_from_outside);
        log->append("<< i : "+QString::number(i)+
                    " ; z : "+QString::number(gantry->whereAmI().at(z_pos_index))+
                    " ; std dev : "+QString::number(StdDev_t));
        gantry->moveZBy(z_step,1.);
        Sleeper::sleep(1);
        if (cap.isOpened()){
            cap >> mat_from_outside;
        } else {
            log->append("Error : Not able to open camera.");
            return;
        }
        StdDev_t = eval_stddev(mat_from_outside);
        log->append(">>>> i : "+QString::number(i)+
                    " ; z : "+QString::number(gantry->whereAmI().at(z_pos_index))+
                    " ; std dev : "+QString::number(StdDev_t));
    }
}
