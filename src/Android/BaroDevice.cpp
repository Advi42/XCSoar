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

#include "BaroDevice.hpp"
#include "NativeBaroListener.hpp"
#include "Java/Class.hxx"
#include "Blackboard/DeviceBlackboard.hpp"
#include "Components.hpp"
#include "Interface.hpp"
#include "LogFile.hpp"

static Java::TrivialClass baro_class;
static jmethodID baro_ctor, close_method;

void
BaroDevice::Initialise(JNIEnv *env)
{
  baro_class.Find(env, "org/xcsoar/GlueBaro");

  baro_ctor = env->GetMethodID(baro_class, "<init>",
                                 "(Lorg/xcsoar/IOIOConnectionHolder;IIIIILorg/xcsoar/Baro$Listener;)V");
  close_method = env->GetMethodID(baro_class, "close", "()V");
}

void
BaroDevice::Deinitialise(JNIEnv *env)
{
  baro_class.Clear(env);
}

static jobject
CreateBaroDevice(JNIEnv *env, jobject holder,
                   DeviceConfig::PressureType type, unsigned bus, unsigned addr, unsigned sample_rate, unsigned flags,
                   BaroListener &listener)
{
  jobject listener2 = NativeBaroListener::Create(env, listener);
  jobject device = env->NewObject(baro_class, baro_ctor, holder,
                                  type, bus, addr, sample_rate, flags,
                                  listener2);
  env->DeleteLocalRef(listener2);

  return device;
}

BaroDevice::BaroDevice(unsigned _index,
                           JNIEnv *env, jobject holder,
                           DeviceConfig::PressureUse use,
                           double _offset, double _factor,
                           DeviceConfig::PressureType type, unsigned bus, unsigned addr,
                           unsigned sample_rate, unsigned flags)
  :index(_index),
   obj(env, CreateBaroDevice(env, holder,
                             type, bus, addr, sample_rate, flags,
                               *this)),
   press_type(type),
   press_use(use),
   offset(_offset),
   factor(_factor),
   calibrate_count(-1), calibrate_sum(0.0),
   kalman_filter(5, 0.3)
{
}

BaroDevice::~BaroDevice()
{
  JNIEnv *env = Java::GetEnv();
  env->CallVoidMethod(obj.Get(), close_method);
}

void BaroDevice::Calibrate(double value)
{
  calibrate_count = 0;
  calibrate_sum = 0;
  calibrate_value = value;
}
bool BaroDevice::IsCalibrating()
{
  return calibrate_count > -1;
}

gcc_pure
static inline
double ComputeNoncompVario(const double pressure, const double d_pressure)
{
  static constexpr double FACTOR(-2260.389548275485);
  static constexpr double EXPONENT(-0.8097374740609689);
  return FACTOR * pow(pressure, EXPONENT) * d_pressure;
}

/*
 * TODO: use ProvidePitotPressure() and get rid of this static variable static_p.
 */
static double static_p = 0;

void
BaroDevice::onBaroValues(unsigned sensor, AtmosphericPressure pressure)
{
  ScopeLock protect(device_blackboard->mutex);
  NMEAInfo &basic = device_blackboard->SetRealState(index);
  basic.UpdateClock();
  basic.alive.Update(basic.clock);

  // Gather values for calibration
  if (calibrate_count > -1)
  {
    calibrate_sum += calibrate_value - pressure.GetHectoPascal();
    calibrate_count++;
    if (calibrate_count >= 100)
    {
      offset = calibrate_sum / calibrate_count;
      basic.ProvideSensorCalibration(factor, offset);
      calibrate_count = -1;
    }
  }

  pressure.Adjust(factor, offset);
  if (pressure.IsPlausible() || (press_use == DeviceConfig::PressureUse::DYNAMIC && pressure.GetHectoPascal() > 0.2))
  {
    double param;

    // Set filter properties depending on sensor type
    if (press_type == DeviceConfig::PressureType::BMP085 &&
        press_use == DeviceConfig::PressureUse::STATIC_WITH_VARIO)
    {
       if (static_p == 0) kalman_filter.SetAccelerationVariance(0.0075);
       param = 0.05;
    } else {
       param = 0.5;
    }

    kalman_filter.Update(pressure.GetHectoPascal(), param);

    switch (press_use) {
      case DeviceConfig::PressureUse::NONE:
        break;

      case DeviceConfig::PressureUse::STATIC_ONLY:
        static_p = kalman_filter.GetXAbs();
        basic.ProvideStaticPressure(AtmosphericPressure::HectoPascal(static_p));
        break;

      case DeviceConfig::PressureUse::STATIC_WITH_VARIO:
        static_p = pressure.GetHectoPascal();
        basic.ProvideNoncompVario(ComputeNoncompVario(kalman_filter.GetXAbs(), kalman_filter.GetXVel()));
        basic.ProvideStaticPressure(AtmosphericPressure::HectoPascal(static_p));
        break;

      case DeviceConfig::PressureUse::TEK_PRESSURE:
        basic.ProvideTotalEnergyVario(ComputeNoncompVario(kalman_filter.GetXAbs(),
                                                    kalman_filter.GetXVel()));
        break;

      case DeviceConfig::PressureUse::PITOT:
        if (static_p != 0)
        {
          double dyn = pressure.GetHectoPascal() - static_p;
          if (dyn < 0.31) dyn = 0;      // suppress speeds below ~25 km/h
          basic.ProvideDynamicPressure(AtmosphericPressure::HectoPascal(dyn));
        }
        break;

      case DeviceConfig::PressureUse::PITOT_ZERO:
        offset = kalman_filter.GetXAbs() - static_p;
        basic.ProvideSensorCalibration(1, offset);
        break;

      case DeviceConfig::PressureUse::DYNAMIC:
        basic.ProvideDynamicPressure(AtmosphericPressure::HectoPascal(kalman_filter.GetXAbs()));
        break;
    }
  }

  device_blackboard->ScheduleMerge();
}

void
BaroDevice::onBaroError()
{
  ScopeLock protect(device_blackboard->mutex);
  NMEAInfo &basic = device_blackboard->SetRealState(index);

  basic.static_pressure_available.Clear();
  basic.noncomp_vario_available.Clear();

  device_blackboard->ScheduleMerge();
}