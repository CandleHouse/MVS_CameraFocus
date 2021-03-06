
#ifndef _MV_INCLUDE_H_
#define _MV_INCLUDE_H_

#include "CameraParams.h"
#include "ObsoleteCamParams.h"

/**
 *  @brief  动态库导入导出定义
 *  @brief  Import and export of dynamic link library definition
 */
#ifndef MV_CAMCTRL_API

    #ifdef WIN32
        #if defined(MV_CAMCTRL_EXPORTS)
            #define MV_CAMCTRL_API __declspec(dllexport)
        #else
            #define MV_CAMCTRL_API __declspec(dllimport)
        #endif
    #else
        #ifndef __stdcall
            #define __stdcall
        #endif

        #ifndef MV_CAMCTRL_API
            #define  MV_CAMCTRL_API
        #endif
    #endif

#endif


#ifdef WIN32
#include <objbase.h> // interface
#else
#define interface struct
#endif

namespace MvCamCtrl
{

    typedef  void       ITransportLayer;
    typedef  void*      TlProxy;
    class               MvCamera;
    class               CTlRefs;
    class               CDevRefs;


}

#endif /* _MV_INCLUDE_H_ */
