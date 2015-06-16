#pragma once

#include <agency/detail/config.hpp>
#include <agency/future.hpp>
#include <agency/new_executor_traits.hpp>
#include <agency/detail/index_cast.hpp>
#include <type_traits>
#include <utility>
#include <cassert>

namespace agency
{
namespace detail
{
namespace new_executor_traits_detail
{


template<size_t... Indices, class Executor, class Function, class TupleOfFutures, class... Types>
typename new_executor_traits<Executor>::template future<
  detail::when_all_execute_and_select_result_t<
    detail::index_sequence<Indices...>,
    typename std::decay<TupleOfFutures>::type
  >
>
  multi_agent_when_all_execute_and_select_with_shared_inits(std::true_type, Executor& ex, Function f, typename new_executor_traits<Executor>::shape_type shape, TupleOfFutures&& futures, Types&&... shared_inits)
{
  return ex.template when_all_execute_and_select<Indices...>(f, shape, std::forward<TupleOfFutures>(futures), std::forward<Types>(shared_inits)...);
} // end multi_agent_when_all_execute_and_select_with_shared_inits()


template<class Function, class Shape>
struct multi_agent_when_all_execute_and_select_with_shared_inits_functor
{
  mutable Function f;
  Shape shape;

  template<size_t depth, class AgentIndex>
  __AGENCY_ANNOTATION
  size_t rank_in_group(const AgentIndex& idx) const
  {
    // to compute the rank of an index at a particular depth,
    // first prepend 0 (1) to idx (shape) to represent an index of the root group (it has none otherwise)
    // XXX seems like index_cast() should just do the right thing for empty indices
    //     it would correspond to a single-agent task
    auto augmented_idx   = detail::tuple_prepend(detail::wrap_scalar(idx), size_t{0});
    auto augmented_shape = detail::tuple_prepend(detail::wrap_scalar(shape), size_t{1});
    
    // take the first depth+1 (plus one because we prepended 1) indices of the index & shape and do an index_cast to size_t
    return detail::index_cast<size_t>(detail::tuple_take<depth+1>(augmented_idx),
                                      detail::tuple_take<depth+1>(augmented_shape),
                                      detail::shape_size(detail::tuple_take<depth+1>(augmented_shape)));
  }

  template<size_t... ContainerIndices, class AgentIndex, class TupleOfContainers, class... Types>
  __AGENCY_ANNOTATION
  void impl(detail::index_sequence<ContainerIndices...>, AgentIndex&& agent_idx, TupleOfContainers&& shared_arg_containers, Types&... past_args) const
  {
    f(std::forward<AgentIndex>(agent_idx),                                                             // pass the agent index
      past_args...,                                                                                    // pass the arguments coming in from futures
      std::get<ContainerIndices>(shared_arg_containers)[rank_in_group<ContainerIndices>(agent_idx)]... // pass the arguments coming in from shared parameters
    );
  }

  template<class Index, class TupleOfContainers, class... Types>
  __AGENCY_ANNOTATION
  void operator()(Index&& idx, TupleOfContainers& shared_arg_containers, Types&... past_args) const
  {
    static const size_t num_containers = std::tuple_size<TupleOfContainers>::value;
    impl(detail::make_index_sequence<num_containers>(), std::forward<Index>(idx), shared_arg_containers, past_args...);
  }
};


template<size_t depth, class Shape>
size_t number_of_groups_at_depth(const Shape& shape)
{
  // to compute the number of groups at a particular depth given a shape,
  // take the first depth elements of shape and return shape_size
  return detail::shape_size(detail::tuple_take<depth>(shape));
}


template<class T, class Executor>
using shared_parameter_container = typename new_executor_traits<Executor>::template container<T>;


template<class Executor, class T>
shared_parameter_container<T,Executor> make_shared_parameter_container(Executor&, size_t n, const T& shared_init)
{
  return shared_parameter_container<T,Executor>(n, shared_init);
}


template<size_t... Indices, class Executor, class... Types>
detail::tuple<shared_parameter_container<Types,Executor>...>
  make_tuple_of_shared_parameter_containers(detail::index_sequence<Indices...>, Executor& ex, typename new_executor_traits<Executor>::shape_type shape, const Types&... shared_inits)
{
  return detail::make_tuple(make_shared_parameter_container(ex, number_of_groups_at_depth<Indices>(shape), shared_inits)...);
}


template<class Executor, class... Types>
detail::tuple<
  shared_parameter_container<
    typename std::decay<Types>::type,
    Executor
  >...
>
  make_tuple_of_shared_parameter_containers(Executor& ex, typename new_executor_traits<Executor>::shape_type shape, Types&&... shared_inits)
{
  return make_tuple_of_shared_parameter_containers(detail::make_index_sequence<sizeof...(shared_inits)>(), ex, shape, std::forward<Types>(shared_inits)...);
}


template<size_t... Indices, class Executor, class Function, class TupleOfFutures, class... Types>
typename new_executor_traits<Executor>::template future<
  detail::when_all_execute_and_select_result_t<
    detail::index_sequence<Indices...>,
    typename std::decay<TupleOfFutures>::type
  >
>
  multi_agent_when_all_execute_and_select_with_shared_inits(std::false_type, Executor& ex, Function f, typename new_executor_traits<Executor>::shape_type shape, TupleOfFutures&& futures, Types&&... shared_inits)
{
  // create a tuple of containers holding a shared parameter for each group
  auto shared_param_containers_tuple = make_tuple_of_shared_parameter_containers(ex, shape, std::forward<Types>(shared_inits)...);

  // turn it into a future
  auto shared_param_containers_tuple_fut = new_executor_traits<Executor>::template make_ready_future<decltype(shared_param_containers_tuple)>(ex, std::move(shared_param_containers_tuple));

  // combine the shared parameters with the incoming futures
  // the tuple of containers goes in front of the incoming futures
  auto shared_and_futures = detail::tuple_prepend(std::move(futures), std::move(shared_param_containers_tuple_fut));

  // wrap f with a functor to map container elements to shared parameters
  auto g = multi_agent_when_all_execute_and_select_with_shared_inits_functor<Function, typename new_executor_traits<Executor>::shape_type>{f, shape};

  // add one to the indices to skip the tuple of containers which was prepended to the tuple of futures
  return new_executor_traits<Executor>::template when_all_execute_and_select<(Indices+1)...>(ex, g, shape, std::move(shared_and_futures));
} // end multi_agent_when_all_execute_and_select_with_shared_inits()


template<class IndexSequence, class Executor, class Function, class Shape, class TupleOfFutures, class TypeList>
struct has_multi_agent_when_all_execute_and_select_with_shared_inits_impl;


template<size_t... Indices, class Executor, class Function, class Shape, class TupleOfFutures, class... Types>
struct has_multi_agent_when_all_execute_and_select_with_shared_inits_impl<index_sequence<Indices...>, Executor, Function, Shape, TupleOfFutures, type_list<Types...>>
{
  template<class Executor1,
           class = decltype(
             std::declval<Executor1>().template when_all_execute_and_select<Indices...>(
               std::declval<Function>(),
               std::declval<Shape>(),
               std::declval<TupleOfFutures>(),
               std::declval<Types>()...
             )
           )>
  static std::true_type test(int);

  template<class>
  static std::false_type test(...);

  using type = decltype(test<Executor>(0));
};

template<class IndexSequence, class Executor, class Function, class Shape, class TupleOfFutures, class TypeList>
using has_multi_agent_when_all_execute_and_select_with_shared_inits = typename has_multi_agent_when_all_execute_and_select_with_shared_inits_impl<IndexSequence, Executor, Function, Shape, TupleOfFutures, TypeList>::type;


} // end detail
} // end new_executor_traits_detail


template<class Executor>
template<size_t... Indices, class Function, class TupleOfFutures, class T1, class... Types>
  typename new_executor_traits<Executor>::template future<
    detail::when_all_execute_and_select_result_t<
      detail::index_sequence<Indices...>,
      typename std::decay<TupleOfFutures>::type
    >
  >
  new_executor_traits<Executor>
    ::when_all_execute_and_select(typename new_executor_traits<Executor>::executor_type& ex,
                                  Function f,
                                  typename new_executor_traits<Executor>::shape_type shape,
                                  TupleOfFutures&& futures,
                                  T1&& outer_shared_init,
                                  Types&&... inner_shared_inits)
{
  static_assert(new_executor_traits<Executor>::execution_depth == 1 + sizeof...(Types), "The number of shared initializers must be equal to the executor's execution_depth.");
  
  using check_for_member_function = detail::new_executor_traits_detail::has_multi_agent_when_all_execute_and_select_with_shared_inits<
    detail::index_sequence<Indices...>,
    Executor,
    Function,
    typename new_executor_traits<Executor>::shape_type,
    typename std::decay<TupleOfFutures>::type,
    detail::type_list<T1,Types...>
  >;

  return detail::new_executor_traits_detail::multi_agent_when_all_execute_and_select_with_shared_inits<Indices...>(check_for_member_function(), ex, f, shape, std::forward<TupleOfFutures>(futures), std::forward<T1>(outer_shared_init), std::forward<Types>(inner_shared_inits)...);
} // end new_executor_traits::when_all_execute_and_select()


} // end agency

