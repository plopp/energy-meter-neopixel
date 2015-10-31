// This #include statement was automatically added by the Particle IDE.
#include "HttpClient/HttpClient.h"

#include "SparkJson/SparkJson.h"
#include "MQTT/MQTT.h"
#include "application.h"
#include "neopixel/neopixel.h"

SYSTEM_MODE(AUTOMATIC);

#define PIXEL_PIN D6
#define PIXEL_COUNT 24
#define PIXEL_TYPE WS2812B

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);


void callback(char* topic, uint8_t* payload, unsigned int length);

byte server[] = { 192,168,1,160 };
MQTT client(server, 1883, callback);

uint32_t colors[24] = {0};

bool initDone;

HttpClient http;

// Headers currently need to be set at init, useful for API keys etc.
http_header_t headers[] = {
    //  { "Content-Type", "application/json" },
    //  { "Accept" , "application/json" },
    { "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};

http_request_t request;
http_response_t response;

byte brightness = 80;

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
    int fpower = spower.toInt();

    int leds = fpower/100;
    int total = 19;
    
    if(leds > total){
        leds = total;
    }
    if(leds < 1){
        leds = 1;
    }
    
    String sleds(leds);
    
    uint16_t i;
    for(i=0; i<24; i++){
        if(i < leds){
            colors[i] = hsv2rgb(110-(110/(total-1))*i,1.0,0.5);
        }
        else{
            colors[i] = 0;
        }
        
    }
}

double tick = 0;
void dimDisplay(){
	//Get sunrise and sunset from domoticz
    request.hostname = "192.168.1.160";
    request.port = 8080;
    request.path = "/json.htm?type=command&param=getSunRiseSet";

    // Get request
    http.get(request, response, headers);
    
    //String responsestatus(response.status);
    String responsebody(response.body);    
    
    StaticJsonBuffer<200> jsonBuffer;
    char responsebuffer[200] = {'\0'};
    responsebody.toCharArray(responsebuffer,200);
    
    JsonObject& root = jsonBuffer.parseObject(responsebuffer);
    
    const char* csunrise = root["Sunrise"];
    const char* csunset = root["Sunset"];
    
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
    
    //Day
    if((hournow > irisehour && minutenow > iriseminute && secondnow > irisesecond) && (hournow < isethour && minutenow < isetminute && secondnow < isetsecond)){
        brightness = 80;        
    }
    //Night
    else if((hournow > isethour && minutenow > isetminute && secondnow > isetsecond) || (hournow < irisehour && minutenow < iriseminute && secondnow < irisesecond)){
        brightness = 10;        
    }
    tick = 0;
}

void setup() 
{
    initDone = false;
    Time.zone(1);
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
    
    // connect to the server
    client.connect("sparkclient");
    
    RGB.control(true);
    RGB.brightness(0);
    
    dimDisplay();

    // publish/subscribe
    if (client.isConnected()) {
        //client.publish("/outTopic","hello world");
        client.subscribe("/actions/domoticz/elvaco1");
        //client.subscribe("/particle/test");
    }
    
 
}

void loop() 
{
    if(tick == 4000){
        dimDisplay();
    }
    if(initDone){
        uint16_t i;
        for(i=0; i<24; i++){
            strip.setPixelColor(i, colors[(i+9)%24]);    
            strip.setBrightness(brightness);
        }
        strip.show();
    }
    else{
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
}

// h: 0-360
// s: 0-1
// v: 0-1

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