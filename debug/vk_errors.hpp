//
// file : vk_errors.hpp
// in : file:///home/tim/projects/hydra/hydra/tools/debug/vk_errors.hpp
//
// created by : Timothée Feuillet
// date: Tue Apr 26 2016 13:32:21 GMT+0200 (CEST)
//
//
// Copyright (c) 2016 Timothée Feuillet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#ifndef __N_6139159572450221341_1885311108_VK_ERRORS_HPP__
#define __N_6139159572450221341_1885311108_VK_ERRORS_HPP__

#include <string>
#include <sstream>
#include <vulkan/vulkan.h>

namespace neam
{
  namespace debug
  {
    namespace errors
    {
      namespace internal
      {
        struct vk_error_entry
        {
          VkResult code;
          std::string code_name;
          std::string description;
        };

        // thanks to https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/xhtml/vkspec.html#fundamentals-returncodes
#define TP_MK_ENTRY(code, descr)  {code, #code, descr}
        vk_error_entry vk_error_table[] =
        {
          TP_MK_ENTRY(VK_SUCCESS, "Command successfully completed"),
          TP_MK_ENTRY(VK_NOT_READY, "A fence or query has not yet completed"),
          TP_MK_ENTRY(VK_TIMEOUT, "A wait operation has not completed in the specified time"),
          TP_MK_ENTRY(VK_EVENT_SET, "An event is signaled"),
          TP_MK_ENTRY(VK_EVENT_RESET, "An event is unsignaled"),
          TP_MK_ENTRY(VK_INCOMPLETE, "A return array was too small for the result"),
          TP_MK_ENTRY(VK_ERROR_OUT_OF_HOST_MEMORY, "A host memory allocation has failed"),
          TP_MK_ENTRY(VK_ERROR_OUT_OF_DEVICE_MEMORY, "A device memory allocation has failed"),
          TP_MK_ENTRY(VK_ERROR_INITIALIZATION_FAILED, "Initialization of an object could not be completed for implementation-specific reasons"),
          TP_MK_ENTRY(VK_ERROR_DEVICE_LOST, "The logical or physical device has been lost"),
          TP_MK_ENTRY(VK_ERROR_MEMORY_MAP_FAILED, "Mapping of a memory object has failed"),
          TP_MK_ENTRY(VK_ERROR_LAYER_NOT_PRESENT, "A requested layer is not present or could not be loaded"),
          TP_MK_ENTRY(VK_ERROR_EXTENSION_NOT_PRESENT, "A requested extension is not supported"),
          TP_MK_ENTRY(VK_ERROR_FEATURE_NOT_PRESENT, "A requested feature is not supported"),
          TP_MK_ENTRY(VK_ERROR_INCOMPATIBLE_DRIVER, "The requested version of Vulkan is not supported by the driver or is otherwise incompatible for implementation-specific reasons"),
          TP_MK_ENTRY(VK_ERROR_TOO_MANY_OBJECTS, "Too many objects of the type have already been created"),
          TP_MK_ENTRY(VK_ERROR_FORMAT_NOT_SUPPORTED, "A requested format is not supported on this device"),
          TP_MK_ENTRY(VK_ERROR_SURFACE_LOST_KHR, " A surface is no longer available"),
          TP_MK_ENTRY(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR, "The requested window is already connected to a VkSurfaceKHR, or to some other non-Vulkan API"),
          TP_MK_ENTRY(VK_SUBOPTIMAL_KHR, "A swapchain no longer matches the surface properties exactly, but can still be used to present to the surface successfully"),
          TP_MK_ENTRY(VK_ERROR_OUT_OF_DATE_KHR, "A surface has changed in such a way that it is no longer compatible with the swapchain, and further presentation requests using the swapchain will fail. Applications must query the new surface properties and recreate their swapchain if they wish to continue presenting to the surface"),
          TP_MK_ENTRY(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR, "The display used by a swapchain does not use the same presentable image layout, or is incompatible in a way that prevents sharing an image"),
          TP_MK_ENTRY(VK_ERROR_VALIDATION_FAILED_EXT, "If the application returns VK_TRUE from it's callback and the api call being aborted returns a VkResult, the layer will return VK_ERROR_VALIDATION_FAILED_EXT."),
//          TP_MK_ENTRY(VK_ERROR_INVALID_SHADER_NV, "---"),
        };
#undef TP_MK_ENTRY
      }

      // errors from the vulkan API
      template<typename T>
      struct vulkan_errors
      {
        static bool is_error(VkResult code)
        {
          if ((int)code >= 0)
            return false;
          for (auto &it : internal::vk_error_table)
          {
            if (it.code == code)
              return true;
          }
          return false;
        }

        static bool exists(VkResult code)
        {
          if ((int)code >= 0)
            return true;
          return is_error(code);
        }

        static std::string get_code_name(VkResult code)
        {
          for (const auto &it : internal::vk_error_table)
          {
            if (it.code == code)
              return it.code_name;
          }
          return "unknow error";
        }

        static std::string get_description(VkResult code)
        {
          for (const auto &it : internal::vk_error_table)
          {
            if (it.code == code)
              return it.description;
          }
          return "unknow error";
        }

        static std::string generate_exception_message(VkResult code, const std::string &message) noexcept
        {
          return get_code_name(code) + ": " + message;
        }
      };
    } // namespace errors
  } // namespace debug
} // namespace neam

#endif // __N_6139159572450221341_1885311108_VK_ERRORS_HPP__

