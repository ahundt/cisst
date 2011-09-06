/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
  $Id$
  
  Author(s):  Balazs Vagvolgyi
  Created on: 2006 

  (C) Copyright 2006-2007 Johns Hopkins University (JHU), All Rights
  Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty.  The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---

*/

#include "vidSVSSource.h"


/******************************/
/*** CSVSSource class *********/
/******************************/

CSVSSource::CSVSSource() :
    CVideoCaptureSourceBase(),
    NumOfStreams(0),
    Running(false),
    CaptureProc(0),
    CaptureThread(0),
    SVSObj(0),
    SVSImage(0),
    OutputBuffer(0)
{
    DeviceID[0] = DeviceID[1] = -1;
}

CSVSSource::~CSVSSource()
{
    Close();

    if (OutputBuffer) delete [] OutputBuffer;
}

svlVideoCaptureSource::PlatformType CSVSSource::GetPlatformType()
{
    return svlVideoCaptureSource::WinSVS;
}

int CSVSSource::SetStreamCount(unsigned int numofstreams)
{
    // Only stereo streams are supported for the time being
    if (numofstreams < 1 || numofstreams > 2) return SVL_FAIL;

    Close();

    NumOfStreams = numofstreams;

    if (OutputBuffer) delete [] OutputBuffer;
    OutputBuffer = new svlImageBuffer*[NumOfStreams];

    for (unsigned int i = 0; i < NumOfStreams; i ++) {
        OutputBuffer[i] = 0;
    }

    return SVL_OK;
}

int CSVSSource::GetDeviceList(svlVideoCaptureSource::DeviceInfo **deviceinfo)
{
    // Test device availability
    bool available = false;
    if (SVSObj) available = true;
    else {
        svsVideoImages* svs = getVideoObject();
        if (svs) {
            if (svs->Open()) {
                if (svs->SetColor(1, 1) && svs->SetSize(640, 480)) available = true;
                svs->Close();
            }
        }
        closeVideoObject();
    }
    if (!available) {
        deviceinfo[0] = 0;
        return 0;
    }

    // Allocate memory for device info array
    // CALLER HAS TO FREE UP THIS ARRAY!!!
    deviceinfo[0] = new svlVideoCaptureSource::DeviceInfo[2];
    memset(deviceinfo[0], 0, 2 * sizeof(svlVideoCaptureSource::DeviceInfo));

    deviceinfo[0][0].id = 0;
    sprintf(deviceinfo[0][0].name, "Small Vision System - Left");
    deviceinfo[0][0].activeinput = -1;
    deviceinfo[0][0].inputcount = 0;
    deviceinfo[0][0].platform = svlVideoCaptureSource::WinSVS;
    deviceinfo[0][0].testok = true;

    deviceinfo[0][1].id = 1;
    sprintf(deviceinfo[0][1].name, "Small Vision System - Right");
    deviceinfo[0][1].activeinput = -1;
    deviceinfo[0][1].inputcount = 0;
    deviceinfo[0][1].platform = svlVideoCaptureSource::WinSVS;
    deviceinfo[0][1].testok = true;

    return 2;
}

int CSVSSource::Open()
{
    if (NumOfStreams < 1 || NumOfStreams > 2) return SVL_FAIL;

    // Return if already successfully initialized
    if (SVSObj) return SVL_OK;

    Close();

    // Create a video source object, using the loaded framegrabber interface
    SVSObj = getVideoObject();
    if (SVSObj == 0) return SVL_VCS_UNABLE_TO_OPEN;
    // Open the video source
    if (!(SVSObj->Open())) return SVL_VCS_UNABLE_TO_OPEN;
    // Set to color mode
    if (!(SVSObj->SetColor(1, 1))) return SVL_VCS_UNSUPPORTED_COLORSPACE;
    // Set image size
    if (!(SVSObj->SetSize(640, 480))) return SVL_VCS_UNSUPPORTED_SIZE;

    // Setup buffers
    for (unsigned int i = 0; i < NumOfStreams; i ++) {
        OutputBuffer[i] = new svlImageBuffer(640, 480);
    }

    return SVL_OK;
}

void CSVSSource::Close()
{
    if (NumOfStreams < 1 || NumOfStreams > 2) return;

    Stop();

    if (SVSObj) {
        SVSObj->Close();
        closeVideoObject();
        delete SVSObj;
        SVSObj = 0;
    }
    // Release capture buffers
    for (unsigned int i = 0; i < NumOfStreams; i ++) {
        if (OutputBuffer[i]) {
            delete OutputBuffer[i];
            OutputBuffer[i] = 0;
        }
    }
}

int CSVSSource::Start()
{
    if (NumOfStreams < 1 || NumOfStreams > 2) return SVL_FAIL;

    // Return if already running
    if (Running) return SVL_OK;

    if (SVSObj == 0) return SVL_VCS_DEVICE_NOT_INITIALIZED;
    if (!SVSObj->Start()) return SVL_VCS_UNABLE_TO_START_CAPTURE;

    Running = true;

    CaptureProc = new CSVSSourceThread();
    CaptureThread = new osaThread;
    CaptureThread->Create<CSVSSourceThread, CSVSSource*>(CaptureProc,
                                                         &CSVSSourceThread::Proc,
                                                         this);
    if (CaptureProc->WaitForInit() == false) return SVL_FAIL;

    return SVL_OK;
}

svlImageRGB* CSVSSource::GetLatestFrame(bool waitfornew, unsigned int videoch)
{
    if (videoch >= NumOfStreams || DeviceID[videoch] < 0) return 0;
    return OutputBuffer[videoch]->Pull(waitfornew);
}

int CSVSSource::CaptureFrame()
{
    SVSImage = SVSObj->GetImage(5000);
    if (SVSImage) {
        // Decode both channels
        if (DeviceID[SVL_LEFT] == SVL_LEFT) {
            ConvertRGB32toRGB24(reinterpret_cast<unsigned char*>(SVSImage->Color()),
                                OutputBuffer[SVL_LEFT]->GetPushBuffer(),
                                640 * 480);
        }
        if (DeviceID[SVL_LEFT] == SVL_RIGHT) {
            ConvertRGB32toRGB24(reinterpret_cast<unsigned char*>(SVSImage->ColorRight()),
                                OutputBuffer[SVL_LEFT]->GetPushBuffer(),
                                640 * 480);
        }
        if (DeviceID[SVL_RIGHT] == SVL_LEFT) {
            ConvertRGB32toRGB24(reinterpret_cast<unsigned char*>(SVSImage->Color()),
                                OutputBuffer[SVL_RIGHT]->GetPushBuffer(),
                                640 * 480);
        }
        if (DeviceID[SVL_RIGHT] == SVL_RIGHT) {
            ConvertRGB32toRGB24(reinterpret_cast<unsigned char*>(SVSImage->ColorRight()),
                                OutputBuffer[SVL_RIGHT]->GetPushBuffer(),
                                640 * 480);
        }
        for (unsigned int i = 0; i < NumOfStreams; i ++) {
            OutputBuffer[i]->Push();
        }
        return SVL_OK;
    }

    return SVL_FAIL;
}

int CSVSSource::Stop()
{
    if (NumOfStreams < 1 || NumOfStreams > 2) return SVL_FAIL;

    if (Running && SVSObj) {
        Running = false;

        if (CaptureThread) {
            CaptureThread->Wait();
            delete(CaptureThread);
            CaptureThread = 0;
        }
        if (CaptureProc) {
            delete(CaptureProc);
            CaptureProc = 0;
        }

        SVSObj->Stop();

        return SVL_OK;
    }

    return SVL_FAIL;
}

bool CSVSSource::IsRunning()
{
    return Running;
}

int CSVSSource::SetDevice(int devid, int inid, unsigned int videoch)
{
    if (NumOfStreams < 1 || NumOfStreams > 2) return SVL_FAIL;

    if (devid < 0 || devid > 1) return SVL_FAIL;
    if (videoch == SVL_LEFT) {
        // Fails if the same channel is to be selected twice
        if (DeviceID[SVL_RIGHT] == devid) return SVL_FAIL;
        DeviceID[SVL_LEFT] = devid;
        return SVL_OK;
    }
    if (videoch == SVL_RIGHT) {
        // Fails if the same channel is to be selected twice
        if (DeviceID[SVL_LEFT] == devid) return SVL_FAIL;
        DeviceID[SVL_RIGHT] = devid;
        return SVL_OK;
    }
    return SVL_FAIL;
}

int CSVSSource::GetWidth(unsigned int videoch)
{
    if (videoch < NumOfStreams) return 640;
    return -1;
}

int CSVSSource::GetHeight(unsigned int videoch)
{
    if (videoch < NumOfStreams) return 480;
    return -1;
}

int CSVSSource::GetFormatList(unsigned int deviceid, svlVideoCaptureSource::ImageFormat **formatlist)
{
    if (formatlist == 0) return SVL_FAIL;

    formatlist[0] = new svlVideoCaptureSource::ImageFormat[1];
    formatlist[0][0].width = 640;
    formatlist[0][0].height = 480;
    formatlist[0][0].colorspace = svlVideoCaptureSource::PixelRGB8;
    formatlist[0][0].rgb_order = true;
    formatlist[0][0].yuyv_order = false;
    formatlist[0][0].framerate = 25.0;

    return 1;
}

int CSVSSource::GetFormat(svlVideoCaptureSource::ImageFormat& format, unsigned int videoch)
{
    if (videoch >= NumOfStreams) return SVL_FAIL;

    format.width = 640;
    format.height = 480;
    format.colorspace = svlVideoCaptureSource::PixelRGB8;
    format.rgb_order = true;
    format.yuyv_order = false;
    format.framerate = 25.0;

    return SVL_OK;
}

void CSVSSource::ConvertRGB32toRGB24(unsigned char* source, unsigned char* dest, const int pixcount)
{
    unsigned char r, g, b;
    for (int i = 0; i < pixcount; i ++) {
        r = *source;
        source ++;
        g = *source;
        source ++;
        b = *source;
        source += 2;

        *dest = b;
        dest ++;
        *dest = g;
        dest ++;
        *dest = r;
        dest ++;
    }
}


/**************************************/
/*** CSVSSourceThread class ***********/
/**************************************/

void* CSVSSourceThread::Proc(CSVSSource* baseref)
{
    // signal success to main thread
    Error = false;
    InitSuccess = true;
    InitEvent.Raise();

    while (baseref->Running) {
        if (baseref->CaptureFrame() != SVL_OK) {
            Error = true;
            break;
        }
    }

	return this;
}
