/** A Particle Photon firmware for visualizing household power and
 *  outdoor temperature using the Adafruit Spark NeoPixel Ring with 24 Neopixels:
 * 
 *  https://www.adafruit.com/products/2268
 * 
 *  Author: Marcus Kempe, makem86@gmail.com. 
 *  Github: https://github.com/plopp/
 *  Source License:
 * 
 *  Copyright (c) 2015 Marcus Kempe
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights to
 *  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of 
 *  the Software, and to permit persons to whom the Software is furnished to do so, 
 *  subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all 
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
 *  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
 *  PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
 *  FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
 *  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **/

#include "HttpClient/HttpClient.h"
#include "SparkJson/SparkJson.h"
#include "MQTT/MQTT.h"
#include "application.h"
#include "neopixel/neopixel.h"
#include "math.h"

SYSTEM_MODE(AUTOMATIC);

#define PIXEL_PIN D6
#define PIXEL_COUNT 24
#define PIXEL_TYPE WS2812B

#define DAY_BRIGHTNESS 80
#define NIGHT_BRIGHTNESS 10

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

//MQTT-init
void callback(char* topic, uint8_t* payload, unsigned int length);
byte server[] = { 192,168,1,160 }; //Server IP-address for MQTT
MQTT client(server, 1883, callback);
#define MQTT_POWER_TOPIC "/actions/domoticz/elvaco1"

//One color array for each mode
uint32_t energycolors[24] = {0};
uint32_t weathercolors[24] = {0};

//HTTP-client init
HttpClient http;
http_header_t headers[] = {
    //  { "Content-Type", "application/json" },
    //  { "Accept" , "application/json" },
    { "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};

//Weather API server and Sunset/Sunrise from Domoticz
#define WEATHER_API_IP "192.168.1.160"
#define WEATHER_API_PORT 8081
#define WEATHER_API_PATH "/weather"

#define SUN_API_IP "192.168.1.160"
#define SUN_API_PORT 8080
#define SUN_API_PATH "/json.htm?type=command&param=getSunRiseSet"

#define RED strip.Color(255,0,0)
#define GREEN strip.Color(0,255,0)
#define BLUE strip.Color(0,0,255)
#define BLACK strip.Color(0,0,0)
#define WHITE strip.Color(255,255,255)

//Variable init
byte brightness = DAY_BRIGHTNESS;
bool initDone;
float fpower = 0.0;
double tick = 0; //Day and night tick
double wtick = 0; //Weather service tick

/**
 * Function for getting the weather from now up until upcoming midnight.
 * Expects a jsonmessage on the following format, with no more 
 * than 24 objects in the array and not less than 1 object: 
 * 
 *      [
 *          {
 *              "timestamp": 1446498000, 
 *              "precipitation": 0, 
 *              "temperature": 10.6
 *          }, 
 *          {
 *              "timestamp": 1446501600, 
 *              "precipitation": 0, 
 *              "temperature": 10.8
 *          }, 
 *          {
 *              "timestamp": 1446505200, 
 *              "precipitation": 0, 
 *              "temperature": 10.9
 *          }, 
 *          {
 *              "timestamp": 1446508800, 
 *              "precipitation": 0, 
 *              "temperature": 11.0
 *          },
 *          ...
 *       ]
 * 
 **/
void getWeather(){
    http_request_t request;
    http_response_t response;
    
    request.hostname = WEATHER_API_IP;
    request.port = WEATHER_API_PORT;
    request.path = WEATHER_API_PATH;
    
    http.get(request, response, headers);
    String responsebody(response.body);
    
    StaticJsonBuffer<2000> jsonBuffer;
    char responsebuffer[2000] = {'\0'};
    responsebody.toCharArray(responsebuffer,2000);
    JsonArray& array = jsonBuffer.parseArray(responsebuffer);

    double temp = array[0]["temperature"];
    String tempstring(temp);

    int i = 0;
    for(JsonArray::iterator it=array.begin(); it!=array.end(); ++it) 
    {
        // *it contains the JsonVariant which can be casted as usuals
        JsonObject& value = *it;
        int iprec = value["precipitation"];
        long dtimestamp = value["timestamp"];
        double dtemperature = value["temperature"];
        int itemp = (int)lround(dtemperature);
        if(i == 0){ //Shows the temperature of this hour
            if(itemp > 0){
                int j;
                for(j=1;j<24;j++){
                    if(j <= itemp) weathercolors[j] = RED;   
                    else if(j > itemp) weathercolors[j] = BLACK;   
                }
                weathercolors[0] = strip.Color(0,0,0); //Reset zero degree led 
            }
            else if(itemp < 0){
                int j;
                unsigned int uitemp = itemp + (-2)*itemp;
                for(j=1;j<24;j++){
                    if(j <= uitemp) weathercolors[24-j] = BLUE;   
                    else if(j > uitemp) weathercolors[24-j] = BLACK;   
                    
                }
                weathercolors[0] = BLACK; //Reset zero degree led
            }
            else{
                weathercolors[0] = WHITE;
            }
        }
        if(iprec > 0 && itemp < 0){
            weathercolors[0] = RED; //Red precipitation marker at sub-zero outdoor temperatures
        }
        else if(iprec > 0 && itemp > 0){
            weathercolors[0] = BLUE; //Blue precipitation marker at sub-zero outdoor temperatures
        }
        i++;
    }
}

/**
 * Callback function for received MQTT-messages 
 * Expects a jsonmessage on the following format as payload: 
 * 
 *      {"SensorType": "electricity", "svalue": "843.038562;9964790.000000"}
 * 
 * Where svalue is a semi-colon delimited string on the format: <power in W>;<energy in Wh>
 **/
void callback(char* topic, byte* payload, unsigned int length) {
    initDone = true;
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(p);
    
    const char* svalue = root["svalue"];
    String value(svalue);
    int semi = value.indexOf(";");
    String spower = value.substring(0,semi);
    fpower = spower.toFloat();
    
    int fpower = spower.toInt();
    int leds = fpower/100;
    int total = 19;
    
    if(leds > total){
        leds = total;
    }
    if(leds < 1){
        leds = 1;
    }
    
    uint16_t i;
    for(i=0; i<24; i++){
        if(i < leds){
            energycolors[i] = hsv2rgb(110-(110/(total-1))*i,1.0,0.5);
        }
        else{
            energycolors[i] = 0;
        }
    }
}

/**
 * Function for dimming the leds during night. This function calls the function getSunRiseSet from Domoticz.
 * Expects a jsonmessage on the following format when calling the API via HTTP: 
 * 
 *      { 
 *          "ServerTime" : "Oct 31 2015 19:13:33", 
 *          "Sunrise" : "07:23:00", 
 *          "Sunset" : "16:28:00", 
 *          "status" : "OK", 
 *          "title" : "getSunRiseSet" 
 *      }
 * 
 * Where svalue is a semi-colon delimited string on the format: <power in W>;<energy in Wh>
 **/
void dimDisplay(){
    http_request_t request;
    http_response_t response;

    request.hostname = SUN_API_IP;
    request.port = SUN_API_PORT;
    request.path = SUN_API_PATH;

    // Get request
    http.get(request, response, headers);
    
    //String responsestatus(response.status);
    String responsebody(response.body);
    
    //Expects json on this format: 
    StaticJsonBuffer<200> jsonBuffer;
    char responsebuffer[200] = {'\0'};
    responsebody.toCharArray(responsebuffer,200);
    
    JsonObject& root = jsonBuffer.parseObject(responsebuffer);
    
    const char* csunrise = root["Sunrise"]; //HH:MM:SS
    const char* csunset = root["Sunset"]; //HH:MM:SS
    
    String sunrise(csunrise);
    String sunset(csunset);
    
    int isethour = sunset.substring(0,2).toInt();
    int isetminute = sunset.substring(3,5).toInt();
    int isetsecond = sunset.substring(6,8).toInt();
    
    int irisehour = sunrise.substring(0,2).toInt();
    int iriseminute = sunrise.substring(3,5).toInt();
    int irisesecond = sunrise.substring(6,8).toInt();
    
    //Time now
    int hournow = Time.hour();
    int minutenow = Time.minute();
    int secondnow = Time.second();
    int totalMinutesNow = hournow*60+minutenow;
    int totalMinutesRise = irisehour*60+iriseminute;
    int totalMinutesSet = isethour*60+isetminute;

    if(totalMinutesNow > totalMinutesRise && totalMinutesNow < totalMinutesSet){
        brightness = DAY_BRIGHTNESS; //Day
    }
    else{
        brightness = NIGHT_BRIGHTNESS; //Night
    }
    tick = 0;
}

void setup() 
{
    initDone = false;
    Time.zone(1);
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'

    client.connect("sparkclient"); // connect to the MQTT server
    
    RGB.control(true); //Take over the Photons led...
    RGB.brightness(0); //...and shut it off.
    
    dimDisplay();
    getWeather();

    if (client.isConnected()) {
        client.subscribe(MQTT_POWER_TOPIC);
    }
}

void loop() 
{
    if(wtick == 0){
        if (!client.isConnected()){
            client.connect("sparkclient"); // connect to the MQTT server   
            client.subscribe(MQTT_POWER_TOPIC);
        }
    }
    if(wtick == 4000){ //Fetch the weather every 4000 ticks.
        getWeather();
        wtick = 0;
    }
    if(tick == 4000){ //Check time and dim the display every 4000 ticks.
        dimDisplay();
    }
    if(initDone){
        uint16_t i;
        if(tick < 2000){ 
            //Go into temperature-state when tick is 0-1999
            for(i=0; i<24; i++){
                strip.setPixelColor(i, weathercolors[i]);    
                if(tick <= brightness){
                    strip.setBrightness(tick);
                }
                else if(tick >= (2000-brightness)){
                    strip.setBrightness(1999 - tick);
                }
                else{
                    strip.setBrightness(brightness);   
                }
            }
        }
        else{ 
            //Go into power-state when tick is 2000-4000
            for(i=0; i<24; i++){
                strip.setPixelColor(i, energycolors[(i+9)%24]);    
                if(tick <= (2000+brightness)){
                    strip.setBrightness(tick - 2000); //Smooth transition in brightness going into this state
                }
                else if(tick >= (4000-brightness)){
                    strip.setBrightness(4000 - tick); //Smooth transition in brightness going out from this state
                }
                else{
                    strip.setBrightness(brightness);   
                }
            }
        }
        strip.show();
    }
    else{
        //Circulate in pink while waiting for weather and energy data.
        uint16_t i;
        for(i=0; i<24; i++){
            strip.setPixelColor(i, strip.Color(200,10,100));   
            strip.setBrightness(brightness);
            strip.show();
            delay(50);
            strip.setPixelColor(i, 0);   
            strip.setBrightness(brightness);
            strip.show();
        }
    }
    if (client.isConnected()) {
        client.loop();
    }
    delay(1);
    tick++;
    wtick++;
}

/**
 * Function for translating hsv to rgb. Using HSV gives a better green to red color scale, but RGB is needed for the LEDs.
 * 
 * Parameters:
 * Hue h: 0.0-360.0°
 * Saturation s: 0.0-1.0
 * Value v: 0.0-1.0
 **/
uint32_t hsv2rgb(float h, float s, float v) {
    float r, g, b, f, p, q, t;
    int i;
    h = h/360;
    i = (int)(h * 6);
    f = h * 6 - i;
    p = v * (1 - s);
    q = v * (1 - f * s);
    t = v * (1 - (1 - f) * s);
    switch (i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }
    return strip.Color((int)(r*255),(int)(g*255),(int)(b*255));
}
