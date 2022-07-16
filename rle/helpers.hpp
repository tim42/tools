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

#include <string>
#include <variant>
#include <optional>
#include <utility>
#include <concepts>

#include "../ct_list.hpp"
#include "../type_id.hpp"

#include "rle_encoder.hpp"
#include "rle_decoder.hpp"

#include "enum.hpp"
#include "serialization_metadata.hpp"

namespace neam::rle
{
  /// \brief Generic en/de coder, handle most generic types (integers, floating points, raw_types)
  /// \note This class only handle very generic types. If it asserts, this means you need to override stuff
  /// \note GLM stuff should fall right here (as most vectors and matrices have a default copy/constructor/destructor
  template<typename T>
  struct coder
  {
    static_assert(std::is_trivially_destructible_v<T>, "T must be trivially destructible to use the default rle::coder");
    static_assert(std::is_trivially_default_constructible_v<T>, "T must be trivially default constructible to use the default rle::coder");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable to use the default rle::coder");

    static_assert(!std::is_pointer_v<T>, "T must not be a pointer. Pointers are not supported.");
    static_assert(!std::is_reference_v<T>, "T must not be a reference. References are not supported.");

    static void encode(encoder& ec, const T& v, status& /*st*/)
    {
      memcpy(ec.allocate(sizeof(T)), &v, sizeof(T));
    }
    static T decode(decoder& dc, status& st)
    {
      if (dc.get_size() >= sizeof(T))
      {
        const T* v = dc.get_address<T>();
        dc.skip(sizeof(T));
        return *v;
      }
      N_RLE_LOG_FAIL("failed to deserialize {} (type size: {}, data size: {})", ct::type_name<T>.str, sizeof(T), dc.get_size());
      st = status::failure;
      return {};
    }

    static void generate_metadata(serialization_metadata& mt)
    {
      mt.add_type<T>( { type_mode::raw, sizeof(T), } );
    }
  };

  template<typename T>
  struct coder<const T>
  {
    static void encode(encoder& ec, const T& v, status& st)
    {
      coder<T>::encode(ec, v, st);
    }
    static T decode(decoder& dc, status& st)
    {
      return coder<T>::decode(dc, st);
    }

    static void generate_metadata(serialization_metadata& mt)
    {
      coder<T>::generate_metadata(mt);
    }
  };

  /// \brief Helper for std::basic_string (includes std::string)
  template<typename CharT, typename Traits, typename Allocator>
  struct coder<std::basic_string<CharT, Traits, Allocator>>
  {
    using str_type = std::basic_string<CharT, Traits, Allocator>;

    static void encode(encoder& ec, const str_type& v, status&)
    {
      memcpy(ec.encode_and_alocate<uint32_t>(v.size() * sizeof(CharT)), v.data(), v.size() * sizeof(CharT));
    }
    static str_type decode(decoder& dc, status& st)
    {
      decoder subdc = dc.decode_and_skip<uint32_t>();
      if (subdc.is_valid() && subdc.get_size()%sizeof(CharT) == 0)
        return { subdc.get_address<CharT>(), subdc.get_size()/sizeof(CharT) };

      N_RLE_LOG_FAIL("failed to deserialize {}: decoder is not in a valid state", ct::type_name<str_type>.str);
      st = status::failure;
      return {};
    }

    static void generate_metadata(serialization_metadata& mt)
    {
      mt.add_type<str_type>( { type_mode::container, 0, { mt.ref<CharT>() } } );
      coder<CharT>::generate_metadata(mt);
    }
  };

  /// \brief std::pair wrapper
  template<typename F, typename S>
  struct coder<std::pair<F, S>>
  {
    using pair_type = std::pair<F, S>;
    static void encode(encoder& ec, const pair_type& v, status& st)
    {
      coder<F>::encode(ec, v.first, st);
      coder<S>::encode(ec, v.second, st);
    }
    static pair_type decode(decoder& dc, status& st)
    {
      return
      {
        coder<F>::decode(dc, st),
        coder<S>::decode(dc, st),
      };
    }
    static void generate_metadata(serialization_metadata& mt)
    {
      mt.add_type<pair_type>( { type_mode::tuple, sizeof(pair_type), { mt.ref<F>(), mt.ref<S>() } } );
      coder<F>::generate_metadata(mt);
      coder<S>::generate_metadata(mt);
    }
  };

  /// \brief std::tuple wrapper
  template<typename... T>
  struct coder<std::tuple<T...>>
  {
    using tuple_type = std::tuple<T...>;
    static void encode(encoder& ec, const tuple_type& v, status& st)
    {
      [&]<size_t... Indices>(std::index_sequence<Indices...>)
      {
        (coder<T>::encode(ec, std::get<Indices>(v), st), ...);
      } (std::make_index_sequence<sizeof...(T)> {});
    }
    static tuple_type decode(decoder& dc, status& st)
    {
      return { coder<T>::decode(dc, st)... };
    }

    static void generate_metadata(serialization_metadata& mt)
    {
      mt.add_type<tuple_type>( { type_mode::tuple, sizeof(tuple_type), { mt.ref<T>()... } } );
      (coder<T>::generate_metadata(mt), ...);
    }
  };

  /// \brief std::variant<> wrapper. So you can use safe union.
  template<typename... T>
  struct coder<std::variant<T...>>
  {
    using opt_type = std::variant<T...>;
    static void encode(encoder& ec, const opt_type& v, status& st)
    {
      if (v.index() == std::variant_npos)
      {
        ec.encode<uint32_t>(0);
        return;
      }
      const uint32_t type_idx = (uint32_t)v.index() + 1;
      ec.encode(type_idx);
      std::visit([&]<typename FT>(const FT& t)
      {
        coder<FT>::encode(ec, t, st);
      }, v);
    }
    static opt_type decode(decoder& dc, status& st)
    {
      auto [type_idx, success] = dc.decode<uint32_t>();
      if (!success)
      {
        N_RLE_LOG_FAIL("failed to deserialize {}: could not decode type index", ct::type_name<opt_type>.str);
        return (st = status::failure), opt_type{};
      }

      if (type_idx == 0) return {};
      --type_idx;
      if (type_idx >= sizeof...(T))
      {
        N_RLE_LOG_FAIL("failed to deserialize {}: type index ({}) is greater than the number of contained types", ct::type_name<opt_type>.str, type_idx);
        return (st = status::failure), opt_type{};
      }

      opt_type v;
      [type_idx = type_idx, &v, &dc, &st]<size_t... Indices>(std::index_sequence<Indices...>)
      {
        ([&]<size_t Index>()
        {
          if (Index == type_idx)
            v = coder<ct::list::get_type<ct::type_list<T...>, Index>>::decode(dc, st);
        }.template operator()<Indices>(), ...);
      } (std::make_index_sequence<sizeof...(T)> {});
      return v;
    }

    static void generate_metadata(serialization_metadata& mt)
    {
      mt.add_type<opt_type>( { type_mode::variant, 0, { mt.ref<T>()... } } );
      (coder<T>::generate_metadata(mt), ...);
    }
  };

  template<typename T>
  struct coder<std::optional<T>>
  {
    using opt_type = std::optional<T>;
    static void encode(encoder& ec, const opt_type& v, status& st)
    {
      ec.encode<uint32_t>(v ? 1 : 0);
      if (v)
        coder<T>::encode(ec, *v, st);
    }
    static opt_type decode(decoder& dc, status& st)
    {
      if (!dc.decode<uint32_t>().first)
        return {};

      return {coder<T>::decode(dc, st)};
    }

    static void generate_metadata(serialization_metadata& mt)
    {
      mt.add_type<opt_type>({ type_mode::variant, 0, { mt.ref<T>() } });
      coder<T>::generate_metadata(mt);
    }
  };

  /// \brief Handle raw_data (let's say for binary blobs or stuff like that)
  template<>
  struct coder<raw_data>
  {
    static void encode(encoder& ec, const raw_data& v, status& /*st*/)
    {
      ec.encode<uint32_t>(v.size);
      if (v.size > 0)
      {
        memcpy(ec.allocate(v.size), v.data.get(), v.size);
      }
    }
    static raw_data decode(decoder& dc, status& st)
    {
      decoder subdc = dc.decode_and_skip<uint32_t>();
      if (!subdc.is_valid())
      {
        N_RLE_LOG_FAIL("failed to deserialize {}: decoder is not in a valid state", ct::type_name<raw_data>.str);
        return (st = status::failure), raw_data{};
      }

      raw_data v = raw_data::allocate(subdc.get_size());
      memcpy(v.data.get(), subdc.get_address(), v.size);
      return v;
    }

    static void generate_metadata(serialization_metadata& mt)
    {
      mt.add_type<raw_data>({ type_mode::container, 0, cr::construct<std::vector>( mt.ref<uint8_t>() ) });
      coder<uint8_t>::generate_metadata(mt);
    }
  };

  // Some utility concepts
  namespace concepts
  {
    template<typename T>
    concept ReservableContainer = requires(T v)
    {
      v.reserve(0);
    };

    template<typename T>
    concept EmplaceBackContainer = requires(T v)
    {
      v.emplace_back(typename T::value_type{});
    };

    template<typename T>
    concept EmplaceHintContainer = requires(T v)
    {
      v.emplace_hint(v.end(), typename T::value_type{});
    };

    // Last resort, works with std::array<>
    template<typename T>
    concept IndexableAssignable = requires(T v)
    {
      // test for constexprness (we must have a statically allocated container for that to be working)
      typename std::integral_constant<size_t, (T{}).size()>;

      v[0] = typename T::value_type{};
    };

    template<typename T>
    concept IterableSizedContainer = requires(T v)
    {
      typename T::value_type;
      v.begin();
      v.end();

      // I'd like to use unsigned integral but some people/libraries just want to have signed sizes...
      { v.size() } -> std::integral;
    };
  }

  // Handle containers in a generic fashion:
  // Should work for all STL containers, including std::array<>
  template<concepts::IterableSizedContainer T>
  requires concepts::EmplaceHintContainer<T>
        || concepts::EmplaceBackContainer<T>
        || concepts::IndexableAssignable<T>
  struct coder<T>
  {
    using value_type = typename T::value_type;
    static void reserve(T& v, size_t c) requires concepts::ReservableContainer<T> { v.reserve(c); }
    static void reserve(T& /*v*/, size_t /*c*/) { }

    static void add(T&v, value_type&& vt, unsigned) requires concepts::EmplaceHintContainer<T> { v.emplace_hint(v.end(), std::move(vt)); }
    static void add(T&v, value_type&& vt, unsigned) requires concepts::EmplaceBackContainer<T> { v.emplace_back(std::move(vt)); }
    static void add(T&v, value_type&& vt, unsigned i) requires (concepts::IndexableAssignable<T> && !(concepts::EmplaceBackContainer<T> || concepts::EmplaceBackContainer<T>)) { v[i] = (std::move(vt)); }

    static void encode(encoder& ec, const T& v, status& st)
    {
      ec.encode<uint32_t>(v.size());
      for (const auto& it : v)
        coder<value_type>::encode(ec, it, st);
    }
    static T decode(decoder& dc, status& st)
    {
      const auto [entry_count, success] = dc.decode<uint32_t>();
      if (!success)
      {
        N_RLE_LOG_FAIL("failed to deserialize {}: could not decode the entry count", ct::type_name<T>.str);
        return (st = status::failure), T{};
      }

      T v;
      reserve(v, entry_count);
      for (unsigned i = 0; i < entry_count; ++i)
      {
        add(v, coder<value_type>::decode(dc, st), i);
        if (st == status::failure)
        {
          N_RLE_LOG_FAIL("failed to decode entry {} of {} (container type: {})", i, entry_count, ct::type_name<T>.str);
          return {};
        }
      }

      return v;
    }

    static void generate_metadata(serialization_metadata& mt)
    {
      mt.add_type<T>({ type_mode::container, 0, { mt.ref<value_type>() } });
      coder<value_type>::generate_metadata(mt);
    }
  };
}
