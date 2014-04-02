#include "CameraHal.h"
namespace android{
#define LOG_TAG "CameraHal_Display"

static volatile int32_t gLogLevel = 1;

#ifdef ALOGD_IF
#define LOG1(...) ALOGD_IF(gLogLevel >= 1, __VA_ARGS__);
#define LOG2(...) ALOGD_IF(gLogLevel >= 2, __VA_ARGS__);
#else
#define LOG1(...) LOGD_IF(gLogLevel >= 1, __VA_ARGS__);
#define LOG2(...) LOGD_IF(gLogLevel >= 2, __VA_ARGS__);
#endif

#define LOG_FUNCTION_NAME           LOG1("%s Enter", __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT      LOG1("%s Exit ", __FUNCTION__);

#define DISPLAY_FORMAT CameraParameters::PIXEL_FORMAT_YUV420P
DisplayAdapter::DisplayAdapter()
              :displayThreadCommandQ("displayCmdQ")
{
    LOGD("%s(%d):IN",__FUNCTION__,__LINE__);
//	strcpy(mDisplayFormat,CAMERA_DISPLAY_FORMAT_YUV420SP/*CAMERA_DISPLAY_FORMAT_YUV420SP*/);
    strcpy(mDisplayFormat,DISPLAY_FORMAT);
    mFrameProvider =  NULL;
    mDisplayRuning = -1;
    mDislayBufNum = 0;
    mDisplayWidth = 0;
    mDisplayHeight = 0;
    mDispBufUndqueueMin = 0;
    mANativeWindow = NULL;
    mDisplayBufInfo =NULL;
    mDisplayState = 0;
    //create display thread

    mDisplayThread = new DisplayThread(this);
    mDisplayThread->run("DisplayThread",ANDROID_PRIORITY_DISPLAY);
    LOGD("%s(%d):OUT",__FUNCTION__,__LINE__);
}
DisplayAdapter::~DisplayAdapter()
{
    LOGD("%s(%d):IN",__FUNCTION__,__LINE__);

    if(mDisplayThread != NULL){
        //stop thread and exit
        if(mDisplayRuning != STA_DISPLAY_STOP)
            stopDisplay();
        mDisplayThread->requestExitAndWait();
        mDisplayThread.clear();
    }
    LOGD("%s(%d):OUT",__FUNCTION__,__LINE__);
}
void DisplayAdapter::dump()
{

}

void DisplayAdapter::setDisplayState(int state)
{
	mDisplayState = state;
}

bool DisplayAdapter::isNeedSendToDisplay()
{
    Mutex::Autolock lock(mDisplayLock);

    if((mDisplayRuning == STA_DISPLAY_PAUSE) || (mDisplayRuning == STA_DISPLAY_STOP))
        return false;
    else{
        LOG2("%s(%d): need to display this frame",__FUNCTION__,__LINE__);
        return true;
    }
}
void DisplayAdapter::notifyNewFrame(FramInfo_s* frame)
{
    mDisplayLock.lock();
    //send a frame to display
    if(mDisplayRuning == STA_DISPLAY_RUNNING){
        Message msg;
        msg.command = CMD_DISPLAY_FRAME;
        msg.arg1 = NULL;
        msg.arg2 = (void*)frame;
        msg.arg3 = (void*)(frame->used_flag);
        displayThreadCommandQ.put(&msg);
        mDisplayCond.signal();
    }else{
    //must return frame if failed to send display
        if(mFrameProvider)
            mFrameProvider->returnFrame(frame->frame_index,frame->used_flag);
    }
    mDisplayLock.unlock();
}


int DisplayAdapter::startDisplay(int width, int height)
{
    int err = NO_ERROR;
    Message msg;
    Semaphore sem;
    LOGD("%s(%d):IN",__FUNCTION__,__LINE__);
    mDisplayLock.lock();
    if (mDisplayRuning == STA_DISPLAY_RUNNING) {
        LOGD("%s(%d): display thread is already run",__FUNCTION__,__LINE__);
        goto cameraDisplayThreadStart_end;
    }
    mDisplayWidth = width;
    mDisplayHeight = height;
    setDisplayState(CMD_DISPLAY_START_PREPARE);
    msg.command = CMD_DISPLAY_START;
    sem.Create();
    msg.arg1 = (void*)(&sem);
    displayThreadCommandQ.put(&msg);    
    mDisplayCond.signal();
cameraDisplayThreadStart_end:
    mDisplayLock.unlock(); 
    if(msg.arg1){
        sem.Wait();
		if(mDisplayState != CMD_DISPLAY_START_DONE)
			err = -1;
    }
    LOGD("%s(%d):OUT",__FUNCTION__,__LINE__);
    return err;
}
//exit display
int DisplayAdapter::stopDisplay()
{
    int err = NO_ERROR;
    Message msg;
    Semaphore sem;
    LOGD("%s(%d):IN",__FUNCTION__,__LINE__);
    mDisplayLock.lock();
    if (mDisplayRuning == STA_DISPLAY_STOP) {
        LOGD("%s(%d): display thread is already pause",__FUNCTION__,__LINE__);
        goto cameraDisplayThreadPause_end;
    }
    setDisplayState(CMD_DISPLAY_STOP_PREPARE);
    msg.command = CMD_DISPLAY_STOP ;
    sem.Create();
    msg.arg1 = (void*)(&sem);
    displayThreadCommandQ.put(&msg);
    mDisplayCond.signal();
cameraDisplayThreadPause_end:
    mDisplayLock.unlock(); 
    if(msg.arg1){
        sem.Wait();
		if(mDisplayState != CMD_DISPLAY_STOP_DONE)
			err = -1;		
    }
    LOGD("%s(%d):OUT",__FUNCTION__,__LINE__);
    return err;

}
int DisplayAdapter::pauseDisplay()
{
    int err = NO_ERROR;
    Message msg;
    Semaphore sem;
    mDisplayLock.lock();
    LOGD("%s(%d):IN",__FUNCTION__,__LINE__);
    if (mDisplayRuning == STA_DISPLAY_PAUSE) {
        LOGD("%s(%d): display thread is already stop",__FUNCTION__,__LINE__);
        goto cameraDisplayThreadStop_end;
    }
    setDisplayState(CMD_DISPLAY_PAUSE_PREPARE);
    msg.command = CMD_DISPLAY_PAUSE;
    sem.Create();
    msg.arg1 = (void*)(&sem);
    displayThreadCommandQ.put(&msg);
	mDisplayCond.signal();
cameraDisplayThreadStop_end:
    mDisplayLock.unlock(); 
    if(msg.arg1){
        sem.Wait();
		if(mDisplayState != CMD_DISPLAY_PAUSE_DONE)
			err = -1;		
    }
    LOGD("%s(%d):OUT",__FUNCTION__,__LINE__);
    return err;
}

int DisplayAdapter::setPreviewWindow(struct preview_stream_ops* window)
{
    //mDisplayRuning status
    //mANativeWindow null?
    //window null ?
    LOGD("%s(%d):IN",__FUNCTION__,__LINE__);
    if(window == mANativeWindow){
        return 0;
    }
    if(mANativeWindow){
        pauseDisplay();
    }
    mANativeWindow = window;
    LOGD("%s(%d):OUT",__FUNCTION__,__LINE__);
    return 0;
}

int DisplayAdapter::getDisplayStatus(void)
{
    Mutex::Autolock lock(mDisplayLock);

    return mDisplayRuning;
}

void DisplayAdapter::setFrameProvider(FrameProvider* framePro)
{
    mFrameProvider = framePro;
}


struct preview_stream_ops* DisplayAdapter::getPreviewWindow()
{
    return mANativeWindow;
}

//internal func
int DisplayAdapter::cameraDisplayBufferCreate(int width, int height, const char *fmt,int numBufs)
{
    int err = NO_ERROR,undequeued = 0;
    int i, total=0;
    buffer_handle_t* hnd = NULL;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds;  
    int stride; 
    
    //LOG_FUNCTION_NAME
    if(!mANativeWindow){
        LOGE("%s(%d): nativewindow is null!",__FUNCTION__,__LINE__);
    }
    if(mDisplayBufInfo){
        //destroy buffer first
        cameraDisplayBufferDestory();
    }
    mDislayBufNum = CONFIG_CAMERA_DISPLAY_BUF_CNT;

    // Set gralloc usage bits for window.
    err = mANativeWindow->set_usage(mANativeWindow, CAMHAL_GRALLOC_USAGE);
    if (err != 0) {
        LOGE("%s(%d): %s(err:%d) native_window_set_usage failed", __FUNCTION__,__LINE__, strerror(-err), -err);

        if ( ENODEV == err ) {
            LOGE("%s(%d): Preview surface abandoned !",__FUNCTION__,__LINE__);
            mANativeWindow = NULL;
        }

        goto fail;
    }
    /* ddl@rock-chips.com: NativeWindow switch to async mode after v0.1.3 */
    /*
    if (mANativeWindow->set_swap_interval(mANativeWindow, 1) != 0) {
        LOGE("%s(%d): set mANativeWindow run in synchronous mode failed",__FUNCTION__,__LINE__);
    }
    */
    mANativeWindow->get_min_undequeued_buffer_count(mANativeWindow, &undequeued);
    mDispBufUndqueueMin = undequeued;
    ///Set the number of buffers needed for camera preview
    
    //total = numBufs+undequeued;
    total = numBufs;
    LOG1("%s(%d): min_undequeued:0x%x total:0x%x",__FUNCTION__,__LINE__, undequeued, total);
    err = mANativeWindow->set_buffer_count(mANativeWindow, total);
    if (err != 0) {
        LOGE("%s(%d): %s(err:%d) native_window_set_buffer_count(%d+%d) failed", __FUNCTION__,__LINE__,strerror(-err), -err,numBufs,undequeued);

        if ( ENODEV == err ) {
            LOGE("%s(%d): Preview surface abandoned !",__FUNCTION__,__LINE__);
            mANativeWindow = NULL;
        }
        goto fail;
    }
    

    // Set window geometry
    err = mANativeWindow->set_buffers_geometry(
            mANativeWindow,
            width,
            height,
            cameraPixFmt2HalPixFmt(fmt)); 
            

    if (err != 0) {
        LOGE("%s(%d): %s(err:%d) native_window_set_buffers_geometry failed", __FUNCTION__,__LINE__, strerror(-err), -err);

        if ( ENODEV == err ) {
            LOGE("%s(%d): Preview surface abandoned !",__FUNCTION__,__LINE__);
            mANativeWindow = NULL;
        }

        goto fail;
    }    
    mDisplayBufInfo = (rk_displaybuf_info_t*)malloc(sizeof(rk_displaybuf_info_t)*total);
    if(!mDisplayBufInfo){
        LOGE("%s(%d): malloc diaplay buffer structue failed!",__FUNCTION__,__LINE__);
        err = -1;
        goto fail;
    }
    for ( i=0; i < total; i++ ) {         
        err = mANativeWindow->dequeue_buffer(mANativeWindow, (buffer_handle_t**)&hnd, &stride);

        if (err != 0) {
            LOGE("%s(%d): %s(err:%d) dequeueBuffer failed", __FUNCTION__,__LINE__, strerror(-err), -err);

            if ( ENODEV == err ) {
                LOGE("%s(%d): Preview surface abandoned !",__FUNCTION__,__LINE__);
                mANativeWindow = NULL;
            }

            goto fail;
        }
        mDisplayBufInfo[i].lock = new Mutex();
        mDisplayBufInfo[i].buffer_hnd = hnd;
        mDisplayBufInfo[i].priv_hnd= (NATIVE_HANDLE_TYPE*)(*hnd);
        mDisplayBufInfo[i].stride = stride;
    #if defined(TARGET_RK29) 
        struct pmem_region sub;
    
        if (ioctl(mDisplayBufInfo[i].priv_hnd->fd,PMEM_GET_PHYS,&sub) == 0) {                    
            mDisplayBufInfo[i].phy_addr = sub.offset + mDisplayBufInfo[i].priv_hnd->offset;    /* phy address */ 
        } else {   
            /* ddl@rock-chips.com: gralloc buffer is not continuous in phy */
            mDisplayBufInfo[i].phy_addr = 0x00;
        }        
    #else
        mDisplayBufInfo[i].phy_addr = 0x00;        
    #endif
        
    }
    // lock the initial queueable buffers
    bounds.left = 0;
    bounds.top = 0;
    bounds.right = width;
    bounds.bottom = height;
    for( i = 0;  i < total; i++ ) {
        void* y_uv[3];
        
        mANativeWindow->lock_buffer(mANativeWindow, (buffer_handle_t*)mDisplayBufInfo[i].buffer_hnd);
        mapper.lock((buffer_handle_t)mDisplayBufInfo[i].priv_hnd, CAMHAL_GRALLOC_USAGE, bounds, y_uv);
        #if defined(TARGET_BOARD_PLATFORM_RK30XX) || defined(TARGET_RK29) || defined(TARGET_BOARD_PLATFORM_RK2928)
            mDisplayBufInfo[i].vir_addr = mDisplayBufInfo[i].priv_hnd->base;
        #elif defined(TARGET_BOARD_PLATFORM_RK30XXB)
            mDisplayBufInfo[i].vir_addr = (int)y_uv[0];
        #endif
        setBufferState(i,0);
        LOGD("%s(%d): mGrallocBufferMap[%d] phy_addr: 0x%x  vir_dir: 0x%x",
            __FUNCTION__,__LINE__, i, mDisplayBufInfo[i].phy_addr,mDisplayBufInfo[i].vir_addr);
    }

    LOG_FUNCTION_NAME_EXIT    
    return err; 
 fail:
        for (i = 0; i<total; i++) {
            if (mDisplayBufInfo[i].buffer_hnd) {
                err = mANativeWindow->cancel_buffer(mANativeWindow, (buffer_handle_t*)mDisplayBufInfo[i].buffer_hnd);
                if (err != 0) {
                  LOGE("%s(%d): cancelBuffer failed w/ error 0x%08x",__FUNCTION__,__LINE__, err);                  
                }
            }
        }
    
    LOGE("%s(%d): exit with error(%d)!",__FUNCTION__,__LINE__,err);
    return err;
}

int DisplayAdapter::cameraDisplayBufferDestory(void)
{
    int ret = NO_ERROR,i;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    LOG_FUNCTION_NAME
    //Give the buffers back to display here -  sort of free it
    if (mANativeWindow) {
        for(i = 0; i < mDislayBufNum; i++) {
            // unlock buffer before giving it up
            if (mDisplayBufInfo[i].priv_hnd && (mDisplayBufInfo[i].buf_state == 0) ) {
                mapper.unlock((buffer_handle_t)mDisplayBufInfo[i].priv_hnd);
                mANativeWindow->cancel_buffer(mANativeWindow, (buffer_handle_t*)mDisplayBufInfo[i].buffer_hnd);
            }
            mDisplayBufInfo[i].buffer_hnd = NULL;
            mDisplayBufInfo[i].priv_hnd = NULL;
            delete mDisplayBufInfo[i].lock;
        }
        if(mDisplayBufInfo){
            free(mDisplayBufInfo);
            mDisplayBufInfo = NULL;
            mDislayBufNum = 0;
            mANativeWindow = NULL;
        }
    } else {
        LOGD("%s(%d): mANativeWindow is NULL, destory is ignore",__FUNCTION__,__LINE__);
    }

    LOG_FUNCTION_NAME_EXIT
cameraDisplayBufferDestory_end:
    return ret;    
}
void DisplayAdapter::setBufferState(int index,int status)
{
    rk_displaybuf_info_t* buf_hnd = NULL;
    if(mDisplayBufInfo){
        buf_hnd = mDisplayBufInfo+index;
        buf_hnd->buf_state = status;
    }else{
        LOGE("%s(%d):display buffer is null.",__FUNCTION__,__LINE__);
    }
}

extern "C" void arm_yuyv_to_nv12(int src_w, int src_h,char *srcbuf, char *dstbuf){

    char *srcbuf_begin;
    int *dstint_y, *dstint_uv, *srcint;
    int i = 0,j = 0;
    int y_size = 0;

    y_size = src_w*src_h;
    dstint_y = (int*)dstbuf;
    srcint = (int*)srcbuf;
    for(i=0;i<(y_size>>2);i++) {
        *dstint_y++ = ((*(srcint+1)&0x00ff0000)<<8)|((*(srcint+1)&0x000000ff)<<16)
                    |((*srcint&0x00ff0000)>>8)|(*srcint&0x000000ff);

        srcint += 2;
    }
    dstint_uv =  (int*)(dstbuf + y_size);
    srcint = (int*)srcbuf;
    for(i=0;i<src_h/2; i++) {
        for (j=0; j<(src_w>>2); j++) {
			#if 1
            *dstint_uv++ = (*(srcint+1)&0xff000000)|((*(srcint+1)&0x0000ff00)<<8)
                        |((*srcint&0xff000000)>>16)|((*srcint&0x0000ff00)>>8);
			#else
			*dstint_uv++ = (((*(srcint+1)&0xff000000)>>8)|((*(srcint+1)&0x0000ff00)<<16)
                        |((*srcint&0xff000000)>>24)|(*srcint&0x0000ff00));
			#endif
            srcint += 2;
        }
        srcint += (src_w>>1);
    }

}

extern "C" void arm_yuyv_to_yv12(int src_w, int src_h,char *srcbuf, char *dstbuf){

    char *srcbuf_begin;
    int *dstint_y, *dstint_uv, *srcint;
    short int* dstsint_u, *dstsint_v;
    int i = 0,j = 0;
    int y_size = 0;

    y_size = src_w*src_h;
    dstint_y = (int*)dstbuf;
    srcint = (int*)srcbuf;
    for(i=0;i<(y_size>>2);i++) {
        *dstint_y++ = ((*(srcint+1)&0x00ff0000)<<8)|((*(srcint+1)&0x000000ff)<<16)
                    |((*srcint&0x00ff0000)>>8)|(*srcint&0x000000ff);

        srcint += 2;
    }
    dstsint_u = (short int*)(dstbuf + y_size);
    dstsint_v = (short int*)((char*)dstsint_u + (y_size >> 2));
    srcint = (int*)srcbuf;
    for(i=0;i<src_h/2; i++) {
        for (j=0; j<(src_w>>2); j++) {

            *dstsint_v++ = (((*srcint&0x0000ff00)>>8) | ((*(srcint+1)&0x0000ff00)));
            *dstsint_u++ = (((*srcint&0xff000000)>>24) | ((*(srcint+1)&0xff000000)>>16));
            
            srcint += 2;
        }
        srcint += (src_w>>1);
    }

}
//for soc camera test
extern "C" void arm_yuyv_to_nv12_soc_ex(int src_w, int src_h,char *srcbuf, char *dstbuf){
    char *srcbuf_begin;
    char  *srcint;
    char *dstint_y; 
    char *dstint_uv;
    int i = 0,j = 0;
    int y_size = 0;


    y_size = src_w*src_h;
    dstint_y = (char*)dstbuf;
    srcint = (char*)srcbuf;
    for(i=0;i<(y_size);i++) {

       *dstint_y++ = ((*(srcint+1)&0x3f) << 2) | ((*(srcint+0)>> 6) & 0x03);

       srcint += 4;

    }
    #if 1
    dstint_uv =  (char*)(dstbuf + y_size);
    srcint = (char*)srcbuf;
    for(i=0;i<src_h/2; i++) {
        for (j=0; j<(src_w >> 1 ); j++) {
			*dstint_uv++ = (((*(srcint+3) &0x3f ) << 2) | ((*(srcint+2) >> 6 ) & 0x03 ));
			*dstint_uv++ = (((*(srcint+7) &0x3f ) << 2) | ((*(srcint+6) >> 6 ) & 0x03 ));
            srcint += 8;
        }
        srcint += (src_w<<2);
    }
    #endif

}



void DisplayAdapter::displayThread()
{
    int err,stride,i,queue_cnt;
    int dequeue_buf_index,queue_buf_index,queue_display_index;
    buffer_handle_t *hnd = NULL; 
    NATIVE_HANDLE_TYPE *phnd;
    GraphicBufferMapper& mapper = GraphicBufferMapper::get();
    Message msg;
    void *y_uv[3];
    int frame_used_flag = -1;
    Rect bounds;
    
    LOG_FUNCTION_NAME    
    while (mDisplayRuning != STA_DISPLAY_STOP) {
display_receive_cmd:        
        if (displayThreadCommandQ.isEmpty() == false ) {
            displayThreadCommandQ.get(&msg);         

            switch (msg.command)
            {
                case CMD_DISPLAY_START:
                {
                    LOGD("%s(%d): receive CMD_DISPLAY_START", __FUNCTION__,__LINE__);
                    cameraDisplayBufferDestory(); 
                    cameraDisplayBufferCreate(mDisplayWidth, mDisplayHeight,mDisplayFormat,CONFIG_CAMERA_DISPLAY_BUF_CNT);
                    mDisplayRuning = STA_DISPLAY_RUNNING;
					setDisplayState(CMD_DISPLAY_START_DONE);
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
                    break;
                }

                case CMD_DISPLAY_PAUSE:
                {
                    LOGD("%s(%d): receive CMD_DISPLAY_PAUSE", __FUNCTION__,__LINE__);

                    cameraDisplayBufferDestory();
                    mDisplayRuning = STA_DISPLAY_PAUSE;
					setDisplayState(CMD_DISPLAY_PAUSE_DONE);
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
                    break;
                }
                
                case CMD_DISPLAY_STOP:
                {
                    LOGD("%s(%d): receive CMD_DISPLAY_STOP", __FUNCTION__,__LINE__);
					cameraDisplayBufferDestory();
                    mDisplayRuning = STA_DISPLAY_STOP;
					setDisplayState(CMD_DISPLAY_STOP_DONE);
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
                    continue;
                }

                case CMD_DISPLAY_FRAME:
                {  
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();

                    if (mDisplayRuning != STA_DISPLAY_RUNNING) 
                        goto display_receive_cmd;
                        
                   
                    if (mANativeWindow == NULL) {
                        LOGE("%s(%d): thread exit, because mANativeWindow is NULL", __FUNCTION__,__LINE__);
                        mDisplayRuning = STA_DISPLAY_STOP;  
                        continue;
                    }

                    FramInfo_s* frame = (FramInfo_s*)msg.arg2;
                    frame_used_flag = (int)msg.arg3;


                    
                    queue_buf_index = (int)msg.arg1;                    
                    queue_display_index = CONFIG_CAMERA_DISPLAY_BUF_CNT;
                    //get a free buffer                        
                        for (i=0; i<CONFIG_CAMERA_DISPLAY_BUF_CNT; i++) {
                            if (mDisplayBufInfo[i].buf_state == 0) 
                                break;
                        }
                        if (i<CONFIG_CAMERA_DISPLAY_BUF_CNT) {
                            queue_display_index = i;
                        } else {
                            err = 0x01;
                            while (err != 0) {
                                err = mANativeWindow->dequeue_buffer(mANativeWindow, (buffer_handle_t**)&hnd, &stride);
                                if (err == 0) {
                                    // lock the initial queueable buffers
                                    bounds.left = 0;
                                    bounds.top = 0;
                                    bounds.right = mDisplayWidth;
                                    bounds.bottom = mDisplayHeight;
                                    mANativeWindow->lock_buffer(mANativeWindow, (buffer_handle_t*)hnd);
                                    mapper.lock((buffer_handle_t)(*hnd), CAMHAL_GRALLOC_USAGE, bounds, y_uv);

                                    phnd = (NATIVE_HANDLE_TYPE*)*hnd;
                                    for (i=0; i<CONFIG_CAMERA_DISPLAY_BUF_CNT; i++) {
                                        if (phnd == mDisplayBufInfo[i].priv_hnd) {  
                                            queue_display_index = i;
                                            break;
                                        }
                                    }
                                    if(i == CONFIG_CAMERA_DISPLAY_BUF_CNT){
                                        err = mANativeWindow->cancel_buffer(mANativeWindow, (buffer_handle_t*)hnd);

                                        //erro,return buffer
                                        if(mFrameProvider)
                                            mFrameProvider->returnFrame(frame->frame_index,frame_used_flag);
                                        //receive another msg
                                         continue;
                                    }
                                    //set buffer status,dequed,but unused
                                    setBufferState(queue_display_index, 0);
                                } else {
                                    LOGD("%s(%d): %s(err:%d) dequeueBuffer failed, so pause here", __FUNCTION__,__LINE__, strerror(-err), -err);

                                    mDisplayLock.lock();
                                    //return this frame to frame provider
                                    if(mFrameProvider)
                                        mFrameProvider->returnFrame(frame->frame_index,frame_used_flag);
                                    if (displayThreadCommandQ.isEmpty() == false ) {
                                        mDisplayLock.unlock(); 
                                        goto display_receive_cmd;
                                    }
						                                    
                                    mDisplayCond.wait(mDisplayLock); 
                                    mDisplayLock.unlock();
                                    LOG2("%s(%d): wake up...", __FUNCTION__,__LINE__);
                                }
                            }
                        } 
                        //fill display buffer
                   
#if 1
                 //   rga_nv12torgb565(frame->frame_width, frame->frame_height, 
                  //                  (char*)(frame->vir_addr), (short int*)mDisplayBufInfo[queue_display_index].vir_addr, 
                   //                 mDisplayWidth,mDisplayWidth,mDisplayHeight);

            //        if((frame->frame_fmt == V4L2_PIX_FMT_YUYV) /*&& (strcmp((mDisplayFormat),CAMERA_DISPLAY_FORMAT_YUV420SP)==0)*/)
                    if((frame->frame_fmt == V4L2_PIX_FMT_YUYV) && (strcmp((mDisplayFormat),CAMERA_DISPLAY_FORMAT_YUV420P)==0))
                    {

								arm_yuyv_to_yv12(frame->frame_width, frame->frame_height,
                                 (char*)(frame->vir_addr), (char*)mDisplayBufInfo[queue_display_index].vir_addr);
						}else if((frame->frame_fmt == V4L2_PIX_FMT_NV12) && (strcmp((mDisplayFormat),CAMERA_DISPLAY_FORMAT_RGB565)==0))
                    {
                       arm_nv12torgb565(frame->frame_width, frame->frame_height,
                						(char*)(frame->vir_addr), (short int*)mDisplayBufInfo[queue_display_index].vir_addr,
                                         mDisplayWidth);
                    }else if((frame->frame_fmt == V4L2_PIX_FMT_NV12) && (strcmp((mDisplayFormat),CAMERA_DISPLAY_FORMAT_YUV420SP)==0))
                        arm_camera_yuv420_scale_arm(V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_NV12, 
							(char*)(frame->vir_addr), (char*)mDisplayBufInfo[queue_display_index].vir_addr,
							frame->frame_width, frame->frame_height,
							mDisplayWidth, mDisplayHeight,
							false);
#endif
//                    LOGD("%s(%d): receive buffer %d, queue buffer %d to display", __FUNCTION__,__LINE__,queue_buf_index,queue_display_index);

                    setBufferState(queue_display_index, 1);
                    mapper.unlock((buffer_handle_t)mDisplayBufInfo[queue_display_index].priv_hnd);
                    err = mANativeWindow->enqueue_buffer(mANativeWindow, (buffer_handle_t*)mDisplayBufInfo[queue_display_index].buffer_hnd);                    
                    if (err != 0){
                                                
                        bounds.left = 0;
                        bounds.top = 0;
                        bounds.right = mDisplayWidth ;
                        bounds.bottom = mDisplayHeight;
                        mANativeWindow->lock_buffer(mANativeWindow, (buffer_handle_t*)mDisplayBufInfo[queue_display_index].buffer_hnd);
                        mapper.lock((buffer_handle_t)(mDisplayBufInfo[queue_display_index].priv_hnd), CAMHAL_GRALLOC_USAGE, bounds, y_uv);

                        mDisplayRuning = STA_DISPLAY_PAUSE;
                        LOGE("%s(%d): enqueue buffer %d to mANativeWindow failed(%d),so display pause", __FUNCTION__,__LINE__,queue_display_index,err);
                    }                

                    //return this frame to frame provider
                    if(mFrameProvider)
                        mFrameProvider->returnFrame(frame->frame_index,frame_used_flag);
                    
                    //deque a display buffer

                        queue_cnt = 0;
                        for (i=0; i<mDislayBufNum; i++) {
                            if (mDisplayBufInfo[i].buf_state == 1) 
                                queue_cnt++;
                        }
						
                        if (queue_cnt > mDispBufUndqueueMin) {
							err = mANativeWindow->dequeue_buffer(mANativeWindow, (buffer_handle_t**)&hnd, &stride);
							if (err == 0) {                                    
                                // lock the initial queueable buffers
                                bounds.left = 0;
                                bounds.top = 0;
                                bounds.right = mDisplayWidth;
                                bounds.bottom = mDisplayHeight;
                                mANativeWindow->lock_buffer(mANativeWindow, (buffer_handle_t*)hnd);
                                mapper.lock((buffer_handle_t)(*hnd), CAMHAL_GRALLOC_USAGE, bounds, y_uv);

                                phnd = (NATIVE_HANDLE_TYPE*)*hnd;
                                for (i=0; i<mDislayBufNum; i++) {
                                    if (phnd == mDisplayBufInfo[i].priv_hnd) {
                                        dequeue_buf_index = i;
                                        break;
                                    }
                                }
                                
                                if (i >= mDislayBufNum) {                    
                                    LOGE("%s(%d): dequeue buffer(0x%x ) don't find in mDisplayBufferMap", __FUNCTION__,__LINE__,(int)phnd);                    
                                    continue;
                                } else {
                                    setBufferState(dequeue_buf_index, 0);
                                }
                                
                            } else {
                                /* ddl@rock-chips.com: dequeueBuffer isn't block, when ANativeWindow in asynchronous mode */
                                LOG2("%s(%d): %s(err:%d) dequeueBuffer failed, so pause here", __FUNCTION__,__LINE__, strerror(-err), -err);
                                mDisplayLock.lock();
                                if (displayThreadCommandQ.isEmpty() == false ) {
                                    mDisplayLock.unlock(); 
                                    goto display_receive_cmd;
                                }                  
                                mDisplayCond.wait(mDisplayLock); 
                                mDisplayLock.unlock();
                                LOG2("%s(%d): wake up...", __FUNCTION__,__LINE__);
                            }
                        }
                    }                    
                    break;
        
                default:
                {
                    LOGE("%s(%d): receive unknow command(0x%x)!", __FUNCTION__,__LINE__,msg.command);
                    break;
                }
            }
        }
        
        mDisplayLock.lock();
        if (displayThreadCommandQ.isEmpty() == false ) {
            mDisplayLock.unlock(); 
            goto display_receive_cmd;
        }        	
        LOG2("%s(%d): display thread pause here... ", __FUNCTION__,__LINE__);
        mDisplayCond.wait(mDisplayLock);  
        mDisplayLock.unlock(); 
        LOG2("%s(%d): display thread wake up... ", __FUNCTION__,__LINE__);
        goto display_receive_cmd;        
        
    }
    LOG_FUNCTION_NAME_EXIT
}

} // namespace android

