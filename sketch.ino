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



void setup() 
{
    initDone = false;
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
    
    // connect to the server
    client.connect("sparkclient");
    
    RGB.control(true);
    RGB.brightness(0);

    // publish/subscribe
    if (client.isConnected()) {
        client.publish("/outTopic","hello world");
        client.subscribe("/actions/domoticz/elvaco1");
        //client.subscribe("/particle/test");
    }
    
 
}
void loop() 
{
    if(initDone){
        uint16_t i;
        for(i=0; i<24; i++){
            strip.setPixelColor(i, colors[(i+9)%24]);    
            strip.setBrightness(50);
        }
        strip.show();
    }
    else{
        uint16_t i;
        for(i=0; i<24; i++){
            strip.setPixelColor(i, Wheel(100));   
            strip.setBrightness(50);
            strip.show();
            delay(50);
            strip.setPixelColor(i, 0);   
            strip.setBrightness(50);
            strip.show();
        }
    }
    if (client.isConnected()) {
        client.loop();
    }
    delay(1);
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