/************************************************************************/
/* qt-opencv-multithreaded:                                             */
/* A multithreaded OpenCV application using the Qt framework.           */
/*                                                                      */
/* MainWindow.cpp                                                       */
/*                                                                      */
/* Nick D'Ademo <nickdademo@gmail.com>                                  */
/*                                                                      */
/* Copyright (c) 2011 Nick D'Ademo                                      */
/*                                                                      */
/* Permission is hereby granted, free of charge, to any person          */
/* obtaining a copy of this software and associated documentation       */
/* files (the "Software"), to deal in the Software without restriction, */
/* including without limitation the rights to use, copy, modify, merge, */
/* publish, distribute, sublicense, and/or sell copies of the Software, */
/* and to permit persons to whom the Software is furnished to do so,    */
/* subject to the following conditions:                                 */
/*                                                                      */
/* The above copyright notice and this permission notice shall be       */
/* included in all copies or substantial portions of the Software.      */
/*                                                                      */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,      */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF   */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                */
/* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS  */
/* BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN   */
/* ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN    */
/* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE     */
/* SOFTWARE.                                                            */
/*                                                                      */
/************************************************************************/

#include "CameraConnectDialog.h"
#include "ProcessingSettingsDialog.h"
#include "Controller.h"
#include "MainWindow.h"

// Qt header files
#include <QDebug>

// Header file containing default values
#include "DefaultValues.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{  
    // Setup user interface
    setupUi(this);
    // Initially set controller to NULL
    controller=NULL;
    // Create processingSettingsDialog
    processingSettingsDialog = new ProcessingSettingsDialog(this);
    // Initialize ProcessingFlags structure
    processingFlags.grayscaleOn=false;
    processingFlags.smoothOn=false;
    processingFlags.dilateOn=false;
    processingFlags.erodeOn=false;
    processingFlags.flipOn=false;
    processingFlags.cannyOn=false;
    // Save application version in QString variable
    appVersion=QUOTE(APP_VERSION);
    // Connect GUI signals to slots
    connect(connectToCameraAction, SIGNAL(triggered()), this, SLOT(connectToCamera()));
    connect(disconnectCameraAction, SIGNAL(triggered()), this, SLOT(disconnectCamera()));
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));
    connect(grayscaleAction, SIGNAL(toggled(bool)), this, SLOT(setGrayscale(bool)));
    connect(smoothAction, SIGNAL(toggled(bool)), this, SLOT(setSmooth(bool)));
    connect(dilateAction, SIGNAL(toggled(bool)), this, SLOT(setDilate(bool)));
    connect(erodeAction, SIGNAL(toggled(bool)), this, SLOT(setErode(bool)));
    connect(flipAction, SIGNAL(toggled(bool)), this, SLOT(setFlip(bool)));
    connect(cannyAction, SIGNAL(toggled(bool)), this, SLOT(setCanny(bool)));
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(setProcessingSettings()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));
    connect(clearImageBufferButton, SIGNAL(released()), this, SLOT(clearImageBuffer()));
    // Enable/disable appropriate menu items
    connectToCameraAction->setDisabled(false);
    disconnectCameraAction->setDisabled(true);
    processingMenu->setDisabled(true);
    // Set GUI in main window
    grayscaleAction->setChecked(false);
    smoothAction->setChecked(false);
    dilateAction->setChecked(false);
    erodeAction->setChecked(false);
    flipAction->setChecked(false);
    cannyAction->setChecked(false);
    frameLabel->setText("No camera connected.");
    imageBufferBar->setValue(0);
    imageBufferLabel->setText("[000/000]");
    captureRateLabel->setText("");
    processingRateLabel->setText("");
    deviceNumberLabel->setText("");
    cameraResolutionLabel->setText("");
    roiLabel->setText("");
    mouseCursorPosLabel->setText("");
    clearImageBufferButton->setDisabled(true);
} // MainWindow constructor

MainWindow::~MainWindow()
{
    // Check if controller exists
    if(controller!=NULL)
    {
        // Disconnect queued connection
        disconnect(controller->processingThread,SIGNAL(newFrame(QImage)),this,SLOT(updateFrame(QImage)));
        // Stop processing thread
        if(controller->processingThread->isActive)
            controller->stopProcessingThread();
        // Stop capture thread
        if(controller->captureThread->isActive)
            controller->stopCaptureThread();
        // Clear image buffer
        controller->clearImageBuffer();
        // Check if threads have stopped
        if((controller->captureThread->isFinished())&&(controller->processingThread->isFinished()))
        {
            // Disconnect camera if connected
            if(controller->captureThread->capture!=NULL)
                controller->disconnectCamera();
            // Delete processing and capturethreads
            controller->deleteProcessingThread();
            controller->deleteCaptureThread();
        }
        // Delete controller
        delete controller;
        controller=NULL;
    }
} // MainWindow deconstructor

void MainWindow::connectToCamera()
{
    // Create dialog
    cameraConnectDialog = new CameraConnectDialog(this);
    // Prompt user
    // If user presses OK button on dialog, tell controller to connect to camera; else do nothing
    if(cameraConnectDialog->exec()==1)
    {
        // Store image buffer size in local variable
        imageBufferSize=cameraConnectDialog->getImageBufferSize();
        // Create controller
        controller = new Controller(cameraConnectDialog->getDeviceNumber(),imageBufferSize);
        // If camera was successfully connected
        if(controller->captureThread->capture!=NULL)
        {
            // Create queued connection between processing thread (emitter) and GUI thread (receiver/listener)
            connect(controller->processingThread,SIGNAL(newFrame(QImage)),this,SLOT(updateFrame(QImage)),Qt::QueuedConnection);
            // Setup imageBufferBar in main window with minimum and maximum values
            imageBufferBar->setMinimum(0);
            imageBufferBar->setMaximum(imageBufferSize);
            // Enable/disable appropriate menu items
            connectToCameraAction->setDisabled(true);
            disconnectCameraAction->setDisabled(false);
            processingMenu->setDisabled(false);
            // Enable "Clear Image Buffer" push button in main window
            clearImageBufferButton->setDisabled(false);
            // Get input stream properties
            sourceWidth=controller->getInputSourceWidth();
            sourceHeight=controller->getInputSourceHeight();
            // Set text in labels in main window
            deviceNumberLabel->setNum(cameraConnectDialog->getDeviceNumber());
            cameraResolutionLabel->setText(QString::number(sourceWidth)+QString("x")+QString::number(sourceHeight));
        }
        // Display error dialog if camera connection is unsuccessful
        else
            QMessageBox::warning(this,"ERROR:","Could not connect to camera.");
    }
} // connectToCamera()

void MainWindow::disconnectCamera()
{
    // Check if controller exists
    if(controller!=NULL)
    {
        // Disconnect queued connection
        disconnect(controller->processingThread,SIGNAL(newFrame(QImage)),this,SLOT(updateFrame(QImage)));
        // Stop processing thread
        if(controller->processingThread->isActive)
            controller->stopProcessingThread();
        // Stop capture thread
        if(controller->captureThread->isActive)
            controller->stopCaptureThread();
        // Clear image buffer
        controller->clearImageBuffer();
        // Check if threads have stopped
        if((controller->captureThread->isFinished())&&(controller->processingThread->isFinished()))
        {
            // Disconnect camera if connected
            if(controller->captureThread->capture!=NULL)
                controller->disconnectCamera();
            // Delete processing and capture threads
            controller->deleteProcessingThread();
            controller->deleteCaptureThread();
        }
        // Delete controller
        delete controller;
        controller=NULL;
        // Enable/Disable appropriate menu items
        connectToCameraAction->setDisabled(false);
        disconnectCameraAction->setDisabled(true);
        processingMenu->setDisabled(true);
        // Set GUI in main window
        grayscaleAction->setChecked(false);
        smoothAction->setChecked(false);
        dilateAction->setChecked(false);
        erodeAction->setChecked(false);
        flipAction->setChecked(false);
        cannyAction->setChecked(false);
        frameLabel->setText("No camera connected.");
        imageBufferBar->setValue(0);
        imageBufferLabel->setText("[000/000]");
        captureRateLabel->setText("");
        processingRateLabel->setText("");
        deviceNumberLabel->setText("");
        cameraResolutionLabel->setText("");
        roiLabel->setText("");
        mouseCursorPosLabel->setText("");
        clearImageBufferButton->setDisabled(true);
    }
    // Display error dialog if camera could not be disconnected
    else
        QMessageBox::warning(this,"ERROR:","Could not disconnect camera.");
} // disconnectCamera()

void MainWindow::about()
{
    QMessageBox::information(this,"About",QString("Written by Nick D'Ademo\n\nContact: nickdademo@gmail.com\nWebsite: www.nickdademo.com\n\nVersion: ")+appVersion);
} // about()

void MainWindow::clearImageBuffer()
{
    controller->clearImageBuffer();
} // clearImageBuffer()

void MainWindow::setGrayscale(bool input)
{
    // Not checked
    if(!input)
        processingFlags.grayscaleOn=false;
    // Checked
    else if(input)
        processingFlags.grayscaleOn=true;
} // setGrayscale()

void MainWindow::setSmooth(bool input)
{
    // Not checked
    if(!input)
        processingFlags.smoothOn=false;
    // Checked
    else if(input)
        processingFlags.smoothOn=true;
} // setSmooth()

void MainWindow::setDilate(bool input)
{
    // Not checked
    if(!input)
        processingFlags.dilateOn=false;
    // Checked
    else if(input)
        processingFlags.dilateOn=true;
} // setDilate()

void MainWindow::setErode(bool input)
{
    // Not checked
    if(!input)
        processingFlags.erodeOn=false;
    // Checked
    else if(input)
        processingFlags.erodeOn=true;
} // setErode()

void MainWindow::setFlip(bool input)
{
    // Not checked
    if(!input)
        processingFlags.flipOn=false;
    // Checked
    else if(input)
        processingFlags.flipOn=true;
} // setFlip()

void MainWindow::setCanny(bool input)
{
    // Not checked
    if(!input)
        processingFlags.cannyOn=false;
    // Checked
    else if(input)
        processingFlags.cannyOn=true;
} // setCanny()

void MainWindow::updateFrame(const QImage &frame)
{
    // Reset mouse event flags
    // The following checks ensure that every click/release pair, no matter how fast, is picked up by the processing thread
    if(controller->processingThread->frameLabel.mouseRightPressed&&frameLabel->getRightMouseButtonRelease())
        frameLabel->setRightMouseButtonPress(false);
    if(controller->processingThread->frameLabel.mouseLeftPressed&&frameLabel->getLeftMouseButtonRelease())
        frameLabel->setLeftMouseButtonPress(false);
    // Update mouse event flags
    controller->processingThread->frameLabel.mouseXPos=frameLabel->getMouseXPos();
    controller->processingThread->frameLabel.mouseYPos=frameLabel->getMouseYPos();
    controller->processingThread->frameLabel.mouseLeftPressed=frameLabel->getLeftMouseButtonPress();
    controller->processingThread->frameLabel.mouseRightPressed=frameLabel->getRightMouseButtonPress();
    controller->processingThread->frameLabel.mouseLeftReleased=frameLabel->getLeftMouseButtonRelease();
    controller->processingThread->frameLabel.mouseRightReleased=frameLabel->getRightMouseButtonRelease();
    // Update processing flags
    controller->processingThread->processingFlags.grayscaleOn=processingFlags.grayscaleOn;
    controller->processingThread->processingFlags.smoothOn=processingFlags.smoothOn;
    controller->processingThread->processingFlags.dilateOn=processingFlags.dilateOn;
    controller->processingThread->processingFlags.erodeOn=processingFlags.erodeOn;
    controller->processingThread->processingFlags.flipOn=processingFlags.flipOn;
    controller->processingThread->processingFlags.cannyOn=processingFlags.cannyOn;
    // Update processing settings
    controller->processingThread->processingSettings.smoothType=processingSettingsDialog->processingSettings.smoothType;
    controller->processingThread->processingSettings.smoothParam1=processingSettingsDialog->processingSettings.smoothParam1;
    controller->processingThread->processingSettings.smoothParam2=processingSettingsDialog->processingSettings.smoothParam2;
    controller->processingThread->processingSettings.smoothParam3=processingSettingsDialog->processingSettings.smoothParam3;
    controller->processingThread->processingSettings.smoothParam4=processingSettingsDialog->processingSettings.smoothParam4;
    controller->processingThread->processingSettings.dilateNumberOfIterations=processingSettingsDialog->processingSettings.dilateNumberOfIterations;
    controller->processingThread->processingSettings.erodeNumberOfIterations=processingSettingsDialog->processingSettings.erodeNumberOfIterations;
    controller->processingThread->processingSettings.flipMode=processingSettingsDialog->processingSettings.flipMode;
    controller->processingThread->processingSettings.cannyThreshold1=processingSettingsDialog->processingSettings.cannyThreshold1;
    controller->processingThread->processingSettings.cannyThreshold2=processingSettingsDialog->processingSettings.cannyThreshold2;
    controller->processingThread->processingSettings.cannyApertureSize=processingSettingsDialog->processingSettings.cannyApertureSize;
    // Show statistics
    //// Show [number of images in buffer / image buffer size] in imageBufferLabel in main window
    imageBufferLabel->setText(QString("[")+QString::number(controller->processingThread->currentSizeOfBuffer)+
                              QString("/")+QString::number(imageBufferSize)+QString("]"));
    //// Show percentage of image bufffer full in imageBufferBar in main window
    imageBufferBar->setValue(controller->processingThread->currentSizeOfBuffer);
    //// Show processing rate in captureRateLabel in main window
    captureRateLabel->setNum(controller->captureThread->avgFPS);
    captureRateLabel->setText(captureRateLabel->text()+" fps");
    //// Show processing rate in processingRateLabel in main window
    processingRateLabel->setNum(controller->processingThread->avgFPS);
    processingRateLabel->setText(processingRateLabel->text()+" fps");
    //// Show ROI information in roiLabel in main window
    roiLabel->setText(QString("(")+QString::number(controller->processingThread->newROI.x)+QString(",")+
                      QString::number(controller->processingThread->newROI.y)+QString(") ")+
                      QString::number(controller->processingThread->newROI.width)+
                      QString("x")+QString::number(controller->processingThread->newROI.height));
    //// Show mouse cursor position in mouseCursorPosLabel in main window
    mouseCursorPosLabel->setText(QString("(")+QString::number(frameLabel->getMouseXPos())+
                                 QString(",")+QString::number(frameLabel->getMouseYPos())+
                                 QString(")"));
    // Display frame in main window
    frameLabel->setPixmap(QPixmap::fromImage(frame));
} // updateFrame()

void MainWindow::setProcessingSettings()
{
    // Prompt user
    // If user presses OK button on dialog, update processing settings
    if(processingSettingsDialog->exec()==1)
        processingSettingsDialog->updateStoredSettingsFromDialog();
    // Else, restore dialog state
    else
       processingSettingsDialog->updateDialogSettingsFromStored();
} // setProcessingSettings()
