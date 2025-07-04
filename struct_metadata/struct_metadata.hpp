//
// created by : Timothée Feuillet
// date: 2021-12-11
//
//
// Copyright (c) 2021 Timothée Feuillet
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

#pragma once

#include <type_traits>
#include <tuple>
#include "../ct_string.hpp"
#include "../ct_list.hpp"

#define N_METADATA_STRUCT_DECL(Struct) struct n_metadata_member_definitions<Struct>

#define N_METADATA_STRUCT(Struct) template<> struct n_metadata_member_definitions<Struct> : public ::neam::metadata::internal::member_definition_base<Struct>


#define N_METADATA_STRUCT_TPL(Struct, ...) struct n_metadata_member_definitions<Struct<__VA_ARGS__>> : public ::neam::metadata::internal::member_definition_base<Struct<__VA_ARGS__>>

#define N_DECLARE_STRUCT_TYPE_TPL(Struct, ...) using struct_type = Struct<__VA_ARGS__>

#define N_MEMBER_DEF_EX(Struct, Member, ...) ::neam::metadata::internal::member_definition<decltype(Struct::Member), offsetof(Struct, Member), #Member, struct_type __VA_OPT__(,) __VA_ARGS__>

/// \brief Entry in the member_list type of serializable structs
#define N_MEMBER_DEF(Member, ...) ::neam::metadata::internal::member_definition<decltype(struct_type::Member), offsetof(struct_type, Member), #Member, struct_type __VA_OPT__(,) __VA_ARGS__>

namespace neam::metadata::internal
{
  template<typename T>
  struct member_definition_base
  {
    using struct_type = std::remove_cv_t<T>;
  };

  template<typename Type, size_t Offset, ct::string_holder Name, typename ParentType, auto... ExtraMetadata>
  struct member_definition
  {
    static constexpr size_t size = sizeof(Type);
    static constexpr size_t offset = Offset;
    using type = std::remove_cv_t<Type>;
    using raw_type = Type;
    using parent_type = ParentType; // parent struct / class type
    static constexpr bool is_const = std::is_const_v<Type>;
    static constexpr bool is_volatile = std::is_volatile_v<Type>;
    static constexpr ct::string_holder name = Name;


    struct metadata_t : public decltype(ExtraMetadata)::metadata...
    {
      metadata_t() : decltype(ExtraMetadata)::metadata(ExtraMetadata)... {}
    };

    /// \brief easy way for user to simply access a metadata from code:
    /// usage: type_info.metadata.description ...
    static const inline metadata_t metadata = {};

    using metadata_types = ct::type_list<typename decltype(ExtraMetadata)::metadata...>;

    /// \brief easy way for code to iterate over all the different metadata (see cr::for_each (in container_utils.hpp)):
    /// possible usage: cr::for_each(type_info.metadata_tuple, [](auto&& v) { ... });
    static inline std::tuple<typename decltype(ExtraMetadata)::metadata...> metadata_tuple() { return { ExtraMetadata... }; }

    /// \brief extract the member from the parent type
    static type& extract_from(parent_type& v)
    {
      uint8_t* ptr = reinterpret_cast<uint8_t*>(&v);
      return *reinterpret_cast<type*>(ptr + Offset);
    }
    static const type& extract_from(const parent_type& v)
    {
      const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&v);
      return *reinterpret_cast<const type*>(ptr + Offset);
    }
  };
}

template<typename T> struct n_metadata_member_definitions {static_assert(!std::is_same_v<T, void>, "No member definition for type T");};

template<typename T> using n_metadata_member_list = typename n_metadata_member_definitions<T>::member_list;

namespace neam::metadata
{
  /// \brief Iterate over all the members of a given structure
  template<typename StructType, typename Func>
  constexpr void for_each_member(Func&& fnc)
  {
    [&]<size_t... Indices>(std::index_sequence<Indices...>)
    {
      ([&]<size_t Index>()
      {
        using member = neam::ct::list::get_type<n_metadata_member_list<StructType>, Index>;
        fnc.template operator()<Index, member>();
      } .template operator()<Indices>(), ...);
    } (std::make_index_sequence<neam::ct::list::size<n_metadata_member_list<StructType>>> {});
  }

  /// \brief Call the function on the given member of the structure, return false if the member is not present
  template<typename StructType, typename Func>
  constexpr bool on_member(std::string_view name, Func&& fnc)
  {
    bool found = false;
    for_each_member<StructType>([&]<size_t Index, typename Member>()
    {
      if (found) return;
      if (Member::name.view() != name) return;
      found = true;

      fnc.template operator()<Index, Member>();
    });
    return found;
  }
}

namespace neam::metadata::concepts
{
  /// \brief A struct with metadata allows for an easy and semi-automatic serialization of compound types
  ///
  /// Here is a serializable struct:
  /// \code
  /// struct MyAwesomeStruct
  /// {
  ///   unsigned some_member = 0;
  ///   std::vector< std::map< int, SomeOtherStruct > > stuff;
  ///   raw_data binary_blob;
  /// };
  /// N_METADATA_STRUCT(MyAwesomeStruct)
  /// {
  ///   using member_list = ct::list
  ///   <
  ///     N_MEMBER(some_member),
  ///     N_MEMBER(stuff),
  ///     N_MEMBER(binary_blob),
  ///   >;
  /// };
  /// \endcode
  ///
  /// Here is a serializable template struct:
  /// \code
  /// template<typename T, uint32_t V>
  /// struct MyAwesomeStruct
  /// {
  ///   unsigned some_member = V;
  ///   std::vector< std::map< T, SomeOtherStruct > > stuff;
  ///   raw_data binary_blob;
  /// };
  ///
  /// template<typename T, uint32_t V>
  /// N_METADATA_STRUCT_TPL(MyAwesomeStruct, T, V)
  /// {
  ///   N_DECLARE_STRUCT_TYPE_TPL(MyAwesomeStruct, T, V);
  ///   using member_list = ct::list
  ///   <
  ///     N_MEMBER(some_member),
  ///     N_MEMBER(stuff),
  ///     N_MEMBER(binary_blob),
  ///   >;
  /// };
  /// \endcode
  template<typename T>
  concept StructWithMetadata = requires(T v)
  {
    ct::list::size<typename n_metadata_member_definitions<T>::member_list>;
  };
}

#include "extra_metadata.hpp"
