#pragma once

#include <agency/detail/config.hpp>
#include <agency/detail/control_structures/bind.hpp>
#include <agency/execution/executor/executor_traits.hpp>
#include <agency/execution/executor/customization_points/async_execute.hpp>
#include <agency/execution/executor/parallel_executor.hpp>
#include <agency/detail/type_traits.hpp>
#include <utility>

namespace agency
{


template<class Executor, class Function, class... Args>
__AGENCY_ANNOTATION
executor_future_t<
  Executor,
  detail::result_of_t<
    typename std::decay<Function&&>::type(typename std::decay<Args&&>::type...)
  >
>
async(Executor& exec, Function&& f, Args&&... args)
{
  auto g = detail::bind(std::forward<Function>(f), std::forward<Args>(args)...);

  return agency::async_execute(exec, std::move(g));
}


template<class Function, class... Args>
executor_future_t<
  parallel_executor,
  detail::result_of_t<
    typename std::decay<Function&&>::type(typename std::decay<Args&&>::type...)
  >
>
  async(Function&& f, Args&&... args)
{
  parallel_executor exec;
  return agency::async(exec, std::forward<Function>(f), std::forward<Args>(args)...);
}


} // end agency

