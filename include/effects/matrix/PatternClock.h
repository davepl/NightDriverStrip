//+--------------------------------------------------------------------------
//
// File:        PatternClock.h
//
// NightDriverStrip - (c) 2018 Plummer's Software LLC.  All Rights Reserved.
//
// This file is part of the NightDriver software project.
//
//    NightDriver is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    NightDriver is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Nightdriver.  It is normally found in copying.txt
//    If not, see <https://www.gnu.org/licenses/>.
//
// Description:
//
//   Effect code to draw an analog clock
//
// History:     Aug-01-2022         Davepl      For David S.
//
//---------------------------------------------------------------------------

#ifndef PatternClock_H
#define PatternClock_H

// Description:
//
// This file defines the PatternClock class, a subclass of LEDStripEffect.
// The class is designed to render a clock effect on an LED matrix. It 
// includes functionality to display time with hour, minute, and second 
// hands, along with tick marks for each hour. The clock's appearance and 
// behavior are customizable through various methods.

class PatternClock : public LEDStripEffect
{
    // Radius is the lesser of the height and width so that the round clock can fit
    // on rectangular display

    float    radius;

  public:

    PatternClock() : LEDStripEffect(EFFECT_MATRIX_CLOCK, "Clock")
    {
    }

    PatternClock(const JsonObjectConst& jsonObject) : LEDStripEffect(jsonObject)
    {
    }

    bool RequiresDoubleBuffering() const override
    {
        return false;
    }

    virtual size_t DesiredFramesPerSecond() const override
    {
        return 60;
    }

    void Draw() override
    {
        // Get the hours, minutes, and seconds of hte current time

        time_t currentTime;
        struct tm *localTime;
        time( &currentTime );
        localTime = localtime( &currentTime );
        auto hours   = localTime->tm_hour;
        auto minutes = localTime->tm_min;
        auto seconds = localTime->tm_sec;

        timeval tv;
        gettimeofday(&tv, nullptr);
        auto sixtieths = tv.tv_usec * 60 / 1000000;

        // Draw the clock face, outer ring and inner dot where the hands mount

        radius = std::min(MATRIX_WIDTH, MATRIX_HEIGHT) / 2 - 0.5;

        g()->Clear();
        g()->DrawSafeCircle(MATRIX_WIDTH/2, MATRIX_HEIGHT/2, 1, CRGB::Blue);

        // Convert hour, minute, and second to angles
        uint8_t hourAngle = (hours % 12) * 21; // (360 / 12) / 1.41 = 21
        uint8_t minuteAngle = minutes * 4; // (360 / 60) / 1.41 = 4
        uint8_t secondAngle = seconds * 4;

        // Calculate hand positions
        int hourX = MATRIX_CENTER_X + ((radius-3) * 3 * (cos8(hourAngle) - 128) / 512);
        int hourY = MATRIX_CENTER_Y + ((radius-3) * 3 * (sin8_C(hourAngle) - 128) / 512);
        
        int minuteX = MATRIX_CENTER_X + (radius * (cos8(minuteAngle) - 128) / 128);
        int minuteY = MATRIX_CENTER_Y + (radius * (sin8(minuteAngle) - 128) / 128);
        
        int secondX = MATRIX_CENTER_X + (radius * (cos8(secondAngle) - 128) / 128);
        int secondY = MATRIX_CENTER_Y + (radius * (sin8(secondAngle) - 128) / 128);

        // Draw the hands
        g()->drawLine(MATRIX_CENTER_X, MATRIX_CENTER_Y, hourX, hourY, CRGB::Yellow);   // Hour hand
        g()->drawLine(MATRIX_CENTER_X, MATRIX_CENTER_Y, minuteX, minuteY, CRGB::Yellow); // Minute hand
        g()->drawLine(MATRIX_CENTER_X, MATRIX_CENTER_Y, secondX, secondY, CRGB::White); // Second hand

        g()->DrawSafeCircle(MATRIX_WIDTH/2, MATRIX_HEIGHT/2, radius, CRGB::Blue);
        g()->DrawSafeCircle(MATRIX_WIDTH/2, MATRIX_HEIGHT/2, radius+1, CRGB::Green);

        for (int z = 0; z < 12*21; z += 21) 
        {   // 21 approximates 30 degrees in 0-255 system
            // Convert the angle from 0-255 range to 0-360 degrees approximation
            uint8_t angle = z;

            // Calculate the start and end points of the tick marks
            int x2 = (MATRIX_CENTER_X + ((radius - 4) * (sin8_C(angle) - 128) / 128));
            int y2 = (MATRIX_CENTER_Y - ((radius - 4) * (cos8(angle) - 128) / 128));
            int x3 = (MATRIX_CENTER_X + ((radius - 1) * (sin8_C(angle) - 128) / 128));
            int y3 = (MATRIX_CENTER_Y - ((radius - 1) * (cos8(angle) - 128) / 128));

            // Draw the tick mark
            g()->drawLine(x2, y2, x3, y3, CRGB::Red);
        }
    }
};

#endif
