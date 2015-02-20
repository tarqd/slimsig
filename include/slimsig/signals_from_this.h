//
//  signals_from_this.h
//  slimsig
//
//  Created by Christopher Tarquini on 2/8/15.
//
//

#ifndef slimsig_signals_from_this_h
#define slimsig_signals_from_this_h
#include "slimsig.h"
#include <tuple>

namespace slimsig {
  template <class T>
  struct has_name_tag_helper {
    template<class U> static std::true_type check(typename U::name_tag*);
    template<class U> static std::false_type check(...);
    static constexpr bool value = decltype(check<T>(nullptr))::value;
  };
  template <class T>
  struct has_name_tag : std::integral_constant<bool, has_name_tag_helper<T>::value> {};
  
  template <class First, class Second>
  struct type_pair {
    using first = First;
    using second = Second;
    
  };
  template <class T>
  struct is_type_pair_helper {
    template <class T1, class T2> static std::true_type check(type_pair<T1, T2>* ptr);
    template <class T1> static std::false_type check(T1*);
    static constexpr bool value = decltype(check((T*)(0)))::value;
  };
  
  template <class T>
  using is_type_pair = std::integral_constant<bool, is_type_pair_helper<T>::value>;
  
  template <class T, bool IsTag = is_type_pair<typename T::name_tag>::value>
  struct name_tag_traits_helper;
  template <class T>
  struct name_tag_traits_helper<T, true> {
    using type = typename T::name_tag;
  };
  template <class T>
  struct name_tag_traits_helper<T, false> {
    using type = type_pair<typename T::name_tag, T>;
  };

  template <class T, bool IsTypePair = is_type_pair<T>::value, bool HasNameTag = has_name_tag<T>::value>
  struct base_name_tag_traits;
  
  template <class T>
  struct base_name_tag_traits<T, true, false>  {
    using type = T;
  };
  template <class T>
  struct base_name_tag_traits<T, false, true> {
    using type = typename std::conditional<is_type_pair<T>::value, typename T::name_tag, type_pair<typename T::name_tag, T>>::type;
  };
  
  template <class T>
  struct name_tag_traits : base_name_tag_traits<T>::type {
    using tag_type = typename base_name_tag_traits<T>::type::first;
    using value_type = typename base_name_tag_traits<T>::type::second;
    using pair_type = typename base_name_tag_traits<T>::type;
  };

  template <class T, class U, class... Types>
  constexpr std::size_t do_type_count(std::size_t count = 0) {
    return do_type_count<T, Types...>(count + std::is_same<T, U>::value ? 1 : 0);
  };
  template <class T>
  constexpr std::size_t do_type_count(std::size_t count = 0) { return count; };

  template <class T, class... Types>
  constexpr std::size_t type_count() {
    return do_type_count<T, Types...>();
  };
  
  template <class... Types>
  struct tag_list {
    template <class T, class... Rest>
    struct get_tag {
      using traits = name_tag_traits<T>;
    };
  };
  
  template <class T>
  using name_tag_t = typename name_tag_traits<T>::tag_type;
  template <class T>
  using name_tag_value_t = typename name_tag_traits<T>::value_type;
  template <class Needle>
  constexpr size_t index_of_impl(size_t index, size_t end) {
      return end;
  };
  template <class Needle, class T, class... Haystack>
  constexpr size_t index_of_impl(size_t index, size_t end) {
    return std::is_same<Needle, T>::value ? index : index_of_impl<Needle, Haystack...>(index + 1, end);
  };
    
    template <class Needle, class... Haystack>
    static constexpr size_t index_of() {
        return index_of_impl<Needle, Haystack...>(0, sizeof...(Haystack) + 1);
    };
  template <class... TypePairs>
  class tagged_tuple : public std::tuple<name_tag_value_t<TypePairs>...> {
    public:
    using tag_type = std::tuple<name_tag_t<TypePairs>...>;
    using value_type = std::tuple<name_tag_value_t<TypePairs>...>;
    using value_type::value_type;
    using value_type::swap;
    using value_type::operator=;
  };
  template <class Name, class... TypePairs>
  auto get(tagged_tuple<TypePairs...>& tuple) ->
    typename std::tuple_element<index_of<Name, name_tag_t<TypePairs>...>(),
                       typename tagged_tuple<TypePairs...>::value_type>::type&
  {
    return std::get<index_of<Name, name_tag_t<TypePairs>...>()>(tuple);
  };
  template <class Name, class... TypePairs>
  auto get(const tagged_tuple<TypePairs...>& tuple) ->
    const typename std::tuple_element<index_of<Name, name_tag_t<TypePairs>...>(),
                       typename tagged_tuple<TypePairs...>::value_type>::type&
  {
    return std::get<index_of<Name, name_tag_t<TypePairs>...>()>(tuple);
  };
  template <class Name, class... TypePairs>
  auto get(tagged_tuple<TypePairs...>&& tuple) ->
    typename std::tuple_element<index_of<Name, name_tag_t<TypePairs>...>(),
                       typename tagged_tuple<TypePairs...>::value_type>::type&&
  {
    return std::get<index_of<Name, name_tag_t<TypePairs>...>()>(tuple);
  };

  
  
};
#endif
