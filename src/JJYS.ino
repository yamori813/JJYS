/*
 * SEIKO TIME SYSTEMS INC. TDC-300 modoki Program for ntpd 40 driver
 * JJY Signal input from PIN2 and send 2400 baudrate to tx.
 * This code developed Arduino 1.0.6 used by MsTimer2 library.
 * You must use external 16MHz crystal for best accuracy.
 *
 * Copyright (c) 2017 Hiroki Mori
 */

#include <MsTimer2.h>

//#define DEBUG
#define INVERT

int pin = 13;
int jjy = 2;
unsigned long startTime;
unsigned long invTime;
unsigned long lastTime;
int lastBIt;
int pos;
int jjystat;
int year, day, hour, minutes, week;

struct nexclock {
  int year;
  int manth;
  int day;
  int hour;
  int minits;
  int week;
};

struct nexclock nexclock;

void calcmonth(int ye, int day, int *manth, int *manday)
{
  int i;
  int sum;
  int mandays[12] = {
    31,28,31,30,31,30,31,31,30,31,30,31          };
  int md;

  i = 0;
  sum = 0;
  while(i < 12) {
    md = mandays[i];
    if(ye % 4 == 0 && i == 1)   // ajust uruudoshi
      ++md;
    if(day <= sum + md) {
      *manth = i + 1;
      *manday = day - sum;
      return;
    }
    sum += md;
    ++i;
  }
}

void calcnexttime(int ye, int day, int hou, int min, int wee)
{
  nexclock.week = wee;

  if(min == 59) {
    min = 0;
    ++hou;
    if(hou == 24) {
      hou = 0;
      ++day;
      if(nexclock.week == 6)
        nexclock.week = 0;
      else
        ++nexclock.week;			
      if((ye % 4 != 0 && day == 357) ||
        (ye % 4 == 0 && day == 358)) {
        ++ye;
        day = 1;
      }
    }

    nexclock.year = ye;
    calcmonth(ye, day, &nexclock.manth, &nexclock.day);
    nexclock.hour = hou;
    nexclock.minits = min;
  } 
  else {
    nexclock.year = ye;
    calcmonth(ye, day, &nexclock.manth, &nexclock.day);
    nexclock.hour = hou;
    nexclock.minits = min + 1;
  }
}

void setdatetime(int pos, int bit){
  if(bit == 1) {
    if(pos >= 1 && pos <=3)
      minutes += (1 << (3 - pos)) * 10;
    else if(pos >= 5 && pos <=8)
      minutes += (1 << (8 - pos));
    else if(pos >= 12 && pos <=13)
      hour += (1 << (13 - pos)) * 10;
    else if(pos >= 15 && pos <=18)
      hour += (1 << (18 - pos));
    else if(pos >= 22 && pos <=23)
      day += (1 << (23 - pos)) * 100;
    else if(pos >= 25 && pos <=28)
      day += (1 << (28 - pos)) * 10;
    else if(pos >= 30 && pos <=33)
      day += (1 << (33 - pos));
    else if(pos >= 41 && pos <=44)
      year += (1 << (44 - pos)) * 10;
    else if(pos >= 45 && pos <=48)
      year += (1 << (48 - pos));
    else if(pos >= 50 && pos <=52)
      week += (1 << (52 - pos));
  }
}

void setup()
{
  pinMode(pin, OUTPUT);
  attachInterrupt(0, intr, CHANGE);
  Serial.begin(2400);
  pos = 0;
  jjystat = 0;
}

void sendtime() {
  int next;
  char messageBuf[40];
  ++pos;
  if(pos == 60) {
    pos = 0;
  }
  if(pos == 59)
    next = 0;
  else
    next = pos + 1;
#ifdef DEBUG
  sprintf(messageBuf, "%02d%02d%02d%d%02d%02d%02d\r\n", 
  nexclock.year , nexclock.manth, nexclock.day, 
  nexclock.week, nexclock.hour, nexclock.minits, next);
#else
  sprintf(messageBuf, "%c%02d%02d%02d%d%02d%02d%02d%c", 
  0x02, nexclock.year , nexclock.manth, nexclock.day, 
  nexclock.week, nexclock.hour, nexclock.minits, next, 0x03);
#endif
  Serial.print(messageBuf);
#ifndef DEBUG
  sprintf(messageBuf, "%c%c%c", 0x02, 0xe5, 0x03);
  Serial.print(messageBuf);
#endif
  if(pos == 59) {
    MsTimer2::stop();
    MsTimer2::set(1000, sendtime);
    MsTimer2::start();
  }
  if(pos == 58) {
    MsTimer2::stop();
  }
}

void loop()
{
}

void intr()
{
  unsigned long nowTime;
  unsigned long aTime;
  digitalWrite(pin, digitalRead(jjy));
  nowTime = millis();
  if(nowTime - lastTime > 10) {  /* workaround noise */
#ifdef INVERT
    if(digitalRead(jjy) == 0) {
#else
    if(digitalRead(jjy) == 1) {
#endif
      if(startTime < nowTime)
        aTime = nowTime - startTime;
      else
        aTime = nowTime + (0xffffffff - startTime) + 1;
      if(aTime > 900 && aTime < 1100) {
        if(startTime < invTime)
          aTime = nowTime - invTime;
        else
          aTime = nowTime + (0xffffffff - invTime) + 1;          
        if(aTime > 100 && aTime < 300) {
          lastBIt = 0;
        } 
        else if(aTime > 400 && aTime < 600) {
          lastBIt = 1;
        } 
        else if(aTime > 700 && aTime < 900) {
          if(lastBIt == 2) {
            if(jjystat == 0) {
#ifdef DEBUG
              Serial.println("GET DUBLE MERKER");
#endif
              jjystat = 1;
              pos = 0;
              minutes = 0;
              hour = 0;
              day = 0;
              year = 0;
              week = 0;
            }
          }
          lastBIt = 2;
        } 
        else {   /* error */
          if(jjystat == 2)
            MsTimer2::stop();
          jjystat = 0;
        }

        if(jjystat != 0) {
          if((pos == 0 || pos % 10 == 9) && lastBIt != 2) {   /* error */
            if(jjystat == 2)
              MsTimer2::stop();
            jjystat = 0;
          } 
          else {
            setdatetime(pos, lastBIt);
            if(pos == 58) {
              calcnexttime(year, day, hour, minutes, week);
              if(minutes != 14 && minutes != 44) {
                year = 0;
                week = 0;
              }
              minutes = 0;
              hour = 0;
              day = 0;
              MsTimer2::set(900, sendtime);
              MsTimer2::start();
              if(jjystat == 1) {
                jjystat = 2;
              }
            }
          }
        }
      }
      else {
        if(jjystat != 0 && !((minutes == 15 || minutes == 45) && 
          (pos >= 40 && pos <= 50))) {
#ifdef DEBUG
          Serial.println("");
          Serial.print("Error ");
          Serial.println(aTime);
#endif
          if(jjystat == 2)
            MsTimer2::stop();
          jjystat = 0;
        }
      }
      startTime = nowTime;
      if(jjystat == 1) {
        ++pos;
        if(pos == 60) {
          pos = 0;
        }
      }
    } 
    else {
      invTime = nowTime;
    }
  }
  lastTime = nowTime;
}


