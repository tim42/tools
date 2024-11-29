//
// created by : Timothée Feuillet
// date: 2022-1-2
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

#include "mt_check/mt_check_base.hpp"

#if __has_include(<glm/glm.hpp>)
  #include <glm/glm.hpp>

  #define N_CR_ROLLING_AVG_MIN_FUNC             glm::min
  #define N_CR_ROLLING_AVG_MAX_FUNC             glm::max
  #define N_CR_ROLLING_ANY_EQUAL_FUNC(a, b)     internal::any_eq_wrapper(a, b)

namespace neam::cr::internal
{
  template<glm::length_t N, typename T, glm::qualifier Q>
  static constexpr bool any_eq_wrapper(glm::vec<N, T, Q> a, glm::vec<N, T, Q> b)
  {
    return glm::any(glm::equal(a, b));
  }

  template<typename T>
  static constexpr bool any_eq_wrapper(T a, T b) { return true || a == b; }
}
#else
  #define N_CR_ROLLING_AVG_MIN_FUNC std::min
  #define N_CR_ROLLING_AVG_MAX_FUNC std::max
  #define N_CR_ROLLING_ANY_EQUAL_FUNC(a, b)     (a == b)
#endif


namespace neam::cr
{
  /// \brief Handle rolling averages (+min / max)
  template<typename InnerType>
  class rolling_average : public mt_checked<rolling_average<InnerType>>
  {
    public:
      using value_type = InnerType;

    public:
      rolling_average(uint32_t max_size) { data.resize(max_size); }

      rolling_average(const rolling_average& o) = default;
      rolling_average(rolling_average&& o) = default;
      rolling_average& operator = (const rolling_average& o) = default;
      rolling_average& operator = (rolling_average&& o) = default;

      void add_value(InnerType value)
      {
        N_MTC_WRITER_SCOPE;

        const InnerType old_value = data[insertion_index];
        data[insertion_index] = value;

        const bool recompute_min_max = (inserted_values == (uint32_t)data.size()) &&
                                       (N_CR_ROLLING_ANY_EQUAL_FUNC(old_value, min_value) || N_CR_ROLLING_ANY_EQUAL_FUNC(old_value, max_value));
        insertion_index = ((insertion_index + 1) % ((uint32_t)data.size()));
        inserted_values = std::min(inserted_values + 1, (uint32_t)data.size());

        if (recompute_min_max)
        {
          [[unlikely]];
          min_value = data[0];
          max_value = data[0];
          sum_value = data[0];
          for (uint32_t i = 1; i < inserted_values; ++i)
          {
            min_value = N_CR_ROLLING_AVG_MIN_FUNC(min_value, data[i]);
            max_value = N_CR_ROLLING_AVG_MAX_FUNC(max_value, data[i]);
            sum_value += data[i]; // recompute sum to avoid error accumulation
          }
        }
        else if (inserted_values > 1)
        {
          [[likely]];
          if (inserted_values == (uint32_t)data.size()) [[likely]]
            sum_value -= old_value;
          sum_value += value;
          min_value = N_CR_ROLLING_AVG_MIN_FUNC(min_value, value);
          max_value = N_CR_ROLLING_AVG_MAX_FUNC(max_value, value);
        }
        else
        {
          [[unlikely]];
          min_value = value;
          max_value = value;
          sum_value = value;
        }
      }

      InnerType get_max() const { N_MTC_READER_SCOPE; return max_value; }
      InnerType get_min() const { N_MTC_READER_SCOPE; return min_value; }
      InnerType get_average() const
      {
        N_MTC_READER_SCOPE;
        neam::check::debug::n_assert(inserted_values > 0, "rolling_average: division by 0 (get_average but no value has been inserted)");
        return sum_value / (InnerType)inserted_values;
      }

      size_t size() const { return inserted_values; }
      size_t total_size() const { return data.size(); }

      InnerType operator [](size_t pos) const
      {
        N_MTC_READER_SCOPE;
        neam::check::debug::n_assert(pos < inserted_values, "rolling_average: out of bounds: reading at position {} of a container of size {}", pos, inserted_values);
        return data[(insertion_index + pos) % inserted_values];
      }

      /// \warning the first entry of data may not be
      const InnerType* inner_data() const { return data.data(); }
      const uint32_t first_element_offset() const { return insertion_index; }

    private:
      std::vector<InnerType> data;

      InnerType max_value;
      InnerType min_value;
      InnerType sum_value;

      uint32_t insertion_index = 0;
      uint32_t inserted_values = 0;
  };
}

#undef N_CR_ROLLING_AVG_MIN_FUNC
#undef N_CR_ROLLING_AVG_MAX_FUNC
