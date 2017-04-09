/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2016 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#ifndef XCSOAR_ANDROID_BARO_DEVICE_HPP
#define XCSOAR_ANDROID_BARO_DEVICE_HPP

#include "BaroListener.hpp"
#include "Java/Object.hxx"
#include "Math/SelfTimingKalmanFilter1d.hpp"
#include "Compiler.h"
#include "Device/Config.hpp"

#include <jni.h>

class BaroDevice final : public BaroListener {
  unsigned index;
  Java::GlobalObject obj;
  DeviceConfig::PressureType press_type;
  DeviceConfig::PressureUse press_use;
  double offset;
  double factor;

  int calibrate_count;
  double calibrate_sum;
  double calibrate_value;
  /**
   * This Kalman filter is used to smooth the pressure input.
   */
  SelfTimingKalmanFilter1d kalman_filter;

public:
  static void Initialise(JNIEnv *env);
  static void Deinitialise(JNIEnv *env);

  BaroDevice(unsigned index,
               JNIEnv *env, jobject holder,
               DeviceConfig::PressureUse press_use,
               double _offset, double _factor,
               DeviceConfig::PressureType type, unsigned bus, unsigned addr,
               unsigned sample_rate, unsigned flags);

  ~BaroDevice();

  void Calibrate(double value);
  bool IsCalibrating();

private:
  /* virtual methods from class BaroListener */
  virtual void onBaroValues(unsigned sensor, AtmosphericPressure pressure) override;
  virtual void onBaroError() override;
};

#endif
