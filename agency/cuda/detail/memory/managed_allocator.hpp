#pragma once

#include <agency/detail/config.hpp>
#include <agency/cuda/device.hpp>
#include <thrust/system_error.h>
#include <thrust/system/cuda/error.h>
#include <cuda_runtime.h>

namespace agency
{
namespace cuda
{
namespace detail
{


template<class T>
class managed_allocator
{
  private:
    template<class> friend class managed_allocator;

    device_id device_;

    const device_id& device() const
    {
      return device_;
    }

  public:
    using value_type = T;

    managed_allocator()
      : device_()
    {
      auto devices = all_devices();

      if(!devices.empty())
      {
        device_ = devices[0];
      }
    }

    managed_allocator(const managed_allocator&) = default;

    template<class U>
    managed_allocator(const managed_allocator<U>& other)
      : device_(other.device_)
    {}
  
    value_type* allocate(size_t n)
    {
      // switch to our device
      scoped_current_device set_current_device(device());

      value_type* result = nullptr;
  
      cudaError_t error = cudaMallocManaged(&result, n * sizeof(T), cudaMemAttachGlobal);
  
      if(error != cudaSuccess)
      {
        throw thrust::system_error(error, thrust::cuda_category(), "managed_allocator::allocate(): cudaMallocManaged");
      }
  
      return result;
    }
  
    void deallocate(value_type* ptr, size_t)
    {
      // switch to our device
      scoped_current_device set_current_device(device());

      cudaError_t error = cudaFree(ptr);
  
      if(error != cudaSuccess)
      {
        throw thrust::system_error(error, thrust::cuda_category(), "managed_allocator::deallocate(): cudaFree");
      }
    }

    template<class U, class... Args>
    void construct(U* ptr, Args&&... args)
    {
      // we need to synchronize with all devices before touching the ptr
      detail::wait(all_devices());

      new(ptr) U(std::forward<Args>(args)...);
    }
};


} // end detail
} // end cuda
} // end agency

