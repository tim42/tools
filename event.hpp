//
// created by : Timothée Feuillet
// date: 2022-7-29
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

#include <vector>
#include "async/async.hpp"
#include "debug/assert.hpp"
#include "spinlock.hpp"
#include "type_id.hpp"

namespace neam::cr
{
  template<typename... Args> class event;

  /// \brief generic RAII token for events
  class event_token_t
  {
    public:
      event_token_t() = default;
      event_token_t(event_token_t&&o)
       : destruct(std::move(o.destruct))
       , key(o.key)
      {
        o.key = 0;
      }
      event_token_t& operator = (event_token_t&&o)
      {
        if (&o == this) return *this;
        release();
        destruct = (std::move(o.destruct));
        key = (o.key);
        o.key = 0;
        return *this;
      }

      ~event_token_t()
      {
        release();
      }

      void release()
      {
        if (key != 0)
          destruct(*this);
      }

      bool is_valid() const
      {
        return key != 0;
      }

    private:
      event_token_t(fu2::unique_function<void(event_token_t&)>&& d, uint64_t k) : destruct(std::move(d)), key(k) {}

      fu2::unique_function<void(event_token_t&)> destruct;
      uint64_t key = 0;

      template<typename... Args> friend class event;
  };

  /// \brief Simple (thread-safe) multicast delegate
  /// \note entries added during a call() will not receive all the events currently being dispatched
  ///       (can be added by another thread or by a callee)
  ///
  /// \note if an entry removes itself during its invocation, and there are multiple invocations in progress,
  ///       it is guaranteed that \e after the release() call on the token has returned, the function will not be called again
  ///       It is possible that the function be called \e during the call to release().
  ///
  /// \note memory is never freed automatically (unless destructed)
  template<typename... Args>
  class event
  {
    public:
      using function_t = fu2::unique_function<void(Args...)>;

      ~event()
      {
        std::lock_guard _l(lock);
        check::debug::n_check(count == 0, "{}: {} event receiver are still registered, referencing a destroyed object",
                             ct::type_name<event<Args...>>, functions.size());
      }

      event_token_t operator += (function_t&& fnc) { return add(std::move(fnc)); }
      event& operator -= (event_token_t& tk) { remove(tk); return *this; }

      /// \brief Call the event and all the registered entries
      void operator()(Args... args) { return call(args...); }

      /// \brief Call the event and all the registered entries
      void call(Args... args)
      {
        {
          uint32_t elem_count;
          uint64_t initial_token;
          {
            std::lock_guard _l(lock);
            elem_count = functions.size();
            initial_token = token;
          }
          for (uint32_t i = 0; i < elem_count; ++i)
          {
            // check whether we skip the function or not:
            {
              std::lock_guard _l(lock);
              if (functions[i].key == k_null_token)
                continue;
              // Added during the event call, so we ignore those
              // as otherwise we might have inconsistent behavior
              if (functions[i].key >= initial_token)
                continue;
              [[unlikely]] if (token < initial_token) // overflow, unlikely on a 64bit number
              {
                if (functions[i].key < token)
                  continue;
              }
              // set the in-use flag to avoid undue removal:
              functions[i].in_use += 1;
            }
            // call the function without the lock, so it can perform operations on the event
            functions[i].func(args...);
            // restore the current function pointer:
            {
              std::lock_guard _l(lock);
              functions[i].in_use -= 1;
              if (functions[i].key == k_null_token)
              {
                if (functions[i].in_use != 0)
                  continue;
                // destroy the function pointer
                functions[i].func = {};
              }
            }
          }
        }
      }

      /// \brief Add a new function to the event
      /// \returns a token that once destructed removes the function from the event
      event_token_t add(function_t&& fnc)
      {
        std::lock_guard _l(lock);
        const uint64_t key = token;
        token += 1;
        if (token <= k_null_token)
          token = k_null_token + 1;
        const uint32_t index = search_free();
        if (index == ~0u)
          functions.push_back({key, std::move(fnc)});
        else
          functions[index] = { key, std::move(fnc) };
        ++count;
        return { [this](event_token_t& tk) { remove(tk); }, key };
      }

      template<typename Class>
      event_token_t add(Class& self, void(Class::*fnc)(Args...))
      {
        return add([&self, fnc](Args... args) { return (self.*fnc)(args...); });
      }

      /// \brief Remove a token.
      /// \note The function that the token will not be called once this function returns,
      ///       but it is possible that it can be called \e during the remove() call
      void remove(event_token_t& tk)
      {
        if (tk.key == k_null_token)
          return;

        std::lock_guard _l(lock);
        if (tk.key == k_null_token)
          return;
        remove_id(tk.key);
        tk.key = k_null_token;
        tk.destruct = {};
      }

      uint32_t get_number_of_listeners() const { return count; }

    private:
      void remove_id(uint64_t k)
      {
        for (auto& it : functions)
        {
          if (it.key == k)
          {
            it.key = k_null_token;
            if (!it.in_use)
              it.func = {};
            --count;
          }
        }
      }

      uint32_t search_free() const
      {
        if (count == functions.size())
          return ~0u;
        for (uint32_t i = 0; i < functions.size(); ++i)
        {
          if (functions[i].key == k_null_token && !functions[i].in_use)
            return i;
        }
        return ~0u;
      }

    private:
      struct entry_t
      {
        uint64_t key;
        function_t func;
        uint32_t in_use = 0;
      };
      mutable spinlock lock;
      std::vector<entry_t> functions;
      static constexpr uint64_t k_null_token = 0;
      uint64_t token = k_null_token + 1;
      uint32_t count = 0;
  };
}

