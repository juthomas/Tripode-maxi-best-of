#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "WiFi.h"
#include <Wire.h>
#include <Button2.h>
#include "esp_adc_cal.h"
#include <WiFiUdp.h>

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23

#define TFT_BL              4   // Display backlight control pin
#define ADC_EN              14  //ADC_EN is the ADC detection enable port
#define ADC_PIN             34
#define BUTTON_1            35
#define BUTTON_2            0

#define MOTOR_1				25
#define MOTOR_2				26
#define MOTOR_3				27

const int motorFreq = 5000;
const int motorResolution = 8;
const int motorChannel1 = 0;
const int motorChannel2 = 1;
const int motorChannel3 = 2;

typedef	struct 	s_data_task
{
	int duration;
	int pwm;
	int motor_id;
	TaskHandle_t thisTaskHandler;
}				t_data_task;


// typedef void(*t_task_func)(void *param);



t_data_task g_data_task[3];

int vref = 1100;

#define BLACK 0x0000
#define WHITE 0xFFFF
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);


// const char* ssid = "Matrice";
const char* ssid = "Freebox-0E3EAE";
// const char* password =  "QGmatrice";
const char* password =  "taigaest1chien";

WiFiUDP Udp;
unsigned int localUdpPort = 49141;
char incomingPacket[255];
String convertedPacket;
char  replyPacket[] = "Message received";


void set_pwm0(int pwm);
void set_pwm1(int pwm);
void set_pwm2(int pwm);

typedef void(*t_set_pwm)(int pwm);

#define TASK_NUMBER 3
static const t_set_pwm	g_set_pwm[TASK_NUMBER] = {
	(t_set_pwm)set_pwm0,
	(t_set_pwm)set_pwm1,
	(t_set_pwm)set_pwm2,
};

void stop_pwm0(void);
void stop_pwm1(void);
void stop_pwm2(void);

typedef void(*t_stop_pwm)(void);

#define TASK_NUMBER 3
static const t_stop_pwm	g_stop_pwm[TASK_NUMBER] = {
	(t_stop_pwm)stop_pwm0,
	(t_stop_pwm)stop_pwm1,
	(t_stop_pwm)stop_pwm2,
};



bool	timersActives[3];
int		pwmValues[3];
int		timerPansements[3];
hw_timer_t * timers[3];



const char* wl_status_to_string(int ah) {
	switch (ah) {
		case WL_NO_SHIELD: return "WL_NO_SHIELD";
		case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
		case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
		case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
		case WL_CONNECTED: return "WL_CONNECTED";
		case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
		case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
		case WL_DISCONNECTED: return "WL_DISCONNECTED";
		default: return "ERROR NOT VALID WL";
	}
}

const char* eTaskGetState_to_string(int ah) {
	switch (ah) {
		case eRunning: return "eRunning";
		case eReady: return "eReady";
		case eBlocked: return "eBlocked";
		case eSuspended: return "eSuspended";
		case eDeleted: return "eDeleted";
		default: return "ERROR NOT STATE";
	}
}




void showVoltage()
{
	static uint64_t timeStamp = 0;
	if (millis() - timeStamp > 1000) {
		timeStamp = millis();
		uint16_t v = analogRead(ADC_PIN);
		float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
		String voltage = "Voltage :" + String(battery_voltage) + "V";
		Serial.println(voltage);
		tft.fillScreen(TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.drawString(voltage,  tft.width() / 2, tft.height() / 2 );
	}
}

void setup() {
	// put your setup code here, to run once:
	Serial.begin(115200);
	WiFi.begin(ssid, password);
	timersActives[0] = false;
	timersActives[1] = false;
	timersActives[2] = false;

	pwmValues[0] = 0;
	pwmValues[1] = 0;
	pwmValues[2] = 0;

	timers[0] = timerBegin(0, 80, true);
	timers[1] = timerBegin(1, 80, true);
	timers[2] = timerBegin(2, 80, true);

	ledcSetup(motorChannel1, motorFreq, motorResolution);
	ledcSetup(motorChannel2, motorFreq, motorResolution);
	ledcSetup(motorChannel3, motorFreq, motorResolution);
	ledcAttachPin(MOTOR_1, motorChannel1);
	ledcAttachPin(MOTOR_2, motorChannel2);
	ledcAttachPin(MOTOR_3, motorChannel3);



	pinMode(ADC_EN, OUTPUT);
	digitalWrite(ADC_EN, HIGH);
	tft.init();
	tft.setRotation(0);
	tft.fillScreen(TFT_BLACK);



	if (TFT_BL > 0) {                           // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
			pinMode(TFT_BL, OUTPUT);                // Set backlight pin to output mode
			digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
	}

    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(0, 0);
    tft.setTextDatum(MC_DATUM);

	tft.drawString("Connecting", tft.width() / 2, tft.height() / 2);
	uint64_t timeStamp = millis();
	
	Serial.println("Connecting");
	while (WiFi.status() != WL_CONNECTED)
	{
		if(millis() - timeStamp > 10000)
		{
			ESP.restart();
			tft.fillScreen(TFT_BLACK);
			tft.drawString("Restarting", tft.width() / 2, tft.height() / 2);
			
		}
		delay(500);
		Serial.println(wl_status_to_string(WiFi.status()));
	tft.fillScreen(TFT_BLACK);
		
		tft.drawString(wl_status_to_string(WiFi.status()), tft.width() / 2, tft.height() / 2);
	}
	Serial.print("Connected, IP address: ");
	Serial.println(WiFi.localIP());
	Udp.begin(localUdpPort);
	Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
}




void drawMotorsActivity()
{
	TFT_eSprite drawing_sprite = TFT_eSprite(&tft);

  drawing_sprite.setColorDepth(8);
  drawing_sprite.createSprite(tft.width(), tft.height());
	drawing_sprite.fillSprite(TFT_BLACK);
	    drawing_sprite.setTextSize(1);
		drawing_sprite.setTextFont(1);
    drawing_sprite.setTextColor(TFT_GREEN);
    drawing_sprite.setTextDatum(MC_DATUM);
	drawing_sprite.setCursor(0, 0);
	drawing_sprite.printf("Ssid : %s\n\nIp : %s\n\nUdp port : %d\n\n", ssid,WiFi.localIP().toString().c_str(), localUdpPort);
	uint16_t v = analogRead(ADC_PIN);
	float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
	drawing_sprite.printf("Voltage : %.2fv\n", battery_voltage);

	
	//uint32_t color1 = TFT_BLUE;
	uint32_t color2 = TFT_WHITE;
	drawing_sprite.drawCircle(67, 120 , 26, color2);
	drawing_sprite.drawCircle(27, 190 , 26, color2);
	drawing_sprite.drawCircle(108, 190 , 26, color2);
	drawing_sprite.drawLine(15, 167, 40, 150, color2);
	drawing_sprite.drawLine(40, 150, 40, 120, color2);
	drawing_sprite.drawLine(93, 120, 93, 150, color2);
	drawing_sprite.drawLine(93, 150, 120, 167, color2);
	drawing_sprite.drawLine(100, 215, 67, 195, color2);
	drawing_sprite.drawLine(67, 195, 35, 215, color2);

			//drawing_sprite.drawCircle(TFT_WIDTH / 2, TFT_HEIGHT/4 * i + TFT_HEIGHT/4 , 20, TFT_BLUE);
	if (timerAlarmEnabled(timers[0]))
	{
		drawing_sprite.fillCircle(67, 120 ,  pwmValues[0] / 11, TFT_BLUE);
		// Serial.printf("Seconds lefts : %lf\n", timerAlarmReadSeconds(timers[0]));
	}
	if (timerAlarmEnabled(timers[1]))
	{
		drawing_sprite.fillCircle(27, 190 ,  pwmValues[1] / 11, TFT_BLUE);
	}
	if (timerAlarmEnabled(timers[2]))
	{
		drawing_sprite.fillCircle(108, 190 ,  pwmValues[2] / 11, TFT_BLUE);
	}
	drawing_sprite.pushSprite(0, 0);
	drawing_sprite.deleteSprite();
	
}


void	set_pwm0(int pwm)
{
	ledcWrite(0, pwm);
}

void	set_pwm1(int pwm)
{
	ledcWrite(1, pwm);
}

void	set_pwm2(int pwm)
{
	ledcWrite(2, pwm);
}

void	stop_pwm0(void)
{
	if (timerPansements[0] <= 0)
	{
		Serial.printf("End of timer 0\n");
		ledcWrite(0, 0);
		timerAlarmDisable(timers[0]);	}
	timerPansements[0]--;
	// timerAlarmDisable(timers[0]);
}

void	stop_pwm1(void)
{
	// ledcWrite(1, 0);
	if (timerPansements[1] <= 0)
	{
		Serial.printf("End of timer 1\n");
		ledcWrite(1, 0);
		timerAlarmDisable(timers[1]);
	}
	timerPansements[1]--;
}

void	stop_pwm2(void)
{
	ledcWrite(2, 0);
	if (timerPansements[2] <= 0)
	{
		Serial.printf("End of timer 2\n");
		ledcWrite(2, 0);
		timerAlarmDisable(timers[2]);	
	}
	timerPansements[2]--;
}


void loop() {
	int packetSize = Udp.parsePacket();
	if (packetSize)
	{
		int len = Udp.read(incomingPacket, 255);
		if (len > 0)
		{
			incomingPacket[len] = 0;
		}
		Serial.printf("UDP packet contents: %s\n", incomingPacket);
		convertedPacket = String(incomingPacket);
		Serial.println(convertedPacket);
		Serial.printf("Debug indexof = P:%d, D:%d, I:%d\n",convertedPacket.indexOf('P'), convertedPacket.indexOf('D'), convertedPacket.indexOf("I"));
		
		if (convertedPacket.indexOf("P") > -1 && convertedPacket.indexOf("D") > -1 && convertedPacket.indexOf("I") > -1 )
		{
			//t_data_task dataTask;

			int duration = convertedPacket.substring(convertedPacket.indexOf("D") + 1).toInt();
			int intensity = convertedPacket.substring(convertedPacket.indexOf("I") + 1).toInt();
			int pin = convertedPacket.substring(convertedPacket.indexOf("P") + 1).toInt();
			if (pin == 0)
			{
				//Stop Previous timer
				if (timerAlarmEnabled(timers[0]))
				{
					timerAlarmDisable(timers[0]);
				}
				pwmValues[0] = intensity;
				g_set_pwm[0](intensity);
				
				timerAttachInterrupt(timers[0], &stop_pwm0, false);
				timerPansements[0] = 1;
				timerAlarmWrite(timers[0], duration * 1000, true);
				timerAlarmEnable(timers[0]);
				
			}
			else if (pin == 1)
			{
				//Stop Previous timer
				if (timerAlarmEnabled(timers[1]))
				{
					timerAlarmDisable(timers[1]);
				}
				pwmValues[1] = intensity;
				g_set_pwm[1](intensity);
				
				timerAttachInterrupt(timers[1], &stop_pwm1, false);
				timerPansements[1] = 1;
				timerAlarmWrite(timers[1], duration * 1000, true);
				timerAlarmEnable(timers[1]);
			}
			else if (pin == 2)
			{
				//Stop Previous timer
				if (timerAlarmEnabled(timers[2]))
				{
					timerAlarmDisable(timers[2]);
				}
				pwmValues[2] = intensity;
				g_set_pwm[2](intensity);
				
				timerAttachInterrupt(timers[2], &stop_pwm2, false);
				timerPansements[2] = 1;
				timerAlarmWrite(timers[2], duration * 1000, true);
				timerAlarmEnable(timers[2]);
			}


		}
		
		
	}
	drawMotorsActivity();
	//Serial.println(".");
	delay(100);
	// put your main code here, to run repeatedly:
}