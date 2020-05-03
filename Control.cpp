//  Open BVM Ventilator - Ventilator state machine and control functions
//
//  Created by WHPThomas <me(at)henri(dot)net> on 20/02/20.
//  Copyright (c) 2020 WHPThomas
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <Arduino.h>
#include <EEPROM.h>
#include <FastIO.h>
#include "options.h"
#include "Pins.h"
#include "ButtonDebouncer.h"
#include "Control.h"
#include "Draw.h"
#include "StepperSpeedControl.h"

page_t page;
level_t level;
selection_t selection;
alarm_t alarm;
events_t events;
phase_t phase;
limit_t limit;
ctrl_t ctrl;
live_t live;

void ctrl_setup()
{
  alarm = NO_ALARM;
  memset(&events, 0, sizeof(events_t));
  memset(&live, 0, sizeof(live_t));

  EEPROM.get(0, ctrl);
  EEPROM.get(sizeof(ctrl_t), limit);

  live_full_press_steps();
  live_breath_cycle_time();
  live_inspiratory_time();
  live_minute_ventilation();

  live.volume = ctrl.tidalVolume;
  live.pressure = ctrl.plateauAirwayPressure;
  live.audibleAlarm = ctrl.ventilationActive;
}

/* START POSITION
 * Step offset of start position
 */
 
void ctrl_start_position(unsigned value) {
  ctrl.startPosition = value;
  //DebugMessage("ctrl.startPosition = ", ctrl.startPosition);
  live_full_press_steps();
  live_400_steps();
  live_600_steps();
}

/* PRESS VOLUME
 * Volume of air in a full press
 * This value should be calibrated for each vendors bag
 */
 
void ctrl_full_press_volume(unsigned value) {
  ctrl.fullPressVolume = value;
  //DebugMessage("ctrl.fullPressVolume = ", ctrl.fullPressVolume);
  live_tital_steps();
  live_volume_per_revolution();
  live_400_steps();
  live_600_steps();
}

/* TIDAL VOLUME
 * Volume of air in a breath 
 */

void ctrl_tidal_volume(unsigned value) {
  ctrl.tidalVolume = value;
  //DebugMessage("ctrl.tidalVolume = ", ctrl.tidalVolume);
  live_tital_steps();
  live_minute_ventilation();
}

/* RESPIRATORY RATE
 * Number of breaths per minute
 */

void ctrl_respiratory_rate(unsigned value) {
  ctrl.respiratoryRate = value;
  //DebugMessage("ctrl.respiratoryRate = ", ctrl.respiratoryRate);
  live_breath_cycle_time();
  live_inspiratory_time();
  live_minute_ventilation();
}

/* RESPIRATORY RATIO
 * Ratio of inspiratory to expiratory time
 */

void ctrl_respiratory_ratio(unsigned value) {
  ctrl.respiratoryRatio = value;
  //DebugMessage("ctrl.respiratoryRatio = ", ctrl.respiratoryRatio);
  live_inspiratory_time();
}

/* PLATEAU AIRWAY PRESSURE
 * Maximum inspiratory pressure
 */

void ctrl_plateau_airway_pressure(unsigned value) {
  ctrl.plateauAirwayPressure = value;
  //DebugMessage("ctrl.plateauAirwayPressure = ", ctrl.plateauAirwayPressure);
}

/* INSPIRATORY FLOW
 * Peak inspiratory flow of air 
 */

void ctrl_inspiratory_flow(unsigned value){
  ctrl.inspiratoryFlow = value;
  //DebugMessage("ctrl.inspiratoryFlow = ", ctrl.inspiratoryFlow);
  float volumePerRevolution = (unsigned long)ctrl.fullPressVolume * STEPS_PER_REVOLUTION / live.fullPressSteps ;
  //DebugMessage("volumePerRevolution = ", volumePerRevolution);
  live.inspiratoryRpm = (unsigned long)ctrl.inspiratoryFlow * 1000 / volumePerRevolution;
  //DebugMessage("live.inspiratoryRpm = ", live.inspiratoryRpm);
}

/* EXPIRATORY FLOW
 * Peak expiratory flow of air 
 */

void ctrl_expiratory_flow(unsigned value){
  ctrl.expiratoryFlow = value;
  //DebugMessage("ctrl.expiratoryFlow = ", ctrl.expiratoryFlow);
  float volumePerRevolution = (unsigned long)ctrl.fullPressVolume * STEPS_PER_REVOLUTION / live.fullPressSteps ;
  //DebugMessage("volumePerRevolution = ", volumePerRevolution);
  live.expiratoryRpm = (unsigned long)ctrl.expiratoryFlow * 1000 / volumePerRevolution;
  //DebugMessage("live.expiratoryRpm = ", live.expiratoryRpm);
}

/* PRESSURE TRIGGER
 * Inspiratory breath pressure trigger 
 */

void ctrl_pressure_trigger(unsigned value) {
  ctrl.triggerPressure = value;
  //DebugMessage("ctrl.triggerPressure = ", ctrl.triggerPressure);
}

/* VOLUME PER REVOLUTION
 * An interum value used to calculate stepper motor RPM
 */
 
void live_volume_per_revolution()
{
  float volumePerRevolution = (unsigned long)ctrl.fullPressVolume * STEPS_PER_REVOLUTION / live.fullPressSteps ;
  //DebugMessage("volumePerRevolution = ", volumePerRevolution);
  live.inspiratoryRpm = (unsigned long)ctrl.inspiratoryFlow * 1000 / volumePerRevolution;
  //DebugMessage("live.inspiratoryRpm = ", live.inspiratoryRpm);
  live.expiratoryRpm = (unsigned long)ctrl.expiratoryFlow * 1000 / volumePerRevolution;  
  //DebugMessage("live.expiratoryRpm = ", live.expiratoryRpm);
}

/* BREATH CYCLE TIME
 * Number of milliseconds for a breath cycle
 */

void live_breath_cycle_time() {
  live.breathCycleTime = 60000 / ctrl.respiratoryRate;
  //DebugMessage("live.breathCycleTime = ", live.breathCycleTime);
}

/* INSPIRATORY TIME
 * Number of milliseconds for inspiration
 */

void live_inspiratory_time() {
  live.inspiratoryTime = live.breathCycleTime / (ctrl.respiratoryRatio + 1);
  //DebugMessage("live.inspiratoryTime = ", live.inspiratoryTime);
}

/* MINUTE VENTILATION
 * Volume of air per minute
 */

void live_minute_ventilation() {
  live.minuteVentilation = ctrl.tidalVolume * ctrl.respiratoryRate;
  //DebugMessage("live.minuteVentilation = ", live.minuteVentilation);
}

/*
void actual_minute_ventilation() {
  live.minuteVentilation = 60.0 * ctrl.actualVolume / ctrl.actualCycleTime;
  //DebugMessage("live.minuteVentilation = ", live.minuteVentilation);
}
*/

/* TIDAL STEPS
 * Number of steps in a breath
 */

unsigned tidal_steps(unsigned volume) {
  return ((unsigned long)live.fullPressSteps * volume) / ctrl.fullPressVolume;
}

/* PRESS STEPS
 * Number of steps in a full press 
 */

void live_full_press_steps() {
  live.fullPressSteps = END_POSITION - ctrl.startPosition;
  //DebugMessage("live.fullPressSteps = ", live.fullPressSteps);
  live_tital_steps();
  live_volume_per_revolution();
  live_400_steps();
  live_600_steps();
}

int clamp_input_value(int value, int step, int8_t dir, int lo, int hi) {
  if(dir > 0) {
    value += step;
  }
  else {
    value -= step;
  }
  if(value < lo) value = lo;
  if(value > hi) value = hi;
  return value;
}

void alarm_event(alarm_t a)
{
  if(alarm != a) {
    alarm = a;
    events.list[4] = events.list[3];
    events.list[3] = events.list[2];
    events.list[2] = events.list[1];
    events.list[1] = events.list[0];
    events.list[0] = alarm;
    events.length++;
    if(events.length > MAX_EVENTS) events.length = MAX_EVENTS;
  }
  red_led();
}

void factory_reset()
{
  ctrl.startPosition = 500;
  ctrl.fullPressVolume = 850;
  ctrl.tidalVolume = 450;
  ctrl.respiratoryRate = 12;
  ctrl.respiratoryRatio = 3;
  ctrl.plateauAirwayPressure = 300;
  ctrl.inspiratoryFlow = 35;
  ctrl.expiratoryFlow = 35;
  ctrl.triggerPressure = 50;
  ctrl.ventilationActive = false;

  limit.minimum.pressure = 50;
  limit.maximum.pressure = 400;
  limit.minimum.ventilation = 3000;
  limit.maximum.ventilation = 8000;
  limit.minimum.volume = 180;
  limit.maximum.volume = 750;

  live_full_press_steps();
  live_breath_cycle_time();
  live_inspiratory_time();
  live_minute_ventilation();

  live.volume = ctrl.tidalVolume;
  live.pressure = ctrl.plateauAirwayPressure;
  
  EEPROM.put(0, ctrl);
  EEPROM.put(sizeof(ctrl_t), limit);
}