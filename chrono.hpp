//
// file : chrono.hpp
// in : file:///home/tim/projects/yaggler/yaggler/tools/chrono.hpp
//
// created by : Timothée Feuillet on linux-coincoin.tim
// date: 16/12/2013 11:04:29
//
//
// Copyright (c) 2013-2016 Timothée Feuillet
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


#ifndef __N_7509358901085987665_2104838811__CHRONO_HPP__
# define __N_7509358901085987665_2104838811__CHRONO_HPP__

#include <chrono>

namespace neam
{
  namespace cr
  {
    class chrono
    {
      public:
        chrono() : old_time(now()), speed(1) {}

        /// \brief time since epoch
        static double now()
        {
          return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() * 1e-9;
        }

        /// \brief time relative to the first call (could be the effective start of the prog)
        static double now_relative()
        {
          static double start = now();
          return now() - start;
        }

        /// \brief return the delta time between the last call and now()
        double delta()
        {
          const double right_now = now();
          double dt = right_now - old_time + old_accumulated_time;
          if (paused)
            dt = old_accumulated_time;
          old_time = right_now;
          old_accumulated_time = 0;
          return dt * speed;
        }

        /// \brief accumulate time (act like delta, but do not reset the counter)
        double get_accumulated_time() const
        {
          if (paused)
            return old_accumulated_time * speed;
          const double right_now = now();
          const double dt = right_now - old_time + old_accumulated_time;
          return dt * speed;
        }

        /// \brief reset the old_time to now (do almost the same as calling delta())
        void reset()
        {
          old_time = now();
          old_accumulated_time = 0;
        }

        /// \brief pause the chrono
        void pause()
        {
          if (paused)
            return;
          paused = true;
          old_accumulated_time = get_accumulated_time();
        }

        /// \brief resume a previously paused chrono
        void resume()
        {
          if (!paused)
            return;
          paused = false;
          old_time = now();
        }

        /// \brief set the speed factor of the timer
        void set_speed(double _speed)
        {
          speed = _speed;
        }

        /// \brief get the speed factor of the timer
        double get_speed() const
        {
          return speed;
        }

      private:
        double old_time;
        double speed;
        double old_accumulated_time = 0;
        bool paused = false;
    };
  } // namespace cr
} // namespace neam

#endif /*__N_7509358901085987665_2104838811__CHRONO_HPP__*/

// kate: indent-mode cstyle; indent-width 2; replace-tabs on; 

