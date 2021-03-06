//
//  alarms.c
//  alarms++
//
//  Created by Christian Reinbacher on 01.11.14.
//
//
#include "alarms.h"
#include "localize.h"

void alarm_draw_row(Alarm* alarm, GContext* ctx)
{
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorBlack);
  GFont font = fonts_get_system_font(alarm->enabled?FONT_KEY_GOTHIC_28_BOLD:FONT_KEY_GOTHIC_28);
  
  char alarm_time[6];
  int hour_out = alarm->hour;
  bool is_am = false;
  if (!clock_is_24h_style())
  {
    convert_24_to_12(alarm->hour, &hour_out, &is_am);
    graphics_draw_text(ctx, is_am?"A\nM":"P\nM",
                       fonts_get_system_font(alarm->enabled?FONT_KEY_GOTHIC_14_BOLD:FONT_KEY_GOTHIC_14),
                       GRect(57-(alarm->enabled?3:0), -1, 144 - 33, 30), GTextOverflowModeWordWrap,
                       GTextAlignmentLeft, NULL);
  }
  
  snprintf(alarm_time,sizeof(alarm_time),"%02d:%02d",hour_out,alarm->minute);
  graphics_draw_text(ctx, alarm_time,font,
                     GRect(3, -3, 144 - 33, 28), GTextOverflowModeFill,
                     GTextAlignmentLeft, NULL);
  
  // draw activity state
  char state[4];
  snprintf(state, sizeof(state), "%s",alarm->enabled? _("ON"):_("OFF"));
  graphics_draw_text(ctx, state,font,
                     GRect(3, -3, 144 - 5, 28), GTextOverflowModeFill,
                     GTextAlignmentRight, NULL);

   // draw active weekdays
  char weekday_state[10];
  snprintf(weekday_state,sizeof(weekday_state),"%s%s%s%s%s\n%s%s",
           alarm->weekdays_active[1]?_("M"):"_",
           alarm->weekdays_active[2]?_("T"):"_",
           alarm->weekdays_active[3]?_("W"):"_",
           alarm->weekdays_active[4]?_("T"):"_",
           alarm->weekdays_active[5]?_("F"):"_",
           alarm->weekdays_active[6]?_("S"):"_",
           alarm->weekdays_active[0]?_("S"):"_");
  
  graphics_draw_text(ctx, weekday_state,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(70, -1, 144 - 33, 30), GTextOverflowModeWordWrap,
                     GTextAlignmentLeft, NULL);
  
}

void alarm_toggle_enable(Alarm *alarm)
{
  if(alarm->enabled==false)
  {
    alarm->enabled=true;
    alarm_schedule_wakeup(alarm);
  }
  else
  {
    alarm->enabled=false;
    alarm_cancel_wakeup(alarm);
  }
}

void alarm_schedule_wakeup(Alarm *alarm)
{
  alarm_cancel_wakeup(alarm);
  if(alarm->enabled)
  {
    // Calculate time to wake up
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    time_t timestamp = clock_to_timestamp(TODAY,alarm->hour,alarm->minute);
    bool some_active=false;
    
    // Check if we may schedule the alarm today
    int current_weekday = t->tm_wday;
    for(int i=0;i<7;i++)
    {
      if(alarm->weekdays_active[(i+current_weekday)%7])
      {
        if(i==0)
          timestamp = clock_to_timestamp(TODAY,alarm->hour,alarm->minute);
        else
          timestamp = clock_to_timestamp(((i+current_weekday)%7)+1,alarm->hour,alarm->minute);
        if((now-timestamp)>(60*60*24*7)) // we can not be more than a week away?
          continue;
        else
        {
          some_active=true;
          break;
        }
      }
    }
    if(some_active)
    {
      t = localtime(&timestamp);
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Scheduled at %d.%d %d:%d",t->tm_mday, t->tm_mon+1,t->tm_hour,t->tm_min);
      alarm->alarm_id = wakeup_schedule(timestamp,0,true);
    }
    else //maybe we have missed a today event. this takes then place next week
    {
      if(alarm->weekdays_active[current_weekday])
      {
        some_active=true;
        timestamp = clock_to_timestamp(TODAY,alarm->hour,alarm->minute);
        t = localtime(&timestamp);
        APP_LOG(APP_LOG_LEVEL_DEBUG,"Scheduled at %d.%d %d:%d",t->tm_mday, t->tm_mon+1,t->tm_hour,t->tm_min);
        alarm->alarm_id = wakeup_schedule(timestamp,0,true);
      }
      else // one-time alert (no weekdays active)
      {
        timestamp = clock_to_timestamp(TODAY,alarm->hour,alarm->minute);
        if((timestamp-now)>(60*60*24))
          timestamp = clock_to_timestamp(current_weekday+1,alarm->hour,alarm->minute);
        
        t = localtime(&timestamp);
        APP_LOG(APP_LOG_LEVEL_DEBUG,"Scheduled one-time event at %d.%d %d:%d",t->tm_mday, t->tm_mon+1,t->tm_hour,t->tm_min);
        alarm->alarm_id = wakeup_schedule(timestamp,0,true);
      }
    }
    if(some_active && alarm->alarm_id<0) // some error occured
    {
      switch (alarm->alarm_id) {
        case E_RANGE:
          alarm->alarm_id = wakeup_schedule(timestamp-120,0,true); //schedule alarm 2 minutes before the deadline, better than nothing...
          APP_LOG(APP_LOG_LEVEL_DEBUG,"Had to reschedule timer...");
          break;
        case E_OUT_OF_RESOURCES:
          // do some magic here...
          break;
        case E_INVALID_ARGUMENT:
          // should not happen
          break;
        default:
          break;
      }
    }
  }
}

void alarm_cancel_wakeup(Alarm *alarm)
{
  if (alarm->alarm_id <= 0) {
    return;
  }
  wakeup_cancel(alarm->alarm_id);
  alarm->alarm_id = -1;
}

void reschedule_wakeup(Alarm *alarms)
{
  wakeup_cancel_all();
  for (int i=0;i<NUM_ALARMS;i++)
  {
    alarm_schedule_wakeup(&alarms[i]);
  }
}

bool alarm_is_one_time(Alarm *alarm)
{
  bool onetime=true;
  
  for (int i=0;i<7;i++)
    if(alarm->weekdays_active[i])
      onetime=false;
  
  return onetime;
}

void convert_24_to_12(int hour_in, int* hour_out, bool* am)
{
  if(hour_in==0)
  {
    *hour_out=12;
    *am=true;
  }
  else if (hour_in==12)
  {
    *hour_out=12;
    *am=false;
  }
  else if (hour_in<12)
  {
    *hour_out = hour_in;
    *am=true;
  }
  else
  {
    *hour_out = hour_in-12;
    *am=false;
  }
}