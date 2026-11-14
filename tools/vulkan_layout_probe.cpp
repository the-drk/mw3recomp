// Asset-free Vulkan diagnostic: isolate driver initialization from guest execution.
#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "asset-free-layout-probe";
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    VkInstance instance{};
    VkResult result = vkCreateInstance(&info, nullptr, &instance);
    std::printf("vkCreateInstance: %d\n", result);
    if (result != VK_SUCCESS) return 1;
    uint32_t count = 0;
    result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (result != VK_SUCCESS || !count) return 2;
    std::vector<VkPhysicalDevice> devices(count);
    result = vkEnumeratePhysicalDevices(instance, &count, devices.data());
    if (result != VK_SUCCESS) return 2;
    VkPhysicalDevice physical = devices[0];
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical, &properties);
    std::printf("device: %s, driver: %u\n", properties.deviceName, properties.driverVersion);
    uint32_t families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &families, nullptr);
    std::vector<VkQueueFamilyProperties> queues(families);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &families, queues.data());
    uint32_t family = 0;
    while (family < families && !(queues[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) ++family;
    if (family == families) return 3;
    float priority = 1;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queue;
    VkDevice device{};
    result = vkCreateDevice(physical, &deviceInfo, nullptr, &device);
    std::printf("vkCreateDevice: %d\n", result);
    if (result != VK_SUCCESS) return 4;
    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorSetLayout layout{};
    std::puts("creating empty descriptor set layout");
    result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout);
    std::printf("vkCreateDescriptorSetLayout: %d\n", result);
    if (result == VK_SUCCESS) vkDestroyDescriptorSetLayout(device, layout, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    return result == VK_SUCCESS ? 0 : 5;
}
