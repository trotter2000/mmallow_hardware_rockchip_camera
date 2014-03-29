#ifndef ANDROID_HARDWARE_CAMERA_ISP_HARDWARE_H
#define ANDROID_HARDWARE_CAMERA_ISP_HARDWARE_H

//usb camera adapter
#include "CameraHal.h"
#include "cam_api/camdevice.h"
#include <string>
#include <utils/KeyedVector.h>


namespace android{
class CameraIspAdapter: public CameraAdapter,public BufferCb
{
public:
	static int preview_frame_inval;
    static int DEFAULTPREVIEWWIDTH;
    static int DEFAULTPREVIEWHEIGHT;
    CameraIspAdapter(int cameraId);
    virtual ~CameraIspAdapter();
    virtual status_t startPreview(int preview_w,int preview_h,int w, int h, int fmt,bool is_capture);
    virtual status_t stopPreview();
    virtual int setParameters(const CameraParameters &params_set);
    virtual void initDefaultParameters();
    virtual status_t autoFocus();
    virtual int getCurPreviewState(int *drv_w,int *drv_h);
    void AfpsResChangeCb();
    virtual void bufferCb( MediaBuffer_t* pMediaBuffer );

    virtual void setupPreview(int width_sensor,int height_sensor,int preview_w,int preview_h);
    
private:
    //talk to driver
    virtual int cameraCreate(int cameraId);
    virtual int cameraDestroy();
    virtual int adapterReturnFrame(int index,int cmd);


    //for isp
    void setScenarioMode(CamEngineModeType_t newScenarioMode);

    void setSensorItf(int newSensorItf);
    void enableAfps( bool enable = false );

    void loadSensor(int cameraId =-1 );
    void loadCalibData(const char* fileName = NULL);
    void openImage( const char* fileName = NULL);

    bool connectCamera();
    void disconnectCamera();

    int start();
    int pause();
    int stop();
protected:
    CamDevice       *m_camDevice;
    KeyedVector<void *, void *> mFrameInfoArray;
    void clearFrameArray();
	mutable Mutex mLock;

    std::string mSensorDriverFile[3];
    int mSensorItfCur;
};

class CameraIspSOCAdapter: public CameraIspAdapter
{
public:

    CameraIspSOCAdapter(int cameraId):CameraIspAdapter(cameraId){};
    virtual ~CameraIspSOCAdapter(){};
#if 0
    virtual int setParameters(const CameraParameters &params_set);
    virtual void initDefaultParameters()
    {
        mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, "10,15,30");  

    };
   virtual status_t autoFocus();
#endif
    virtual void setupPreview(int width_sensor,int height_sensor,int preview_w,int preview_h);
    virtual void bufferCb( MediaBuffer_t* pMediaBuffer );

};



}
#endif