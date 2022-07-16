//
// created by : Timothée Feuillet
// date: 2022-7-15
//
//
// Copyright (c) 2022 Timothée Feuillet
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

#include "../struct_metadata/struct_metadata.hpp"

namespace neam::rle
{
  namespace concepts
  {
    /// \note You may want to match VersionedStruct instead, as it allows migrating old data
    /// \note to keep forward compatibility, non-versionned structs are assumed to be at version 0. Layout change may result is bad behavior.
    template<typename T>
    concept SerializableStruct = metadata::concepts::StructWithMetadata<T>;

    template<typename T>
    concept StructHasPostDeserialize = requires(T v)
    {
      v.post_deserialize();
    };

    /// \brief Versioned structs allows to load old data and migrate it to the new data
    /// It implies the existance of:
    ///  static constexpr uint32_t min_supported_version = 0;
    ///  static constexpr uint32_t current_version = 1; // will be written to data
    ///  using version_list = ct::type_list< MyPreviousAwesomeStruct, MyAwesomeStruct >;
    ///
    ///  // optional, only if current version != min_supported_version
    ///  // each struct (except the first) in the version list must have this function taking a r-value of the previous version
    ///  // (note that the concept does not test this assumption, but it will fail to compile anyway)
    ///  void migrate_from(MyPreviousAwesomeStruct &&prev);
    template<typename T>
    concept VersionedStruct = requires(T v)
    {
      requires SerializableStruct<T>;

      typename std::integral_constant<uint32_t, T::min_supported_version>;
      typename std::integral_constant<uint32_t, T::current_version>;
      ct::list::size<typename T::version_list>;
      requires std::same_as<ct::list::get_type<typename T::version_list, T::current_struct_version - 1>, T>;
    };
  }
}
