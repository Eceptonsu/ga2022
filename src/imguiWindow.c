/*
* This file is insipred by the article
* https://frguthmann.github.io/posts/vulkan_imgui/ as well as
* https://vkguide.dev/docs/extra-chapter/implementing_imgui/
* 
* In addition, the code references
* https://github.com/cimgui/cimgui
*/
#include <assert.h>

#include "heap.h"
#include "imguiWindow.h"

// Some file specific variables
static ImVec4 textColor;
static ImVec2 buttonSize;
static bool doubleCheck = false;

typedef struct imgui_info_t
{
    SDL_Window* window;
    ImGui_ImplVulkanH_Window* wd;
    ImVec4 clearColor;

    // Program specific data
    bool orthoView;
    bool perspView;
    float viewDistance;
    float horizontalPan;
    float verticalPan;
    float viewDistanceP;
    float horizontalPanP;
    float verticalPanP;
    float yaw;
    float pitch;
    float roll;

    int difficulty;
    float playerSpeed;
    ImVec4 playerColor;

    // Game Update
    bool update;
    bool quit;
} imgui_info_t;

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

// Similar to how vulkan is setup in gpu.c
static void SetupVulkan(const char** extensions, uint32_t extensions_count)
{
    VkResult err;

    // Create Vulkan Instance
    VkInstanceCreateInfo inst_create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .enabledExtensionCount = extensions_count,
      .ppEnabledExtensionNames = extensions,
    };

    err = vkCreateInstance(&inst_create_info, g_Allocator, &g_Instance);
    check_vk_result(err);
    IM_UNUSED(g_DebugReport);

    uint32_t gpu_count;
    err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, NULL);
    check_vk_result(err);
    IM_ASSERT(gpu_count > 0);

    VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpu_count);
    err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, gpus);
    check_vk_result(err);

    int use_gpu = 0;
    for (int i = 0; i < (int)gpu_count; i++)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(gpus[i], &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            use_gpu = i;
            break;
        }
    }

    g_PhysicalDevice = gpus[use_gpu];
    free(gpus);

    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, NULL);
    VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(
        sizeof(VkQueueFamilyProperties) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, queues);
    for (uint32_t i = 0; i < count; i++)
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            g_QueueFamily = i;
            break;
        }
    free(queues);
    IM_ASSERT(g_QueueFamily != (uint32_t)-1);

    int device_extension_count = 1;
    const char* device_extensions[] = { "VK_KHR_swapchain" };
    const float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info[1] = {
      [0] .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      [0].queueFamilyIndex = g_QueueFamily,
      [0].queueCount = 1,
      [0].pQueuePriorities = queue_priority,
    };
    VkDeviceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]),
      .pQueueCreateInfos = queue_info,
      .enabledExtensionCount = device_extension_count,
      .ppEnabledExtensionNames = device_extensions,
    };
    err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
    check_vk_result(err);
    vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);

    // Create Descriptor Pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 1000 * IM_ARRAYSIZE(pool_sizes),
      .poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes),
      .pPoolSizes = pool_sizes,
    };
    err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
    check_vk_result(err);
}

// The function creates a separate window from the one created in main
// Dedicated for the imgui, and data transfer will happen between two instances
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface,
    int width, int height)
{
    wd->Surface = surface;

    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, wd->Surface, &res);
    if (res != VK_TRUE)
    {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }

    const VkFormat requestSurfaceImageFormat[] = {
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM
    };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat,
        (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);


    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        g_PhysicalDevice, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));

    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(
        g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator,
        width, height, g_MinImageCount);
}

static void CleanupVulkan()
{
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow()
{
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
    VkResult err;

    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        g_SwapChainRebuild = true;
        return;
    }
    check_vk_result(err);

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
    check_vk_result(err);

    err = vkResetFences(g_Device, 1, &fd->Fence);
    check_vk_result(err);

    err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
    check_vk_result(err);
    VkCommandBufferBeginInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
    check_vk_result(err);
    VkRenderPassBeginInfo rp_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = wd->RenderPass,
      .framebuffer = fd->Framebuffer,
      .renderArea.extent.width = wd->Width,
      .renderArea.extent.height = wd->Height,
      .clearValueCount = 1,
      .pClearValues = &wd->ClearValue,
    };
    vkCmdBeginRenderPass(fd->CommandBuffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer, VK_NULL_HANDLE);

    vkCmdEndRenderPass(fd->CommandBuffer);
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo sub_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &image_acquired_semaphore,
      .pWaitDstStageMask = &wait_stage,
      .commandBufferCount = 1,
      .pCommandBuffers = &fd->CommandBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &render_complete_semaphore,
    };

    err = vkEndCommandBuffer(fd->CommandBuffer);
    check_vk_result(err);
    err = vkQueueSubmit(g_Queue, 1, &sub_info, fd->Fence);
    check_vk_result(err);
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd)
{
    if (g_SwapChainRebuild) return;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &render_complete_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &wd->Swapchain,
      .pImageIndices = &wd->FrameIndex,
    };
    VkResult err = vkQueuePresentKHR(g_Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        g_SwapChainRebuild = true;
        return;
    }
    check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->ImageCount;
}

// The function sets up the imgui in the vulkan window
imgui_info_t* SetUpImgui(heap_t* heap)
{
    imgui_info_t* imgui_info = heap_alloc(heap, sizeof(imgui_info_t), 8);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("failed to init: %s", SDL_GetError());
    }

    /// Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_MOUSE_CAPTURE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("GA 2022 Final Project Imgui User Interface",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 540, 720, window_flags);

    if (window == NULL) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
    }

    imgui_info->window = window;

    // Setup Vulkan
    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, NULL);
    char** extensions = malloc(extensions_count * sizeof(const char*));
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, extensions);
    SetupVulkan(extensions, extensions_count);
    free(extensions);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err;
    if (SDL_Vulkan_CreateSurface(window, g_Instance, &surface) == 0)
    {
        printf("Failed to create Vulkan surface.\n");
    }

    // Create Framebuffers
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    imgui_info->wd = wd;
    SetupVulkanWindow(wd, surface, w, h);

    // Setup imgui
    igCreateContext(NULL);
    ImGuiIO* ioptr = igGetIO();
    ioptr->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo init_info = {
      .Instance = g_Instance,
      .PhysicalDevice = g_PhysicalDevice,
      .Device = g_Device,
      .QueueFamily = g_QueueFamily,
      .Queue = g_Queue,
      .PipelineCache = g_PipelineCache,
      .DescriptorPool = g_DescriptorPool,
      .Subpass = 0,
      .MinImageCount = g_MinImageCount,
      .ImageCount = wd->ImageCount,
      .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
      .Allocator = g_Allocator,
      .CheckVkResultFn = check_vk_result
    };
    ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

    igStyleColorsDark(NULL);

    // Upload Fonts
    // Use any command queue
    VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
    VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

    err = vkResetCommandPool(g_Device, command_pool, 0);
    check_vk_result(err);
    VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    err = vkBeginCommandBuffer(command_buffer, &begin_info);
    check_vk_result(err);

    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

    VkSubmitInfo end_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer,
    };
    err = vkEndCommandBuffer(command_buffer);
    check_vk_result(err);
    err = vkQueueSubmit(g_Queue, 1, &end_info, VK_NULL_HANDLE);
    check_vk_result(err);

    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    // Variable initialization
    textColor.x = 1.0f;
    textColor.y = 0.0f;
    textColor.z = 1.0f;
    textColor.w = 1.0f;

    buttonSize.x = 520;
    buttonSize.y = 0;

    ImVec4 clearColor;
    clearColor.x = 0.45f;
    clearColor.y = 0.55f;
    clearColor.z = 0.60f;
    clearColor.w = 1.00f;
    imgui_info->clearColor = clearColor;

    imgui_info->orthoView = true;
    imgui_info->perspView = false;

    imgui_info->viewDistance = 30.0f;
    imgui_info->horizontalPan = 0.0f;

    imgui_info->viewDistanceP = 20.0f;
    imgui_info->horizontalPanP = 0.0f;
    imgui_info->verticalPanP = 0.0f;

    imgui_info->difficulty = 2;
    imgui_info->playerSpeed = 5.0f;
    ImVec4 playerColor;
    playerColor.x = 0.0f;
    playerColor.y = 0.40f;
    playerColor.z = 0.0f;
    playerColor.w = 1.0f;
    imgui_info->playerColor = playerColor;

    imgui_info->update = false;
    imgui_info->quit = false;

    return imgui_info;
}

void DrawImgui(imgui_info_t* imgui_info)
{
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0)
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
    }

    // Resize swap chain
    if (g_SwapChainRebuild)
    {
        int width, height;
        SDL_GetWindowSize(imgui_info->window, &width, &height);
        if (width > 0 && height > 0)
        {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData,
                g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
    }

    // start imgui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    igNewFrame();

    // show a simple window that we created ourselves.
    {
        ImGuiWindowFlags_ window_flags = 0;
        window_flags |= ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoResize;
        window_flags |= ImGuiWindowFlags_NoCollapse;

        static float f = 0.0f;
        static int counter = 0;

        igBegin("GA 2022 Final Project", NULL, window_flags);

        igText("Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / igGetIO()->Framerate, igGetIO()->Framerate);
        igNewLine();

        igText("Camera Settings");
        // Determine if the camera is orthographic
        igCheckbox("Orthographic", &(imgui_info->orthoView));
        imgui_info->perspView = !(imgui_info->orthoView);

        // Determine if the camera is perpsective
        igSameLine(0.0f, -1.0f);
        igCheckbox("Perspective", &(imgui_info->perspView));
        imgui_info->orthoView = !(imgui_info->perspView);

        if (imgui_info->orthoView) 
        {
            igSliderFloat("View Distance Ortho", &(imgui_info->viewDistance), 30.0f, 50.0f, "%.5f", 0);
            igSliderFloat("Horizontal Pan Ortho", &(imgui_info->horizontalPan), -30.0f, 30.0f, "%.5f", 0);
            igSliderFloat("Vertical Pan Ortho", &(imgui_info->verticalPan), -15.0f, 15.0f, "%.5f", 0);
        }
        else
        {
            igSliderFloat("View Distance Persp", &(imgui_info->viewDistanceP), 20.0f, 50.0f, "%.5f", 0);
            igSliderFloat("Horizontal Pan Persp", &(imgui_info->horizontalPanP), -25.0f, 25.0f, "%.5f", 0);
            igSliderFloat("Vertical Pan Persp", &(imgui_info->verticalPanP), -15.0f, 15.0f, "%.5f", 0);
            igSliderFloat("Yaw", &(imgui_info->yaw), -90.0f, 90.0f, "%.5f", 0);
            igSliderFloat("Pitch", &(imgui_info->pitch), -90.0f, 90.0f, "%.5f", 0);
            igSliderFloat("Roll", &(imgui_info->roll), -90.0f, 90.0f, "%.5f", 0);

        }
        igNewLine();

        int tempDifficulty = imgui_info->difficulty;
        igText("Frogger Game Settings");
        igInputInt("Frogger Game Difficulty", &tempDifficulty, 1, 1, 0);
        igTextColored(textColor, "Should be a number between 1 and 5");
        if (tempDifficulty < 1) tempDifficulty = 1;
        else if (tempDifficulty > 5) tempDifficulty = 5;
        if (tempDifficulty != imgui_info->difficulty) 
        {
            printf("%d %d\n", tempDifficulty, imgui_info->difficulty);
            imgui_info->difficulty = tempDifficulty;
            imgui_info->update = true;
        }

        float tempSpeed = imgui_info->playerSpeed;
        igInputFloat("Player Speed", &tempSpeed, 0.5f, 1.0f, "%.1f", 0);
        if (tempSpeed < 5.0f) tempSpeed = 5.0f;
        else if (tempSpeed > 10.0f) tempSpeed = 10.0f;
        imgui_info->playerSpeed = tempSpeed;
        
        ImVec4 backUpColor = imgui_info->playerColor;
        igColorPicker3("Player Color", (float*)&backUpColor, ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);
        imgui_info->playerColor = backUpColor;
        igNewLine();

        if (igButton("Quit", buttonSize))
        {
            if (doubleCheck) imgui_info->quit = true;
            doubleCheck = true;
        }
        if (doubleCheck)
        {
            if (fmodf((float)igGetTime(), 0.40f) < 0.20f)
            {
                igSameLine(0.0f, -1.0f);
                igText("ARE YOU SURE?");
            }
        }
        
        igEnd();
    }

    // render
    igRender();
    ImDrawData* draw_data = igGetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized)
    {
        imgui_info->wd->ClearValue.color.float32[0] = imgui_info->clearColor.x * imgui_info->clearColor.w;
        imgui_info->wd->ClearValue.color.float32[1] = imgui_info->clearColor.y * imgui_info->clearColor.w;
        imgui_info->wd->ClearValue.color.float32[2] = imgui_info->clearColor.z * imgui_info->clearColor.w;
        imgui_info->wd->ClearValue.color.float32[3] = imgui_info->clearColor.w;
        FrameRender(imgui_info->wd, draw_data);
        FramePresent(imgui_info->wd);
    }
}

void DestoryImgui(imgui_info_t* imgui_info)
{
    VkResult err;

    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);

    CleanupVulkanWindow();
    CleanupVulkan();

    SDL_DestroyWindow(imgui_info->window);
    SDL_Quit();
}