#include <openxr/openxr.h>
#include <cstdio>
#include <cstring>
int main() {
    XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy_s(info.applicationInfo.applicationName,"Origins VR resolution");
    info.applicationInfo.apiVersion=XR_API_VERSION_1_0;
    XrInstance instance=XR_NULL_HANDLE;
    // Exit 4 = no 32-bit OpenXR runtime can be loaded at all (Pimax registers its 64-bit runtime for 32-bit apps too):
    // the launcher then retries with SteamVR's 32-bit runtime. A connected-but-idle headset still fails later (1 or 2).
    uint32_t extensions=0;
    if(XR_FAILED(xrEnumerateInstanceExtensionProperties(nullptr,0,&extensions,nullptr))) { fprintf(stderr,"No 32-bit OpenXR runtime\n"); return 4; }
    XrResult result=xrCreateInstance(&info,&instance);
    if(XR_FAILED(result)) { fprintf(stderr,"OpenXR create instance failed %d\n",result); return 1; }
    XrSystemGetInfo request{XR_TYPE_SYSTEM_GET_INFO}; request.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system=XR_NULL_SYSTEM_ID;
    result=xrGetSystem(instance,&request,&system);
    if(XR_FAILED(result)) { xrDestroyInstance(instance); fprintf(stderr,"Headset unavailable %d\n",result); return 2; }
    XrViewConfigurationView eyes[2]={{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    uint32_t count=0;
    result=xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,eyes);
    XrInstanceProperties runtime{XR_TYPE_INSTANCE_PROPERTIES}; xrGetInstanceProperties(instance,&runtime);
    xrDestroyInstance(instance);
    if(XR_FAILED(result) || count!=2) return 3;
    // Both allocations cover the maximum recommendation, never a monitor-derived fallback.
    unsigned w=eyes[0].recommendedImageRectWidth>eyes[1].recommendedImageRectWidth ? eyes[0].recommendedImageRectWidth:eyes[1].recommendedImageRectWidth;
    unsigned h=eyes[0].recommendedImageRectHeight>eyes[1].recommendedImageRectHeight ? eyes[0].recommendedImageRectHeight:eyes[1].recommendedImageRectHeight;
    printf("{\"width\":%u,\"height\":%u,\"runtime\":\"%s\"}\n",w,h,runtime.runtimeName);
    return 0;
}
