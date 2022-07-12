//
// created by : Timothée Feuillet
// date: 2021-12-2
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

#include "helpers.hpp"
#include "enum.hpp"

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

  namespace internal
  {
    template<typename Type, size_t Offset, ct::string_holder Name>
    struct member_definition
    {
      static constexpr size_t size = sizeof(Type);
      static constexpr size_t offset = Offset;
      using type = Type;
      static constexpr ct::string_holder name = Name;
    };
  }

  /// \brief Handle structs (both versionned and not versionned)
  template<concepts::SerializableStruct T>
  struct coder<T>
  {
    static constexpr uint32_t get_version() requires concepts::VersionedStruct<T> { return T::current_version; }
    static constexpr uint32_t get_version() { return 0; }

    static constexpr uint32_t get_min_version() requires concepts::VersionedStruct<T> { return T::min_supported_version; }
    static constexpr uint32_t get_min_version() { return 0; }

    static void post_deserialize(T& v) requires concepts::StructHasPostDeserialize<T> { v.post_deserialize(); }
    static void post_deserialize(T& ) { }

    template<typename MT, size_t Offset>
    static const MT& const_member_at(const T& v)
    {
      const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&v);
      return *reinterpret_cast<const MT*>(ptr + Offset);
    }
    template<typename MT, size_t Offset>
    static MT& member_at(T& v)
    {
      uint8_t* ptr = reinterpret_cast<uint8_t*>(&v);
      return *reinterpret_cast<MT*>(ptr + Offset);
    }

    static void encode(encoder& ec, const T& v, status& st)
    {
      ec.encode<uint32_t>(get_version());
      [&ec, &st, &v]<size_t... Indices>(std::index_sequence<Indices...>)
      {
        ([&ec, &st, &v]<size_t Index>()
        {
          using member = ct::list::get_type<typename n_metadata_member_definitions<T>::member_list, Index>;
          if (st != status::failure)
            coder<typename member::type>::encode(ec, const_member_at<typename member::type, member::offset>(v), st);
        }.template operator()<Indices>(), ...);
      } (std::make_index_sequence<ct::list::size<typename n_metadata_member_definitions<T>::member_list>> {});
    }

    /// \brief Decode in a new object, assumes version check has been done before
    template<concepts::SerializableStruct ST>
    static ST decode_struct(decoder& dc, status& st) requires std::default_initializable<T>
    {
      ST v;
      decode_struct(v, dc, st);
      return v;
    }

    /// \brief Decode in-place, assumes version check has been done before
    template<concepts::SerializableStruct ST>
    static void decode_struct(ST& v, decoder& dc, status& st)
    {
      [&st, &dc, &v]<size_t... Indices>(std::index_sequence<Indices...>)
      {
        ([&st, &dc, &v]<size_t Index>()
        {
          using member = ct::list::get_type<typename n_metadata_member_definitions<ST>::member_list, Index>;
          if (st == status::success)
          {
            using member_type = typename member::type;
            member_at<member_type, member::offset>(v).~member_type();
            new (&member_at<member_type, member::offset>(v))  member_type ( coder<member_type>::decode(dc, st) );
            if (st != status::success)
            {
              N_RLE_LOG_FAIL("failed to deserialize member of {} (name: {}, type: {})",
                             ct::type_name<ST>.str, member::name.string, ct::type_name<member_type>.str);
            }
          }
        }.template operator()<Indices>(), ...);
      } (std::make_index_sequence<ct::list::size<typename n_metadata_member_definitions<ST>::member_list>> {});

      post_deserialize(v);
    }

    /// \brief Handle data migration
    /// \note T must be default constructible
    static T decode(decoder& dc, status& st) requires std::default_initializable<T>
    {
      T v;
      decode(v, dc, st);
      return v;
    }

    /// \brief Handle data migration
    /// \note In-place decode (for non-default constructible T)
    static void decode(T& dest, decoder& dc, status& st)
    {
      const auto [version, success] = dc.decode<uint32_t>();
      if (!success || version < get_min_version() || version > get_version())
      {
        N_RLE_LOG_FAIL("failed to deserialize {}: invalid struct version (success: {}, decoded-version: {}, min-version: {}, max-version: {})",
                       ct::type_name<T>.str, success, version, get_min_version(), get_version());
        st = status::failure;
        return;
      }

      if constexpr (get_min_version() == get_version())
      {
        decode_struct<T>(dest, dc, st);
        return;
      }
      else
      {
        // Handle up-to-date data:
        if (version == get_version())
        {
          decode_struct<T>(dest, dc, st);
          return;
        }

        // We now we aren't up-to-date. Handle the migration.
        // We want something like that:
        // version_n_m3 xm3 = decode_struct<version_n_m3>(dc, st);
        // version_n_m2 xm2 = version_n_m2::migrate_from(std::move(xm3));
        // version_n_m1 xm1 = version_n_m1::migrate_from(std::move(xm2));
        // return current_version::migrate_from(std::move(xm1));
        //
        // To handle that, we use a std::variant (containing all possible versions...),
        //  ignore any version < data-versoin,
        //  construct at the data-version,
        //  then migrate until the current one
        typename ct::list::extract<typename T::version_list>::template as<std::variant> version_variant;
        [&dest, &st, &dc, &version_variant, version = version]<size_t... Indices>(std::index_sequence<Indices...>)
        {
          ([&dest, &st, &dc, &version_variant, version]<size_t Index>()
          {
            using version_struct = ct::list::get_type<typename T::version_list, Index>;
            static constexpr uint32_t version_number = get_min_version() + Index;

            if (st == status::failure || version_number < version) return;
            if (version_number == version)
            {
              // decode:
              version_variant = decode_struct<version_struct>(dc, st);
            }
            else
            {
              if constexpr (Index > 0)
              {
                // migrate:
                if constexpr (std::is_same_v<T, version_struct>)
                {
                  dest.migrate_from(*std::get_if<Index - 1>(version_variant));
                }
                else
                {
                  version_struct v;
                  v.migrate_from(*std::get_if<Index - 1>(version_variant));
                  version_variant.emplace(std::move(v));
                }
              }
              else
              {
                N_RLE_LOG_FAIL("failed to deserialize {}: unable to upgrade struct to the final version (current version: {}, final version: {})",
                               ct::type_name<T>.str, version_number, version);
                st = status::failure;
              }
            }
          } .template operator()<Indices>(), ...);
        } (std::make_index_sequence<ct::list::size<typename T::version_list>> {});

        return;
      }
    }

    static void generate_metadata(serialization_metadata& mt)
    {
      std::vector<type_reference> contained_types;

      [&mt, &contained_types]<size_t... Indices>(std::index_sequence<Indices...>)
      {
        ([&mt, &contained_types]<size_t Index>()
        {
          using member = ct::list::get_type<typename n_metadata_member_definitions<T>::member_list, Index>;
          coder<typename member::type>::generate_metadata(mt);
          contained_types.push_back(mt.ref<typename member::type>(member::name.string));
        }.template operator()<Indices>(), ...);
      } (std::make_index_sequence<ct::list::size<typename n_metadata_member_definitions<T>::member_list>> {});

      mt.add_type<T>(
      {
        .mode = type_mode::versioned_tuple,
        .size = sizeof(T),
        .contained_types = std::move(contained_types),
        .version = get_version(),
      });
    }
  };
}



