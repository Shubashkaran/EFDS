#include "application.h"
#line 1 "/Users/HomePc/Documents/EFDS/src/EFDS.ino"
#include <HC_SR04.h>

#line 4 "/Users/HomePc/Documents/EFDS/src/EFDS.ino"
#define QUIP "allegory of nick cave"
#define SAMPLE_RATE_MILLIS (5 * 60 * 1000) 
#define REPORT_RATE_SEC (15 * 60)
#define REPORT_RATE_MILLIS (REPORT_RATE_SEC * 1000) 
#define REPORT_DELTA_CM 1.0 
#define LOW_POWER_THRESHOLD 20
#define LOW_POWER_SLEEP_SEC (60 * 60)
#define CRITICAL_POWER_THRESHOLD 5
#define CRITICAL_POWER_SLEEP_SEC (24 * 60 * 60)
#define SAMPLE_COUNT 5
#define SAMPLE_PAUSE_MILLIS 5*1000
#define SLEEP_ROUTINE_MILLIS 5*1000
#define RECONNECT_SPIN_MILLIS 1000

int compare_ints(const void* a, const void* b);
double median_sample();
void setup();
void publish(double sample, unsigned long timestamp);
void loop();
void blinkLed();
double last_reported_sample = -3.0;
double delta = 0.0; 
unsigned long lastReportTime = 0; 
int trigPin = D4;
int echoPin = D5;

SYSTEM_MODE(SEMI_AUTOMATIC);

HC_SR04 rangefinder = HC_SR04(trigPin, echoPin);
FuelGauge battery;

double samples[SAMPLE_COUNT];
    
int compare_ints(const void* a, const void* b)
{
    double arg1 = *(const double*)a;
    double arg2 = *(const double*)b;
 
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

double median_sample() {
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        samples[i] = rangefinder.getDistanceCM();
        Particle.process();
        delay(SAMPLE_PAUSE_MILLIS);
    }

    qsort(samples, SAMPLE_COUNT, sizeof(double), compare_ints);

    return samples[SAMPLE_COUNT/2];
}

void setup()
{
    Serial.begin(9600);
    pinMode(D7, OUTPUT);
    Particle.variable("cm", &last_reported_sample, DOUBLE);
    Particle.connect();
    Particle.process();
    Particle.publish("floodtracker/quip", QUIP, PUBLIC | NO_ACK);
    Serial.println(QUIP);
}

void publish(double sample, unsigned long timestamp) {
    Serial.printf("%.2fcm\tat %d\n", sample, timestamp);
    Particle.publish("level_mm", String::format("%d", (int)(sample * 10.0)), PUBLIC | NO_ACK);
    Particle.publish("floodtracker/level_mm", String::format("%d", (int)(sample * 10.0)), PUBLIC | NO_ACK);

    if (5 == SAMPLE_COUNT) {
        Particle.publish("floodtracker/levels_cm",
        String::format("%f %f %f %f %f", samples[0], samples[1], samples[2], samples[3], samples[4]),
        PUBLIC | NO_ACK);
    }
}

void loop()
{
    Particle.process();
    double current_sample = median_sample();
    
    unsigned long now = millis();
    
    Serial.printf("%.2fcm\tat %d\treport in %ds\n", 
                  current_sample,
                  now,
                  (REPORT_RATE_MILLIS - (now - lastReportTime)) / 1000);
    
    
    delta = abs(last_reported_sample - current_sample);
    
    
    bool been_a_while = ((now - lastReportTime) >= REPORT_RATE_MILLIS);
    bool big_change = (delta > REPORT_DELTA_CM);
    
    
    if( been_a_while || big_change ) {
        publish(current_sample, now);
        Particle.publish("floodtracker/battery", String::format("%f", battery.getSoC()), PUBLIC | NO_ACK);
        Serial.printf("published\n");
        lastReportTime = now;

    }
    if (big_change) {
        last_reported_sample = current_sample;
    }
    

    long sleep_plan = REPORT_RATE_SEC;
    float charge = battery.getSoC();
    if (charge < LOW_POWER_THRESHOLD) {
        sleep_plan = LOW_POWER_SLEEP_SEC;
    }
    if (charge < CRITICAL_POWER_THRESHOLD) {
        sleep_plan = CRITICAL_POWER_SLEEP_SEC;
    }
    Particle.publish("floodtracker/sleep_plan", String::format("%d", sleep_plan), PUBLIC | NO_ACK);

    Particle.process();
    delay(SLEEP_ROUTINE_MILLIS);
    Particle.process();
    System.sleep(SLEEP_MODE_DEEP, sleep_plan);
}

void blinkLed() {
    digitalWrite(D7,HIGH);
    delay(150);   
    digitalWrite(D7,LOW);    
}
