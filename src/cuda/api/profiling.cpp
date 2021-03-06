#include <cuda/api/profiling.hpp>
#include <cuda/api/error.hpp>

#include <cuda_profiler_api.h>
#include <nvToolsExt.h>
#include <nvToolsExtCudaRt.h>

#include <mutex>

#ifdef __unix__
#include <pthread.h>
#else
#ifdef _WIN32
#include <processthreadsapi.h>
#endif // _WIN32
#endif // __unix__

namespace cuda {
namespace profiling {

namespace mark {

namespace detail {
static std::mutex profiler_mutex; // To prevent multiple threads from accessing the profiler simultaneously
}

void point(const std::string& description, color_t color)
{
	std::lock_guard<std::mutex> { detail::profiler_mutex };
	// logging?
	nvtxEventAttributes_t eventAttrib = {0};
	eventAttrib.version = NVTX_VERSION;
	eventAttrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
	eventAttrib.colorType = NVTX_COLOR_ARGB;
	eventAttrib.color = color;
	eventAttrib.messageType = NVTX_MESSAGE_TYPE_ASCII;
	eventAttrib.message.ascii = description.c_str();
	nvtxMarkEx(&eventAttrib);
}

range::handle_t range_start(
	const std::string& description, ::cuda::profiling::range::Type type, color_t color)
{
	std::lock_guard<std::mutex> { detail::profiler_mutex };
	nvtxEventAttributes_t range_attributes;
	range_attributes.version   = NVTX_VERSION;
	range_attributes.size      = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
	range_attributes.colorType = NVTX_COLOR_ARGB;
	range_attributes.color     = color;
	range_attributes.messageType = NVTX_MESSAGE_TYPE_ASCII;
	range_attributes.message.ascii = description.c_str();
	nvtxRangeId_t range_handle = nvtxRangeStartEx(&range_attributes);
	static_assert(std::is_same<range::handle_t, nvtxRangeId_t>::value,
		"range::handle_t must be the same type as nvtxRangeId_t - but isn't.");
	return range_handle;
}

void range_end(range::handle_t range_handle)
{
	static_assert(std::is_same<range::handle_t, nvtxRangeId_t>::value,
		"range::handle_t must be the same type as nvtxRangeId_t - but isn't.");
	nvtxRangeEnd(range_handle);
}

} // namespace mark


scoped_range_marker::scoped_range_marker(const std::string& description, profiling::range::Type type)
{
	range = profiling::mark::range_start(description);
}

scoped_range_marker::~scoped_range_marker()
{
	// TODO: Can we check the range for validity somehow?
	profiling::mark::range_end(range);
}

void start()
{
	auto status = cudaProfilerStart();
	throw_if_error(status, "Starting to profile");
}

void stop()
{
	auto status = cudaProfilerStop();
	throw_if_error(status, "Starting to profile");
}

void name_host_thread(uint32_t thread_id, const std::string& name)
{
	nvtxNameOsThreadA(thread_id, name.c_str());
}


#if defined(__unix__) || defined(_WIN32)
void name_host_thread(uint32_t thread_id, const std::wstring& name)
{
	auto this_thread_s_native_handle =
#ifdef __unix__
		::pthread_self();
#else
		::GetCurrentThreadId();
#endif // __unix__
	nvtxNameOsThreadW(this_thread_s_native_handle, name.c_str());
}

void name_this_thread(const std::string& name)
{
	auto this_thread_s_native_handle =
#ifdef __unix__
		::pthread_self();
#else
		::GetCurrentThreadId();
#endif // __unix__
	name_host_thread(this_thread_s_native_handle, name);
}

#endif

/*
void name_this_thread(const std::wstring& name)
{
	name_host_thread(std::thread::native_handle(std::this_thread::get_id()), name);
}
*/

} // namespace profiling
} // namespace cuda

