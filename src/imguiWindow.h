#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_vulkan.h"
#include "vulkan/vulkan.h"

#ifdef _MSC_VER
#include <windows.h>
#endif

#define CIMGUI_USE_VULKAN
#define CIMGUI_USE_SDL
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

#ifdef IMGUI_HAS_IMSTR
#define igBegin igBegin_Str
#define igSliderFloat igSliderFloat_Str
#define igCheckbox igCheckbox_Str
#define igColorEdit3 igColorEdit3_Str
#define igButton igButton_Str
#endif

#define IM_UNUSED(_VAR)  ((void)(_VAR))
#define IM_ASSERT(_EXPR) assert(_EXPR)
#define IM_ARRAYSIZE(_ARR) ((int)(sizeof(_ARR) / sizeof(*(_ARR))))

static VkAllocationCallbacks* g_Allocator = NULL;
static VkInstance               g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 g_Device = VK_NULL_HANDLE;
static uint32_t                 g_QueueFamily = (uint32_t)-1;
static VkQueue                  g_Queue = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;
typedef struct imgui_info_t imgui_info_t;

static void check_vk_result(VkResult err);

static void SetupVulkan(const char** extensions, uint32_t extensions_count);

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height);

static void CleanupVulkan();

static void CleanupVulkanWindow();

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data);

static void FramePresent(ImGui_ImplVulkanH_Window* wd);

// Imgui window initialization
imgui_info_t* SetUpImgui(heap_t* heap);

// Rendering the UI
// Should be used in the main render loop
void DrawImgui(imgui_info_t* imgui_info);

// Garbage cleaning
void DestoryImgui(imgui_info_t* imgui_info);