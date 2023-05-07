//+--------------------------------------------------------------------------
//
// File:        PatternWeather.h
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
//
// Description:
//
//   Gets the weather for a given zip code
//
// History:     Jun-25-202         Davepl      Adapted from own code
//
//---------------------------------------------------------------------------

#ifndef PatternWeather_H
#define PatternWeather_H

#include <Arduino.h>
#include <string.h>
#include <HTTPClient.h>
#include <ledstripeffect.h>
#include <ledmatrixgfx.h>
#include <secrets.h>
#include <RemoteDebug.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include "globals.h"
#include "deviceconfig.h"
#include "jsonserializer.h"
#include <FontGfx_apple5x7.h>
#include <thread>

#define WEATHER_INTERVAL_SECONDS (10*60)

static const String pszDaysOfWeek[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };

static const String pszWeatherIcons[] = {   "",                                 // 00 Unused
                                            "/bmp/clearsky.jpg",                // 01
                                            "/bmp/fewclouds.jpg",               // 02
                                            "/bmp/scatteredclouds.jpg",         // 03
                                            "/bmp/brokenclouds.jpg",            // 04
                                            "/bmp/testcloud.jpg",               // Unused slots
                                            "",
                                            "",
                                            ""
                                            "/bmp/showerrain.jpg",              // 09
                                            "/bmp/rain.jpg",                    // 10
                                            "/bmp/thunderstorm.jpg",            // 11
                                            "",
                                            "/bmp/snow.jpg",                    // 13
                                        };

class PatternWeather : public LEDStripEffect
{

private:

    String strLocationName       = "";
    String strLocation           = "";
    String strCountryCode        = "";
    String strLatitude           = "0.0";
    String strLongitude          = "0.0";
    int    dayOfWeek             = 0;
    int    iconToday             = -1;
    int    iconTomorrow          = -1;
    float  temperature           = 0.0f;
    float  pressure              = 0.0f;
    float  highToday             = 0.0f;
    float  loToday               = 0.0f;
    float  highTomorrow          = 0.0f;
    float  loTomorrow            = 0.0f;

    bool   dataReady             = false;
    TaskHandle_t weatherTask     = nullptr;
    bool   updateInProgress      = false;
    time_t latestUpdate          = 0;


    // The weather is obviously weather, and we don't want text overlaid on top of our text

    virtual bool ShouldShowTitle() const
    {
        return false;
    }

    virtual size_t DesiredFramesPerSecond() const override
    {
        return 25;
    }

    inline float KelvinToFarenheit(float K)
    {
        return (K - 273.15) * 9.0f/5.0f + 32;
    }

    inline float KelvinToCelsius(float K)
    {
        return K - 273.15;
    }
    
    inline float KelvinToLocal(float K)
    {
        if (g_aptrDeviceConfig->UseCelsius())
            return KelvinToCelsius(K);
        else
            return KelvinToFarenheit(K);
    }

    bool updateCoordinates() 
    {
        HTTPClient http;
        String url;

        if (!HasLocationChanged())
            return false;

        const String& configLocation = g_aptrDeviceConfig->GetLocation();
        const String& configCountryCode = g_aptrDeviceConfig->GetCountryCode();
        const bool configLocationIsZip = g_aptrDeviceConfig->IsLocationZip();

        if (configLocationIsZip) 
            url = "http://api.openweathermap.org/geo/1.0/zip?zip=" + configLocation + "," + configCountryCode + "&appid=" + g_aptrDeviceConfig->GetOpenWeatherAPIKey();
        else
            url = "http://api.openweathermap.org/geo/1.0/direct?q=" + configLocation + "," + configCountryCode + "&limit=1&appid=" + g_aptrDeviceConfig->GetOpenWeatherAPIKey();

        http.begin(url);
        int httpResponseCode = http.GET();

        if (httpResponseCode <= 0) 
        {
            debugW("Error fetching coordinates for location: %s", configLocation);
            http.end();
            return false;
        }

        String response = http.getString();
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, response);
        JsonObject coordinates = configLocationIsZip ? doc.as<JsonObject>() : doc[0].as<JsonObject>();

        strLatitude = coordinates["lat"].as<String>();
        strLongitude = coordinates["lon"].as<String>();

        http.end();

        strLocation = configLocation;
        strCountryCode = configCountryCode;

        return true;
    }

    // getTomorrowTemps
    //
    // Request a forecast and then parse out the high and low temps for tomorrow

    bool getTomorrowTemps(float& highTemp, float& lowTemp) 
    {
        HTTPClient http;
        String url = "http://api.openweathermap.org/data/2.5/forecast?lat=" + strLatitude + "&lon=" + strLongitude + "&appid=" + g_aptrDeviceConfig->GetOpenWeatherAPIKey();
        http.begin(url);
        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) 
        {
            String response = http.getString();
            DynamicJsonDocument doc(4096);
            deserializeJson(doc, response);
            JsonArray list = doc["list"];

            // Get tomorrow's date
            time_t tomorrow = time(nullptr) + 86400;
            tm* tomorrowTime = localtime(&tomorrow);
            char dateStr[11];
            strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", tomorrowTime);

            iconTomorrow = -1;
            
            // Look for the temperature data for tomorrow
            for (size_t i = 0; i < list.size(); ++i) 
            {
                JsonObject entry = list[i];
                String dt_txt = entry["dt_txt"];
                if (dt_txt.startsWith(dateStr)) 
                {
                    //Serial.printf("Weather: Updating Forecast: %s", response.c_str());
                    JsonObject main = entry["main"];
                    if (main["temp_max"] > 0)
                        highTemp        = KelvinToLocal(main["temp_max"]);
                    if (main["temp_min"] > 0)
                        lowTemp         = KelvinToLocal(main["temp_min"]);
                    
                    String iconIdTomorrow = entry["weather"][0]["icon"];
                    iconTomorrow = iconIdTomorrow.toInt();            
                    if (iconTomorrow < 1 || iconTomorrow >= ARRAYSIZE(pszWeatherIcons))
                        iconTomorrow = -1;

                    debugI("Got tomorrow's temps: Lo %d, Hi %d, Icon %d", (int)lowTemp, (int)highTemp, iconTomorrow);                        
                    break;
                }
            }
            http.end();
            return true;
        }
        else 
        {
            debugW("Error fetching forecast data for location: %s in country: %s", strLocation.c_str(), strCountryCode.c_str());
            http.end();
            return false;
        }
    }

    // getWeatherData
    //
    // Get the current temp and the high and low for today

    bool getWeatherData()
    {
        HTTPClient http;

        String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + strLatitude + "&lon=" + strLongitude + "&appid=" + g_aptrDeviceConfig->GetOpenWeatherAPIKey();
        http.begin(url);
        int httpResponseCode = http.GET();
        if (httpResponseCode > 0)
        {
            iconToday = -1;
            String weatherData = http.getString();
            DynamicJsonDocument jsonDoc(4096);
            deserializeJson(jsonDoc, weatherData);

            // Once we have a non-zero temp we can start displaying things
            if (0 < jsonDoc["main"]["temp"])
                dataReady = true;

            temperature = KelvinToLocal(jsonDoc["main"]["temp"]);
            highToday   = KelvinToLocal(jsonDoc["main"]["temp_max"]);
            loToday     = KelvinToLocal(jsonDoc["main"]["temp_min"]);
            
            String iconIndex = jsonDoc["weather"][0]["icon"];
            iconToday = iconIndex.toInt();
            debugI("Got today's temps: Now %d Lo %d, Hi %d, Icon %d", (int)temperature, (int)loToday, (int)highToday, iconToday);
            if (iconToday < 1 || iconToday >= ARRAYSIZE(pszWeatherIcons))
                iconToday = -1;

            const char * pszName = jsonDoc["name"];
            if (pszName)
                strLocationName = pszName;

            http.end();
            return true;
        }
        else
        {
            debugW("Error fetching Weather data for location: %s in country: %s", strLocation.c_str(), strCountryCode.c_str());
            http.end();
            return false;
        }
    }

    // Thread entry point so we can update the weather data asynchronously.  The update thread exists
    // when it's done fetching, it is not persistent.

    static void UpdateWeather(void * pv)
    {
        PatternWeather * pObj = (PatternWeather *) pv;

        pObj->updateCoordinates();

        if (pObj->getWeatherData())
        {
            debugW("Got today's weather");
            if (pObj->getTomorrowTemps(pObj->highTomorrow, pObj->loTomorrow))
                debugI("Got tomorrow's weather");
            else
                debugW("Failed to get tomorrow's weather");
        }
        else
        {
            debugW("Failed to get today's weather");
        }
        auto task = pObj->weatherTask;
        pObj->weatherTask = nullptr;
        pObj->updateInProgress = false;
        debugW("Weather thread exiting");
        vTaskDelete(task);
    }

    bool HasLocationChanged()
    {
        String configLocation = g_aptrDeviceConfig->GetLocation();
        String configCountryCode = g_aptrDeviceConfig->GetCountryCode();

        return strLocation != configLocation || strCountryCode != configCountryCode;
    }

public:

    PatternWeather() : LEDStripEffect(EFFECT_MATRIX_WEATHER, "Weather")
    {
    }

    PatternWeather(const JsonObjectConst&  jsonObject) : LEDStripEffect(jsonObject)
    {
    }

    // We re-render the entire display every frame

    virtual void Draw() override
    {
        const int fontHeight = 7;
        const int fontWidth  = 5;
        const int xHalf      = MATRIX_WIDTH / 2 - 1;

        g()->fillScreen(CRGB(0, 0, 0));
        g()->fillRect(0, 0, MATRIX_WIDTH, 9, g()->to16bit(CRGB(0,0,128)));        

        g()->setFont(&Apple5x7);

        if (WiFi.isConnected())
        {
            static CEveryNSeconds timingObj(WEATHER_INTERVAL_SECONDS);

            time_t now;
            time(&now);

            // If location and/or country have changed, trigger an update regardless of timer, but 
            // not more than once every half a minute
            if ((timingObj || HasLocationChanged()) && (now - latestUpdate) >= 30)
            {
                if (!updateInProgress && nullptr == weatherTask)
                {   
                    latestUpdate = now;
                    updateInProgress = true;
                    
                    // Create a thread to update the weather data rather than blocking the draw thread

                    debugW("Spawning thread to check weather..");
                    xTaskCreatePinnedToCore(UpdateWeather, "Weather", 4096, (void *) this, NET_PRIORITY, &weatherTask, NET_CORE);
                }
                else
                {
                    debugW("Skipping weather fetch because previous update still in progress");
                }
            }
        }


        // Draw the graphics
        if (iconToday >= 0)
        {
            auto filename = pszWeatherIcons[iconToday];
            if (!filename.isEmpty())
                if (JDR_OK != TJpgDec.drawFsJpg(0, 10, filename))        // Draw the image
                    debugW("Could not display %s", filename);
        }    
        if (iconTomorrow >= 0)
        {
            auto filename = pszWeatherIcons[iconTomorrow];
            if (!filename.isEmpty())
                if (JDR_OK != TJpgDec.drawFsJpg(xHalf+1, 10, filename))        // Draw the image
                    debugW("Could not display %s", filename);
        }    

        // Print the town/city name, which we looked up via trhe zip code

        int x = 0;
        int y = fontHeight + 1;
        g()->setCursor(x, y);
        g()->setTextColor(WHITE16);
        String showLocation = strLocation;
        showLocation.toUpperCase();
        if (g_aptrDeviceConfig->GetOpenWeatherAPIKey().isEmpty())
            g()->print("No API Key");
        else
            g()->print((strLocationName.isEmpty() ? showLocation : strLocationName).substring(0, (MATRIX_WIDTH - 2 * fontWidth)/fontWidth));

        // Display the temperature, right-justified

        if (dataReady)
        {
            String strTemp((int)temperature);
            x = MATRIX_WIDTH - fontWidth * strTemp.length();
            g()->setCursor(x, y);
            g()->setTextColor(g()->to16bit(CRGB(192,192,192)));
            g()->print(strTemp);
        }

        // Draw the separator lines

        y+=1;

        g()->drawLine(0, y, MATRIX_WIDTH-1, y, CRGB(0,0,128));
        g()->drawLine(xHalf, y, xHalf, MATRIX_HEIGHT-1, CRGB(0,0,128));
        y+=2 + fontHeight;

        // Figure out which day of the week it is

        time_t today = time(nullptr);
        tm * todayTime = localtime(&today);
        const String pszToday = pszDaysOfWeek[todayTime->tm_wday];
        const String pszTomorrow = pszDaysOfWeek[ (todayTime->tm_wday + 1) % 7 ];

        // Draw the day of the week and tomorrow's day as well

        g()->setTextColor(WHITE16);
        g()->setCursor(0, MATRIX_HEIGHT);
        g()->print(pszToday);
        g()->setCursor(xHalf+2, MATRIX_HEIGHT);
        g()->print(pszTomorrow);

        // Draw the temperature in lighter white

        if (dataReady)
        {
            g()->setTextColor(g()->to16bit(CRGB(192,192,192)));
            String strHi((int) highToday);
            String strLo((int) loToday);
        
            // Draw today's HI and LO temperatures

            x = xHalf - fontWidth * strHi.length();
            y = MATRIX_HEIGHT - fontHeight;
            g()->setCursor(x,y);
            g()->print(strHi);
            x = xHalf - fontWidth * strLo.length();
            y+= fontHeight;
            g()->setCursor(x,y);
            g()->print(strLo);

            // Draw tomorrow's HI and LO temperatures

            strHi = String((int)highTomorrow);
            strLo = String((int)loTomorrow);
            x = MATRIX_WIDTH - fontWidth * strHi.length();
            y = MATRIX_HEIGHT - fontHeight;
            g()->setCursor(x,y);
            g()->print(strHi);
            x = MATRIX_WIDTH - fontWidth * strLo.length();
            y+= fontHeight;
            g()->setCursor(x,y);
            g()->print(strLo);
        }
    }
};

#endif
