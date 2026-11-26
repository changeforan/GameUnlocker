#include "GpuHook.hpp"
#include "logger.hpp"
#include "SysPropHook.hpp"
#include <cstring>
#include <bytehook.h>

#define GL_VENDOR   0x1F00
#define GL_RENDERER 0x1F01
#define VK_MAX_PHYSICAL_DEVICE_NAME_SIZE 256
struct VkPhysicalDeviceProperties {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t deviceType;
    char deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    uint8_t pipelineCacheUUID[16];
};
struct VkPhysicalDeviceProperties2 {
    uint32_t sType;

    void* pNext;
    VkPhysicalDeviceProperties properties;
};

namespace gameunlocker {

static const char* spoofed_gl_vendor() {

    static std::string cached;
    if (cached.empty()) {
        auto profileOpt = SysPropHook::getProfile();
        if (profileOpt.has_value() && !profileOpt.value().gl_vendor.empty()) {
            cached = profileOpt.value().gl_vendor;
        } else {
            cached = "Qualcomm";
        }
    }
    return cached.c_str();
}

static const char* spoofed_gl_renderer() {

    static std::string cached;
    if (cached.empty()) {
        auto profileOpt = SysPropHook::getProfile();
        if (profileOpt.has_value() && !profileOpt.value().gl_renderer.empty()) {
            cached = profileOpt.value().gl_renderer;
        } else {
            cached = "Adreno (TM) 750";
        }
    }
    return cached.c_str();
}

static uint32_t spoofed_vk_vendor_id() {

    std::string vendor = spoofed_gl_vendor();
    if (vendor.find("Qualcomm") != std::string::npos) return 0x5143;
    if (vendor.find("ARM") != std::string::npos) return 0x13B5;
    if (vendor.find("Imagination") != std::string::npos) return 0x1010;
    return 0x5143; 
}

static uint32_t spoofed_vk_device_id() {

    std::string vendor = spoofed_gl_vendor();
    if (vendor.find("Qualcomm") != std::string::npos) return 0x07050000; 
    if (vendor.find("ARM") != std::string::npos) return 0x7150; 
    return 0x0;
}

static jstring (*orig_GLES20_glGetString)(JNIEnv*, jclass, jint) = nullptr;

static jstring (*orig_GLES30_glGetString)(JNIEnv*, jclass, jint) = nullptr;

static jstring (*orig_GLES31_glGetString)(JNIEnv*, jclass, jint) = nullptr;

static jstring (*orig_GLES32_glGetString)(JNIEnv*, jclass, jint) = nullptr;

static jstring my_GLES20_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR)   return env->NewStringUTF(spoofed_gl_vendor());
    if (name == GL_RENDERER) return env->NewStringUTF(spoofed_gl_renderer());
    if (orig_GLES20_glGetString) return orig_GLES20_glGetString(env, clazz, name);
    return nullptr;
}

static jstring my_GLES30_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR)   return env->NewStringUTF(spoofed_gl_vendor());
    if (name == GL_RENDERER) return env->NewStringUTF(spoofed_gl_renderer());
    if (orig_GLES30_glGetString) return orig_GLES30_glGetString(env, clazz, name);
    return nullptr;
}

static jstring my_GLES31_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR)   return env->NewStringUTF(spoofed_gl_vendor());
    if (name == GL_RENDERER) return env->NewStringUTF(spoofed_gl_renderer());
    if (orig_GLES31_glGetString) return orig_GLES31_glGetString(env, clazz, name);
    return nullptr;
}

static jstring my_GLES32_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR)   return env->NewStringUTF(spoofed_gl_vendor());
    if (name == GL_RENDERER) return env->NewStringUTF(spoofed_gl_renderer());
    if (orig_GLES32_glGetString) return orig_GLES32_glGetString(env, clazz, name);
    return nullptr;
}
static const unsigned char* my_native_glGetString(unsigned int name) {
    BYTEHOOK_STACK_SCOPE();
    if (name == GL_VENDOR) {
        return reinterpret_cast<const unsigned char*>(spoofed_gl_vendor());
    }
    if (name == GL_RENDERER) {
        return reinterpret_cast<const unsigned char*>(spoofed_gl_renderer());
    }
    return BYTEHOOK_CALL_PREV(my_native_glGetString, name);
}

static void on_native_gl_hooked(bytehook_stub_t, int status, const char* caller_path,
                                const char*, void*, void*, void*) {
    if (status == BYTEHOOK_STATUS_CODE_OK) {
        LOGI("GpuHook: native OpenGL glGetString hook attached in '%s'", caller_path ? caller_path : "?");
    } else if (status != BYTEHOOK_STATUS_CODE_NOSYM) {
        LOGW("GpuHook: native OpenGL glGetString hook failed in '%s' (status=%d)",
             caller_path ? caller_path : "?", status);
    }
}

static void my_vkGetPhysicalDeviceProperties(void* physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    BYTEHOOK_STACK_SCOPE();
    BYTEHOOK_CALL_PREV(my_vkGetPhysicalDeviceProperties, physicalDevice, pProperties);
    if (pProperties) {
        std::strncpy(pProperties->deviceName, spoofed_gl_renderer(), VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);
        pProperties->deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1] = '\0';
        pProperties->vendorID = spoofed_vk_vendor_id();
        pProperties->deviceID = spoofed_vk_device_id();
    }
}

static void my_vkGetPhysicalDeviceProperties2(void* physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    BYTEHOOK_STACK_SCOPE();
    BYTEHOOK_CALL_PREV(my_vkGetPhysicalDeviceProperties2, physicalDevice, pProperties);
    if (pProperties) {
        std::strncpy(pProperties->properties.deviceName, spoofed_gl_renderer(), VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);
        pProperties->properties.deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1] = '\0';
        pProperties->properties.vendorID = spoofed_vk_vendor_id();
        pProperties->properties.deviceID = spoofed_vk_device_id();
    }
}

static void on_native_vk_hooked(bytehook_stub_t, int status, const char* caller_path,
                                const char* sym_name, void*, void*, void*) {
    if (status == BYTEHOOK_STATUS_CODE_OK) {
        LOGI("GpuHook: Vulkan %s hook attached in '%s'", sym_name, caller_path ? caller_path : "?");
    } else if (status != BYTEHOOK_STATUS_CODE_NOSYM) {
        LOGW("GpuHook: Vulkan %s hook failed in '%s' (status=%d)",
             sym_name, caller_path ? caller_path : "?", status);
    }
}

static bool register_native_hook(const char* api, const char* symbol, void* proxy, bytehook_hooked_t callback) {
    if (bytehook_hook_all(nullptr, symbol, proxy, callback, nullptr) == nullptr) {
        LOGW("GpuHook: %s %s registration failed", api, symbol);
        return false;
    }
    LOGI("GpuHook: %s %s registered", api, symbol);
    return true;
}

bool GpuHook::onEnable(const Context& ctx) {
    JNIEnv* env = ctx.getEnv();
    zygisk::Api* api = ctx.getApi();
    if (!env || !api) {
        LOGE("GpuHook: JNIEnv or Api is null");
        return false;
    }

    bool anyJniHooked = false;
    JNINativeMethod gles20_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", reinterpret_cast<void*>(my_GLES20_glGetString)}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES20", gles20_methods, 1);
    if (gles20_methods[0].fnPtr) {
        orig_GLES20_glGetString = reinterpret_cast<jstring (*)(JNIEnv*, jclass, jint)>(gles20_methods[0].fnPtr);
        anyJniHooked = true;
    } else {
        LOGW("GpuHook: GLES20 glGetString hook target not found");
    }
    JNINativeMethod gles30_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", reinterpret_cast<void*>(my_GLES30_glGetString)}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES30", gles30_methods, 1);
    if (gles30_methods[0].fnPtr) {
        orig_GLES30_glGetString = reinterpret_cast<jstring (*)(JNIEnv*, jclass, jint)>(gles30_methods[0].fnPtr);
        anyJniHooked = true;
    } else {
        LOGW("GpuHook: GLES30 glGetString hook target not found");
    }
    JNINativeMethod gles31_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", reinterpret_cast<void*>(my_GLES31_glGetString)}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES31", gles31_methods, 1);
    if (gles31_methods[0].fnPtr) {
        orig_GLES31_glGetString = reinterpret_cast<jstring (*)(JNIEnv*, jclass, jint)>(gles31_methods[0].fnPtr);
        anyJniHooked = true;
    }
    JNINativeMethod gles32_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", reinterpret_cast<void*>(my_GLES32_glGetString)}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES32", gles32_methods, 1);
    if (gles32_methods[0].fnPtr) {
        orig_GLES32_glGetString = reinterpret_cast<jstring (*)(JNIEnv*, jclass, jint)>(gles32_methods[0].fnPtr);
        anyJniHooked = true;
    }
    LOGI("GpuHook: profile vendor='%s' renderer='%s'; Java OpenGL ES hook installed=%d",
         spoofed_gl_vendor(), spoofed_gl_renderer(), anyJniHooked ? 1 : 0);

    const int initStatus = bytehook_init(BYTEHOOK_MODE_AUTOMATIC, false);
    if (initStatus != BYTEHOOK_STATUS_CODE_OK) {
        LOGE("GpuHook: ByteHook %s initialization failed (status=%d); native OpenGL and Vulkan disabled",
             bytehook_get_version(), initStatus);
        return anyJniHooked;
    }
    LOGI("GpuHook: ByteHook %s initialized (status=%d)", bytehook_get_version(), initStatus);

    const bool nativeGl = register_native_hook("native OpenGL", "glGetString",
        reinterpret_cast<void*>(my_native_glGetString), on_native_gl_hooked);
    const bool vulkan = register_native_hook("Vulkan", "vkGetPhysicalDeviceProperties",
        reinterpret_cast<void*>(my_vkGetPhysicalDeviceProperties), on_native_vk_hooked);
    const bool vulkan2 = register_native_hook("Vulkan", "vkGetPhysicalDeviceProperties2",
        reinterpret_cast<void*>(my_vkGetPhysicalDeviceProperties2), on_native_vk_hooked);
    const bool vulkan2Khr = register_native_hook("Vulkan", "vkGetPhysicalDeviceProperties2KHR",
        reinterpret_cast<void*>(my_vkGetPhysicalDeviceProperties2), on_native_vk_hooked);
    return anyJniHooked || nativeGl || vulkan || vulkan2 || vulkan2Khr;
}
REGISTER_HOOK(GpuHook);
}
