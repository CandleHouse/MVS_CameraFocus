#include <stdio.h>
#include <process.h>
#include <conio.h>
#include "windows.h"
#include "MvCameraControl.h"
#include <tchar.h>
#include <QFile>

#include <opencv2/opencv.hpp>
using namespace cv;

#include <QDebug>
using namespace std;

HWND g_hwnd = NULL;
bool g_bExit = false;
unsigned int g_nPayloadSize = 0;
int max_diff = 0;                   // 一次对焦最大误差
int noise[100]={0};                 // 稳定率
int max_chess_diff = 0;             // 二次棋盘内对焦最大误差

// ch:等待按键输入 | en:Wait for key press
void WaitForKeyPress(void)
{
    while(!_kbhit())
    {
        Sleep(10);
    }
    _getch();
}

bool PrintDeviceInfo(MV_CC_DEVICE_INFO* pstMVDevInfo)
{
    if (NULL == pstMVDevInfo)
    {
        printf("The Pointer of pstMVDevInfo is NULL!\n");
        return false;
    }
    if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE)
    {
        int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
        int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
        int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
        int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);

        // ch:打印当前相机ip和用户自定义名字 | en:print current ip and user defined name
        printf("CurrentIp: %d.%d.%d.%d\n" , nIp1, nIp2, nIp3, nIp4);
        printf("UserDefinedName: %s\n\n" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    }
    else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
    {
        printf("UserDefinedName: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
        printf("Serial Number: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chSerialNumber);
        printf("Device Number: %d\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.nDeviceNumber);
    }
    else
    {
        printf("Not support.\n");
    }

    return true;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        g_hwnd = NULL;
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static  unsigned int __stdcall CreateRenderWindow(void* pUser)
{
    HINSTANCE hInstance = ::GetModuleHandle(NULL);              //获取应用程序的模块句柄
    WNDCLASSEX wc;
    wc.cbSize           = sizeof(wc);
    wc.style            = CS_HREDRAW | CS_VREDRAW;              //窗口的风格
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = hInstance;
    wc.hIcon            = ::LoadIcon(NULL, IDI_APPLICATION);    //图标风格
    wc.hIconSm          = ::LoadIcon( NULL, IDI_APPLICATION );
    wc.hbrBackground    = (HBRUSH)( COLOR_WINDOW + 1);          //背景色
    wc.hCursor          = ::LoadCursor(NULL, IDC_ARROW);        //鼠标风格
    wc.lpfnWndProc      = WndProc;                              //自定义消息处理函数
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = _T("RenderWindow");                       //该窗口类的名称

    if(!RegisterClassEx(&wc))
    {
        return 0;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD styleEx = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
    RECT rect = {0, 0, 640, 480};

    AdjustWindowRectEx(&rect, style, false, styleEx);

    HWND hWnd = CreateWindowEx(styleEx, _T("RenderWindow"), _T("Display"), style, 0, 0,
        rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, hInstance, NULL);
    if(hWnd == NULL)
    {
        return 0;
    }

    ::UpdateWindow(hWnd);
    ::ShowWindow(hWnd, SW_SHOW);

    g_hwnd = hWnd;

    MSG msg = {0};
    while(msg.message != WM_QUIT)
    {
        if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
// 二次对焦，在棋盘内部取点
int focusSpecify(Mat& img, vector<Point2f> corner){
    int diff = 0;
    int diff_thre = 50;                                      // 相邻像素间的误差阈值设定
    int diff_sum_thre = 7000;                                // 所有采样线段的误差阈值之和设定
    // 棋盘内小方格宽度
    int width = (corner[5].y - corner[0].y + corner[0].x - corner[48].x) / 13;

    // 横向采样
    for (int i = corner[48].y + (width / 2); i < corner[53].y; i += width) {
        uchar* ptrow = img.ptr<uchar>(i);
        for (int j = corner[48].x - width * 2; j < corner[0].x + width * 2; j++) {
            if (abs(ptrow[j + 1] - ptrow[j]) > diff_thre)
                diff += abs(ptrow[j + 1] - ptrow[j]);
        }
    }
    // 纵向采样
    for (int i = corner[48].y - (width * 2); i < corner[53].y + (width * 2); i++) {
        uchar* ptrow = img.ptr<uchar>(i);
        uchar* ptrow2 = img.ptr<uchar>(i + 1);
        for (int j = corner[48].x + width / 2; j < corner[0].x + width; j += width + 1) {
            if (abs(ptrow[j] - ptrow2[j]) > diff_thre)
                diff += abs(ptrow[j] - ptrow2[j]);
        }
    }

    // 绘制横向采样线段
    for (int i = corner[48].y + (width / 2); i < corner[53].y; i += width)
        line(img,Point(corner[48].x - width * 2,i),Point(corner[0].x + width * 2,i),Scalar(90, 90, 90),3);

    // 绘制纵向采样线段
    for (int i = corner[48].y - (width * 2); i < corner[53].y + (width * 2); i++) {
        uchar* ptrow = img.ptr<uchar>(i);
        for (int j = corner[48].x + width / 2; j < corner[0].x + width; j += width + 1) {
            ptrow[j-2] = 90;ptrow[j-1] = 90;ptrow[j] = 90;ptrow[j+1] = 90;ptrow[j+2] = 90;
        }
    }
    if (max_chess_diff < diff)
        max_chess_diff = diff;

    // 可自行选择二次对焦的阈值
//    if (diff < max_diff * 0.9)
//        cout <<"Chess_unfocous:"<< diff <<endl;
//    else
        cout <<"Chess_focous:"<< diff <<endl;
}
// 一次对焦，设定阈值判断是否失焦
bool focusDetect(Mat& img){

    int diff = 0;
    int diff_thre = 20;                                      // 相邻像素间的误差阈值设定
    int diff_sum_thre = 30000;                               // 所有采样线段的误差阈值之和设定
    int h1 = img.rows / 3;
    int h2 = img.rows * 2 / 3;
    int l1 = img.cols / 3;
    int l2 = img.cols * 2 / 3;
    // 十条行采样线段
    for (int i = h1; i <= h2; i += (h2 - h1) / 10){
        uchar* ptrow = img.ptr<uchar>(i);
        for (int j = l1; j <= l2; j++){
            if (abs(ptrow[j + 1] - ptrow[j]) > diff_thre)
                diff += abs(ptrow[j + 1] - ptrow[j]);
        }
//        cout << diff << endl;
    }
    // 十条列采样线段
    for (int i = h1; i <= h2; i++ ){
        uchar* ptrow = img.ptr<uchar>(i);
        uchar* ptrow2 = img.ptr<uchar>(i + 1);
        for (int j = l1; j <= l2; j += (l2 - l1) / 10){
            if (abs(ptrow[j] - ptrow2[j])>diff_thre)
                diff += abs(ptrow[j] - ptrow2[j]);
        }
    }
    if (max_diff < diff)
        max_diff = diff;

    bool res = true;
    int flag = 0;
    int pos = 0;
    int neg = 0;
    for (int i = 0; i < 100; i++) {
        if (noise[i] == 0)
            neg++;
        else
            pos++;
    }
    if (diff < max_diff * 0.88) {
        cout << "wrong focus! " << "current: " << diff << " max:" << max_diff << " rate:"<< setprecision(5) << double(pos/100.0) << endl;
        res = false;
    } else {
        // 使用棋盘标定检测
        vector<Point2f> corner;
        bool find = findChessboardCorners(img,Size(6,9),corner);
        drawChessboardCorners(img,Size(6,9),corner,find);
        // 二次对焦
        focusSpecify(img,corner);
        flag = 1;
        cout << "right focus! " << "current: " << diff << " max:" << max_diff << " rate:"<< setprecision(5) << double(pos/100.0) << endl;
    }
    // 更新近一百次对焦情况，判断稳定情况rate
    for (int i = 99; i > 0; i--) {
        noise[i] = noise[i - 1];
    }
    noise[0] = flag;

    // 绘制ROI
    for (int i = h1; i <= h1 + 4; i++){
        uchar* ptrow = img.ptr<uchar>(i);
        for (int j = l1; j <= l1 + 100; j++)
            ptrow[j] = 0;
        for (int j = l2 - 100; j <= l2; j++)
            ptrow[j] = 0;
    }
    for (int i = h2 - 4; i <= h2; i++){
        uchar* ptrow = img.ptr<uchar>(i);
        for (int j = l1; j <= l1 + 100; j++)
            ptrow[j] = 0;
        for (int j = l2 - 100; j <= l2; j++)
            ptrow[j] = 0;
    }
    for (int i = h1; i <= h1 + 70; i++){
        uchar* ptrow = img.ptr<uchar>(i);
        ptrow[l1] = 0;ptrow[l1+1] = 0;ptrow[l1+2] = 0;ptrow[l1+3] = 0;ptrow[l1+4] = 0;
        ptrow[l2] = 0;ptrow[l2-1] = 0;ptrow[l2-2] = 0;ptrow[l2-3] = 0;ptrow[l2-4] = 0;
    }
    for (int i = h2 - 70; i <= h2; i++){
        uchar* ptrow = img.ptr<uchar>(i);
        ptrow[l1] = 0;ptrow[l1+1] = 0;ptrow[l1+2] = 0;ptrow[l1+3] = 0;ptrow[l1+4] = 0;
        ptrow[l2] = 0;ptrow[l2-1] = 0;ptrow[l2-2] = 0;ptrow[l2-3] = 0;ptrow[l2-4] = 0;
    }
    return res;
}
static  unsigned int __stdcall WorkThread(void* pUser)
{
    int nRet = MV_OK;

    MV_FRAME_OUT_INFO_EX stImageInfo = {0};
    MV_DISPLAY_FRAME_INFO stDisplayInfo = {0};
    unsigned char * pData = (unsigned char *)malloc(sizeof(unsigned char) * (g_nPayloadSize));
    if (pData == NULL)
    {
        return 0;
    }
    unsigned int nDataSize = g_nPayloadSize;

    while(1)
    {
        nRet = MV_CC_GetOneFrameTimeout(pUser, pData, nDataSize, &stImageInfo, 1000);
        if (nRet == MV_OK)
        {
            printf("Get One Frame: Width[%d], Height[%d], nFrameNum[%d]",
                stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.nFrameNum);

            if (g_hwnd)
            {
                // 将Momo格式转为Opencv的Mat
                Mat getImage;
                getImage=Mat(stImageInfo.nHeight,stImageInfo.nWidth,CV_8UC1,pData);
                // 判断是否对焦
                focusDetect(getImage);

                stDisplayInfo.hWnd = g_hwnd;
                // 将绘制ROI的Mat.data赋值给MV_DISPLAY_FRAME_INFO.pData
                stDisplayInfo.pData = getImage.data;
                stDisplayInfo.nDataLen = stImageInfo.nFrameLen;
                stDisplayInfo.nWidth = stImageInfo.nWidth;
                stDisplayInfo.nHeight = stImageInfo.nHeight;
                stDisplayInfo.enPixelType = stImageInfo.enPixelType;

                MV_CC_DisplayOneFrame(pUser, &stDisplayInfo);
            }
        }
        else
        {
            printf("No data[0x%x]\n", nRet);
        }
        if(g_bExit)
        {
            break;
        }
    }

    free(pData);

    return 0;
}

int main()
{
    int nRet = MV_OK;
    void* handle = NULL;

    do
    {
        // ch:枚举设备 | en:Enum device
        MV_CC_DEVICE_INFO_LIST stDeviceList = {0};
        nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
        if (MV_OK != nRet)
        {
            printf("Enum Devices fail! nRet [0x%x]\n", nRet);
            break;
        }

        if (stDeviceList.nDeviceNum > 0)
        {
            for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++)
            {
                printf("[device %d]:\n", i);
                MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
                if (NULL == pDeviceInfo)
                {
                    break;
                }
                PrintDeviceInfo(pDeviceInfo);
            }
        }
        else
        {
            printf("Find No Devices!\n");
            break;
        }

        printf("Please Input camera index:");
        unsigned int nIndex = 0;
        scanf("%d", &nIndex);

        if (nIndex >= stDeviceList.nDeviceNum)
        {
            printf("Input error!\n");
            break;
        }

        // ch:选择设备并创建句柄 | en:Select device and create handle
        nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
        if (MV_OK != nRet)
        {
            printf("Create Handle fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:打开设备 | en:Open device
        nRet = MV_CC_OpenDevice(handle);
        if (MV_OK != nRet)
        {
            printf("Open Device fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:探测网络最佳包大小(只对GigE相机有效) | en:Detection network optimal package size(It only works for the GigE camera)
        if (stDeviceList.pDeviceInfo[nIndex]->nTLayerType == MV_GIGE_DEVICE)
        {
            int nPacketSize = MV_CC_GetOptimalPacketSize(handle);
            if (nPacketSize > 0)
            {
                nRet = MV_CC_SetIntValue(handle,"GevSCPSPacketSize",nPacketSize);
                if(nRet != MV_OK)
                {
                    printf("Warning: Set Packet Size fail nRet [0x%x]!", nRet);
                }
            }
            else
            {
                printf("Warning: Get Packet Size fail nRet [0x%x]!", nPacketSize);
            }
        }

        // ch:设置触发模式为off | en:Set trigger mode as off
        nRet = MV_CC_SetEnumValue(handle, "TriggerMode", 0);
        if (MV_OK != nRet)
        {
            printf("Set Trigger Mode fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:获取数据包大小 | en:Get payload size
        MVCC_INTVALUE stParam = {0};
        nRet = MV_CC_GetIntValue(handle, "PayloadSize", &stParam);
        if (MV_OK != nRet)
        {
            printf("Get PayloadSize fail! nRet [0x%x]\n", nRet);
            break;
        }
        g_nPayloadSize = stParam.nCurValue;

        unsigned int nThreadID = 0;
        void* hCreateWindow = (void*) _beginthreadex( NULL , 0 , CreateRenderWindow , handle, 0 , &nThreadID);

        if (NULL == hCreateWindow)
        {
            break;
        }

        // ch:开始取流 | en:Start grab image
        nRet = MV_CC_StartGrabbing(handle);
        if (MV_OK != nRet)
        {
            printf("Start Grabbing fail! nRet [0x%x]\n", nRet);
            break;
        }

        nThreadID = 0;
        void* hThreadHandle = (void*) _beginthreadex( NULL , 0 , WorkThread , handle, 0 , &nThreadID);
        if (NULL == hThreadHandle)
        {
            break;
        }

        printf("Press a key to stop grabbing.\n");
        WaitForKeyPress();

        g_bExit = true;
        WaitForSingleObject(hThreadHandle, INFINITE);
        CloseHandle(hThreadHandle);

        // ch:停止取流 | en:Stop grab image
        nRet = MV_CC_StopGrabbing(handle);
        if (MV_OK != nRet)
        {
            printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:关闭设备 | Close device
        nRet = MV_CC_CloseDevice(handle);
        if (MV_OK != nRet)
        {
            printf("ClosDevice fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:销毁句柄 | Destroy handle
        nRet = MV_CC_DestroyHandle(handle);
        if (MV_OK != nRet)
        {
            printf("Destroy Handle fail! nRet [0x%x]\n", nRet);
            break;
        }
    } while (0);


    if (nRet != MV_OK)
    {
        if (handle != NULL)
        {
            MV_CC_DestroyHandle(handle);
            handle = NULL;
        }
    }

    printf("Press a key to exit.\n");
    WaitForKeyPress();

    return 0;
}
