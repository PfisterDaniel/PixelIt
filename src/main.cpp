
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <PubSubClient.h> 
#include <LittleFS.h>
#include <TimeLib.h> 
#include <ArduinoJson.h> 
#include <Wire.h>
#include <Adafruit_GFX.h>
#define FASTLED_INTERNAL
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <LightDependentResistor.h> 
#include <DHTesp.h>
#include <SoftwareSerial.h>
// PixelIT Stuff 
#include "PixelItFont.h"
#include "Webinterface.h"
#include "Tools.h"

#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <flash_hal.h>
#include <FS.h>
#include "StreamString.h"


#define DEBUG 0
void FadeOut(int = 10, int = 0);
void FadeIn(int = 10, int = 0);



//// MQTT Config
bool mqttAktiv = false;
String  mqttUser = "";
String	mqttPassword = "";
String mqttServer = "0.0.0.0";
String mqttMasterTopic = "Home/PixelDisplay/";
int mqttPort = 1883;
int mqttRetryCounter = 0;
int mqttMaxRetrys = 3;


//// LDR Config
#define LDR_RESISTOR 10000 //ohms
#define LDR_PIN A0
#define LDR_PHOTOCELL LightDependentResistor::GL5516

//// Matrix Config
#define MATRIX_PIN D2

#define NUMMATRIX (32 * 8)
CRGB leds[NUMMATRIX];

#define COMPILE_HOUR          (((__TIME__[0]-'0')*10) + (__TIME__[1]-'0'))
#define COMPILE_MINUTE        (((__TIME__[3]-'0')*10) + (__TIME__[4]-'0'))
#define COMPILE_SECOND        (((__TIME__[6]-'0')*10) + (__TIME__[7]-'0'))
#define COMPILE_YEAR          ((((__DATE__ [7]-'0')*10+(__DATE__[8]-'0'))*10+(__DATE__ [9]-'0'))*10+(__DATE__ [10]-'0'))
#define COMPILE_SHORTYEAR     (((__DATE__ [9]-'0'))*10+(__DATE__ [10]-'0'))
#define COMPILE_MONTH         ((  __DATE__ [2] == 'n' ? (__DATE__ [1] == 'a' ? 0 : 5)   \
								: __DATE__ [2] == 'b' ? 1                               \
								: __DATE__ [2] == 'r' ? (__DATE__ [0] == 'M' ?  2 : 3)  \
								: __DATE__ [2] == 'y' ? 4                               \
								: __DATE__ [2] == 'l' ? 6                               \
								: __DATE__ [2] == 'g' ? 7                               \
								: __DATE__ [2] == 'p' ? 8                               \
								: __DATE__ [2] == 't' ? 9                               \
								: __DATE__ [2] == 'v' ? 10 : 11) +1)
#define COMPILE_DAY           ((__DATE__ [4]==' ' ? 0 : __DATE__  [4]-'0')*10+(__DATE__[5]-'0'))




const String version = String(COMPILE_YEAR) + IntFormat(COMPILE_MONTH) + IntFormat(COMPILE_DAY) + IntFormat(COMPILE_HOUR) + IntFormat(COMPILE_MINUTE);


FastLED_NeoMatrix* matrix;
WiFiClient espClient;
WiFiUDP udp;
PubSubClient client(espClient);
WiFiManager wifiManager;
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
LightDependentResistor photocell(LDR_PIN, LDR_RESISTOR, LDR_PHOTOCELL);
DHTesp dht;
SoftwareSerial softSerial(D7, D8); // RX | TX

// Matrix Vars
int matrixtBrightness = 127;
int matrixType = 1;


// System Vars
bool sleepMode = false;
bool bootScreenAktiv = true;
bool shouldSaveConfig = false;
String _updaterError;

// Bmp Vars
uint16_t bmpArray[64];

// Timerserver Vars
IPAddress timeServerIP;
String ntpServer = "de.pool.ntp.org";
int ntpRetryCounter = 0;
int ntpMaxRetrys = 3;

// Clock  Vars
bool clockBlink = false;
bool clockAktiv = true;
bool clockSwitchAktiv = true;
bool clockWithSeconds = true;
bool dateWithYear = true;
int clockSwitchSec = 7;
int clockCounterClock = 0;
int clockCounterDate = 0;
int clockTimeZone = 1;
time_t clockLastUpdate;
uint16_t  clockColorR = 255, clockColorG = 255, clockColorB = 255;

// Scrolltext Vars
bool scrollTextAktivLoop = false;
uint scrollTextPrevMillis = 0;
int scrollTextDefaultDelay = 100;
uint scrollTextDelay;
int scrollPos;
int scrollposY;
bool scrollwithBMP;
int scrollxPixelText;
String scrollTextString;

// Animate BMP Vars
uint16_t animationBmpList[10][64];
bool animateBMPAktivLoop = false;
uint animateBMPPrevMillis = 0;
int animateBMPCounter = 0;
bool animateBMPReverse = false;
bool animateBMPRubberbandingAktiv = false;
uint animateBMPDelay;
int animateBMPLimitLoops = -1;
int animateBMPLoopCount = 0;
int animateBMPLimitFrames = -1;
int animateBMPFrameCount = 0;

// Sensors Vars 
uint sendLuxPrevMillis = 0;
uint sendDHTPrevMillis = 0;
uint sendInfoPrevMillis = 0;
String OldGetMatrixInfo;
String OldGetLuxSensor;
String OldGetDHTSensor;

// Websoket Vars
String websocketConnection[10];


/**
 * Log Function
 * */
void Log(String function, String message)
{

	String timeStamp = IntFormat(year()) + "-" + IntFormat(month()) + "-" + IntFormat(day()) + "T" + IntFormat(hour()) + ":" + IntFormat(minute()) + ":" + IntFormat(second());

	Serial.println("[" + timeStamp + "] " + function + ": " + message);

	// Prüfen ob über Websocket versendet werden muss
	if (webSocket.connectedClients() > 0)
	{
		for (int i = 0; i < (int)sizeof websocketConnection / (int)sizeof websocketConnection[0]; i++)
		{
			if (websocketConnection[i] == "/dash")
			{
				webSocket.sendTXT(i, "{\"log\":{\"timeStamp\":\"" + timeStamp + "\",\"function\":\"" + function + "\",\"message\":\"" + message + "\"}}");
			}
		}
	}
}

void SaveConfigCallback()
{
	shouldSaveConfig = true;
}

void SaveConfig()
{
	//save the custom parameters to FS
	if (shouldSaveConfig) {
		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();

		json["matrixtBrightness"] = matrixtBrightness;
		json["matrixType"] = matrixType;
		json["ntpServer"] = ntpServer;
		json["clockTimeZone"] = clockTimeZone;
		json["scrollTextDefaultDelay"] = scrollTextDefaultDelay;
		json["bootScreenAktiv"] = bootScreenAktiv;
		json["mqttAktiv"] = mqttAktiv;
		json["mqttUser"] = mqttUser;
		json["mqttPassword"] = mqttPassword;
		json["mqttServer"] = mqttServer;
		json["mqttMasterTopic"] = mqttMasterTopic;
		json["mqttPort"] = mqttPort;

		File configFile = LittleFS.open("/config.json", "w");
		
		json.printTo(configFile);
		configFile.close();
		Log("SaveConfig", "Saved");
		//end save
	}
}

void LoadConfig()
{
	if (LittleFS.exists("/config.json"))
	{
		//file exists, reading and loading
		File configFile = LittleFS.open("/config.json", "r");
		if (configFile)
		{
			Serial.println("opened config file");

			DynamicJsonBuffer jsonBuffer;
			JsonObject& json = jsonBuffer.parseObject(configFile);

			if (json.success())
			{
				if (json.containsKey("matrixtBrightness"))
				{
					matrixtBrightness = json["matrixtBrightness"];
				}

				if (json.containsKey("matrixType"))
				{
					matrixType = json["matrixType"];
				}

				if (json.containsKey("ntpServer"))
				{
					ntpServer = json["ntpServer"].as<char*>();
				}

				if (json.containsKey("clockTimeZone"))
				{
					clockTimeZone = json["clockTimeZone"];
				}

				if (json.containsKey("scrollTextDefaultDelay"))
				{
					scrollTextDefaultDelay = json["scrollTextDefaultDelay"];
				}

				if (json.containsKey("bootScreenAktiv"))
				{
					bootScreenAktiv = json["bootScreenAktiv"];
				}

				if (json.containsKey("mqttAktiv"))
				{
					mqttAktiv = json["mqttAktiv"];
				}

				if (json.containsKey("mqttUser"))
				{
					mqttUser = json["mqttUser"].as<char*>();
				}

				if (json.containsKey("mqttPassword"))
				{
					mqttPassword = json["mqttPassword"].as<char*>();
				}

				if (json.containsKey("mqttServer"))
				{
					mqttServer = json["mqttServer"].as<char*>();
				}

				if (json.containsKey("mqttMasterTopic"))
				{
					mqttMasterTopic = json["mqttMasterTopic"].as<char*>();
				}

				if (json.containsKey("mqttPort"))
				{
					mqttPort = json["mqttPort"];
				}

				Log("LoadConfig", "Loaded");
			}

		}
	}
	else
	{
		Log("LoadConfig", "No Configfile, init new file");
		SaveConfigCallback();
		SaveConfig();
	}
}

void SetConfig(JsonObject& json)
{
	if (json.containsKey("matrixtBrightness"))
	{
		matrixtBrightness = json["matrixtBrightness"];
		matrix->setBrightness(matrixtBrightness);
	}

	if (json.containsKey("matrixType"))
	{
		matrixType = json["matrixType"];
	}

	if (json.containsKey("ntpServer"))
	{
		ntpServer = json["ntpServer"].as<char*>();
	}

	if (json.containsKey("clockTimeZone"))
	{
		clockTimeZone = json["clockTimeZone"];
	}

	if (json.containsKey("scrollTextDefaultDelay"))
	{
		scrollTextDefaultDelay = json["scrollTextDefaultDelay"];
	}

	if (json.containsKey("bootScreenAktiv"))
	{
		bootScreenAktiv = json["bootScreenAktiv"];
	}

	if (json.containsKey("mqttAktiv"))
	{
		mqttAktiv = json["mqttAktiv"];
	}

	if (json.containsKey("mqttUser"))
	{
		mqttUser = json["mqttUser"].as<char*>();
	}

	if (json.containsKey("mqttPassword"))
	{
		mqttPassword = json["mqttPassword"].as<char*>();
	}

	if (json.containsKey("mqttServer"))
	{
		mqttServer = json["mqttServer"].as<char*>();
	}

	if (json.containsKey("mqttMasterTopic"))
	{
		mqttMasterTopic = json["mqttMasterTopic"].as<char*>();
	}

	if (json.containsKey("mqttPort"))
	{
		mqttPort = json["mqttPort"];
	}

	SaveConfigCallback();
	SaveConfig();
}

void ScrollText(bool isFadeInRequired)
{
	int tempxPixelText = 0;

	if (scrollxPixelText < 32 && !scrollwithBMP)
	{
		scrollxPixelText = 32;
	}
	else if (scrollxPixelText < 24 && scrollwithBMP)
	{
		scrollxPixelText = 24;
	}
	else if (scrollwithBMP)
	{
		tempxPixelText = 8;
	}


	if (scrollPos > ((scrollxPixelText - tempxPixelText) * -1))
	{
		matrix->clear();
		matrix->setCursor(--scrollPos, scrollposY);
		matrix->print(scrollTextString);

		if (scrollwithBMP)
		{
			matrix->drawRGBBitmap(0, 0, bmpArray, 8, 8);
		}

		if (isFadeInRequired)
		{
			FadeIn();
		}
		else
		{
			matrix->show();
		}
	}
	else
	{
		// + 8 Pixel sonst scrollt er mitten drinn los!
		scrollPos = 33;
	}
}

void DrawTextHelper(String text, bool bigFont, bool centerText, bool scrollText, bool autoScrollText, bool withBMP, bool fadeInRequired, uint16_t bmpArray[64], int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
	//int charSize = 0;
	//int16_t x1, y1;
	//uint16_t h
	uint16_t xPixelText, xPixel;

	text = Utf8ToAscii(text);

	if (withBMP)
	{
		// Verfügbare Text Pixelanzahl in der Breite (X) mit Bild 
		xPixel = 24;
		posX = 8;
	}
	else
	{
		// Verfügbare Text Pixelanzahl in der Breite (X) ohne Bild 
		xPixel = 32;
	}

	if (bigFont)
	{
		// Grosse Schrift setzten
		matrix->setFont();
		xPixelText = text.length() * 6;
		// Positions Korrektur
		posY = posY - 1;
	}
	else
	{
		// Kleine Schrift setzten
		matrix->setFont(&PixelItFont);
		xPixelText = text.length() * 4;
		// Positions Korrektur
		posY = posY + 5;
	}

	//matrix->getTextBounds(text, 0, 0, &x1, &y1, &xPixelText, &h);

	if (centerText)
	{
		// Kein Offset benötigt.
		int offset = 0;

		// Mit BMP berechnen
		if (withBMP)
		{
			// Offset um nicht das BMP zu überschreiben
			// + 1 weil wir der erste pixel 0 ist!!!!
			offset = 8 + 1;
		}

		// Berechnung für das erste Pixel des Textes
		posX = ((xPixel - xPixelText) / 2) + offset;
	}

	matrix->setCursor(posX, posY);

	matrix->setTextColor(matrix->Color(colorRed, colorGreen, colorBlue));

	if (scrollText || (autoScrollText && xPixelText > xPixel))
	{

		matrix->setBrightness(matrixtBrightness);


		scrollTextString = text;
		scrollposY = posY;
		scrollwithBMP = withBMP;
		scrollxPixelText = xPixelText;
		// + 8 Pixel sonst scrollt er mitten drinn los!
		scrollPos = 33;


		scrollTextAktivLoop = true;
		scrollTextPrevMillis = millis();
		ScrollText(fadeInRequired);

	}
	// Fall doch der Text auf denm Display passt!
	else if (autoScrollText)
	{
		matrix->print(text);
		// Hier muss geprüfter weden ob ausgefadet wurde,
		// dann ist nämlich die Brightness zu weit unten (0),
		// aber auch nur wenn nicht animateBMPAktivLoop aktiv ist,
		// denn sonst hat er das schon erledigt.
		if (fadeInRequired && !animateBMPAktivLoop)
		{
			FadeIn();
		}
		else
		{
			matrix->show();
		}
	}
	else
	{
		matrix->print(text);
	}
}

void DrawText(String text, bool bigFont, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
	DrawTextHelper(text, bigFont, false, false, false, false, false, NULL, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawTextCenter(String text, bool bigFont, bool withBMP, int colorRed, int colorGreen, int colorBlue)
{
	DrawTextHelper(text, bigFont, true, false, false, withBMP, false, NULL, colorRed, colorGreen, colorBlue, 0, 1);
}

void DrawTextScrolled(String text, bool bigFont, bool withBMP, bool fadeInRequired, uint16_t bmpArray[64], int colorRed, int colorGreen, int colorBlue)
{
	DrawTextHelper(text, bigFont, false, true, false, withBMP, fadeInRequired, bmpArray, colorRed, colorGreen, colorBlue, 0, 1);
}

void DrawAutoTextScrolled(String text, bool bigFont, bool withBMP, bool fadeInRequired, uint16_t bmpArray[64], int colorRed, int colorGreen, int colorBlue)
{
	DrawTextHelper(text, bigFont, false, false, true, withBMP, fadeInRequired, bmpArray, colorRed, colorGreen, colorBlue, 0, 1);
}

void ClearBMPArea()
{
	int16_t h = 8;
	int16_t w = 8;
	int16_t x = 0;
	int16_t y = 0;

	for (int16_t j = 0; j < h; j++, y++)
	{
		for (int16_t i = 0; i < w; i++)
		{
			matrix->drawPixel(x + i, y, (uint16_t)0);
		}
	}
}

int DayOfWeekFirstMonday(int OrigDayofWeek)
{
	int idx = 7 + OrigDayofWeek - 1;
	if (idx > 6) // week ends at 6, because Enum.DayOfWeek is zero-based
	{
		idx -= 7;
	}
	return idx;
}

void DrawWeekDay()
{
	for (int i = 0; i <= 6; i++) {
		if (i == DayOfWeekFirstMonday(dayOfWeek(now()) - 1)) {
			matrix->drawLine(2 + i * 4, 7, i * 4 + 4, 7, matrix->Color(clockColorR, clockColorG, clockColorB));
		}
		else {
			matrix->drawLine(2 + i * 4, 7, i * 4 + 4, 7, 21162);
		}
	}
}

uint ColorWheel(byte wheelPos, int pos)
{
	if (wheelPos < 85) {
		return matrix->Color((wheelPos * 3) - pos, (255 - wheelPos * 3) - pos, 0);
	}
	else if (wheelPos < 170) {
		wheelPos -= 85;
		return matrix->Color((255 - wheelPos * 3) - pos, 0, (wheelPos * 3) - pos);
	}
	else {
		wheelPos -= 170;
		return matrix->Color(0, (wheelPos * 3) - pos, (255 - wheelPos * 3) - pos);
	}
}

void DrawClock(bool fromJSON)
{
	matrix->clear();

	char date[14];
	char time[14];
	int xPosTime = 2;
	int xPosDate = 2;

	//Prüfen ob das Datum mit Jahr angezeigt werden soll
	if(dateWithYear){
		xPosDate = 2;
		sprintf_P(date, PSTR("%02d.%02d.%02d"), day(), month(), year()-2000);
	}else{
		xPosDate = 7;
		sprintf_P(date, PSTR("%02d.%02d."), day(), month());
	}

	//Prüfen ob die Uhrzeit mit Sekunden angezeigt werden soll
	if (clockWithSeconds)
	{
		xPosTime = 2;
		
		if (clockBlink)
		{
			clockBlink = false;
			sprintf_P(time, PSTR("%02d:%02d:%02d"), hour(), minute(), second());
		}
		else
		{
			clockBlink = !clockBlink;
			sprintf_P(time, PSTR("%02d %02d %02d"), hour(), minute(), second());
		}
	}
	else
	{
		xPosTime = 7;

		if (clockBlink)
		{
			clockBlink = false;
			sprintf_P(time, PSTR("%02d:%02d"), hour(), minute());
		}
		else
		{
			clockBlink = !clockBlink;
			sprintf_P(time, PSTR("%02d %02d"), hour(), minute());
		}
	}

	if (!clockSwitchAktiv || (clockSwitchAktiv && clockCounterClock <= clockSwitchSec))
	{
		if (clockSwitchAktiv)
		{
			clockCounterClock++;
		}

		if (clockCounterClock > clockSwitchSec)
		{
			clockCounterDate = 0;

			int counter = 0;
			while (counter <= 6)
			{
				counter++;
				matrix->clear();
				DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, (1 + counter));
				DrawText(String(date), false, clockColorR, clockColorG, clockColorB, xPosDate, (-6 + counter));
				matrix->drawLine(0, 7, 33, 7, 0);
				DrawWeekDay();
				matrix->show();
				delay(50);
			}
		}
		else
		{
			DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, 1);
		}
	}
	else
	{
		clockCounterDate++;

		if (clockCounterDate == clockSwitchSec)
		{
			clockCounterClock = 0;

			int counter = 0;
			while (counter <= 6)
			{
				counter++;
				matrix->clear();
				DrawText(String(date), false, clockColorR, clockColorG, clockColorB, xPosDate, (1 + counter));
				DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, (-6 + counter));
				matrix->drawLine(0, 7, 33, 7, 0);
				DrawWeekDay();
				matrix->show();
				delay(50);
			}
		}
		else
		{
			DrawText(String(date), false, clockColorR, clockColorG, clockColorB, xPosDate, 1);
		}
	}

	DrawWeekDay();


	// Wenn der Aufruf nicht über JSON sondern über den Loop kommt
	// muss ich mich selbst ums Show kümmern.
	if (!fromJSON)
	{
		matrix->show();
	}
}

void AnimateBMP(bool isShowRequired)
{
	// Prüfen auf 2, 2 ist mein Platzhalter für leeres obj!
	if (animationBmpList[animateBMPCounter][0] == 2)
	{
		// Ist ein Repeat Limit übergeben worden.
		if (animateBMPLimitLoops > 0 && !animateBMPRubberbandingAktiv)
		{
			animateBMPLoopCount++;

			// Ist der Repeat Limit erreicht den AnimateBMP Loop deaktiveren.
			if (animateBMPLoopCount == animateBMPLimitLoops)
			{
				animateBMPAktivLoop = false;
				return;
			}
		}

		// Prüfen ob Rubberbanding aktiv und mehr wie 1 Frame vorhanden ist. 
		if (animateBMPRubberbandingAktiv && animateBMPCounter > 1)
		{
			animateBMPReverse = true;

			// 2 abziehen da sonst der letzte Frame doppelt angezeigt wird.
			if (animateBMPCounter > 0)
			{
				animateBMPCounter = animateBMPCounter - 2;
			}
		}
		else
		{
			animateBMPCounter = 0;
		}
	}

	if (animateBMPCounter < 0)
	{
		// Auf 1 setzten da sons der erste Frame doppelt angezeigt wird.
		animateBMPCounter = 1;
		animateBMPReverse = false;

		// Ist ein Repeat Limit übergeben worden.
		if (animateBMPLimitLoops > 0)
		{
			animateBMPLoopCount++;
			// Ist der Repeat Limit erreicht den AnimateBMP Loop deaktiveren.
			if (animateBMPLoopCount >= animateBMPLimitLoops)
			{
				animateBMPAktivLoop = false;
				return;
			}
		}
	}

	ClearBMPArea();

	matrix->drawRGBBitmap(0, 0, animationBmpList[animateBMPCounter], 8, 8);

	for (int y = 0; y < 64; y++)
	{
		bmpArray[y] = animationBmpList[animateBMPCounter][y];
	}

	// Soll der Loop wieder zurücklaufen?
	if (animateBMPReverse)
	{
		animateBMPCounter--;
	}
	else
	{
		animateBMPCounter++;
	}


	if (isShowRequired)
	{
		matrix->show();
	}

}

void WifiSetup()
{
	wifiManager.resetSettings();
	ESP.restart();
	delay(300);
}

void HandleGetMainPage()
{
	server.sendHeader("Connection", "close");
	server.send(200, "text/html", PAGE_INDEX);
}

void HandleGetDashPage()
{
	if(server.hasHeader("Caller")){
		if(server.header("Caller") == "MyPixelDashboard"){
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", PAGE_DASH);
		}else{
			server.sendHeader("Location", "/",true);
    		server.send(302, "text/plane",""); 
		}
	}else{
		server.sendHeader("Location", "/",true);
		server.send(302, "text/plane",""); 
	}
}

void HandleGetConfigPage()
{
	if(server.hasHeader("Caller")){
		if(server.header("Caller") == "MyPixelDashboard"){
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", PAGE_CONFIG);
		}else{
			server.sendHeader("Location", "/",true);
    		server.send(302, "text/plane",""); 
		}
	}else{
		server.sendHeader("Location", "/",true);
		server.send(302, "text/plane",""); 
	}
}

void HandleGetTestAreaPage()
{
	if(server.hasHeader("Caller")){
		if(server.header("Caller") == "MyPixelDashboard"){
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", PAGE_TEST);
		}else{
			server.sendHeader("Location", "/",true);
    		server.send(302, "text/plane",""); 
		}
	}else{
		server.sendHeader("Location", "/",true);
		server.send(302, "text/plane",""); 
	}
}

void HandleGetUpdatePage()
{
	if(server.hasHeader("Caller")){
		if(server.header("Caller") == "MyPixelDashboard"){
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", PAGE_UPDATE);
		}else{
			server.sendHeader("Location", "/",true);
    		server.send(302, "text/plane",""); 
		}
	}else{
		server.sendHeader("Location", "/",true);
		server.send(302, "text/plane",""); 
	}
}

void ColoredBarWipe()
{
	for (uint16_t i = 0; i < 32 + 1; i++) {
		matrix->fillRect(0, 0, i - 1, 8, matrix->Color(0, 0, 0));

		matrix->drawFastVLine(i, 0, 8, ColorWheel((i * 8) & 255, 0));
		matrix->drawFastVLine(i + 1, 0, 8, ColorWheel((i * 9) & 255, 0));
		matrix->drawFastVLine(i - 1, 0, 8, 0);
		matrix->drawFastVLine(i - 2, 0, 8, 0);
		matrix->show();
		delay(15);
	}
}
void ColoredBarWipeVertical()
{
	for (uint16_t i = 0; i < 8 + 1; i++) {
		matrix->drawFastHLine(0, i, 32, ColorWheel((i * 16) & 255, 0));
		matrix->drawFastHLine(0, i + 1, 32, ColorWheel((i * 32) & 255, 0));
		matrix->drawFastHLine(0, i - 1, 32, 0);
		matrix->drawFastHLine(0, i - 2, 32, 0);
		matrix->show();
		delay(50);
	}
}

String GetConfig()
{
	File configFile = LittleFS.open("/config.json", "r");
	if (configFile)
	{
		Log(F("GetConfig"), F("Opened config file"));
		size_t size = configFile.size();
		// Allocate a buffer to store contents of the file.
		std::unique_ptr<char[]> buf(new char[size]);

		configFile.readBytes(buf.get(), size);
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.parseObject(buf.get());

		String json;
		root.printTo(json);

		return json;
	}else{
		return "";
	}
}

String GetDHTSensor()
{
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();

	float humidity = roundf(dht.getHumidity());
	float temperature = dht.getTemperature();

	if (isnan(humidity) || isnan(temperature))
	{
		root["humidity"] = "Nicht installiert";
		root["temperature"] = "Nicht installiert";
	}
	else
	{
		root["humidity"] = humidity;
		root["temperature"] = temperature;
	}
	String json;
	root.printTo(json);

	return json;
}

String GetLuxSensor()
{
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();

	float lux = roundf(photocell.getCurrentLux() * 1000) / 1000;
	if (isnan(lux)){
		root["lux"] = "Nicht installiert";
	}else{
		root["lux"] = roundf(photocell.getCurrentLux() * 1000) / 1000;
	}
	
	String json;
	root.printTo(json);

	return json;
}

String GetMatrixInfo()
{
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();

	root["firmwareVersion"] = version;
	root["sketchSize"] = ESP.getSketchSize();
	root["freeSketchSpace"] = ESP.getFreeSketchSpace();
	root["wifiRSSI"] = String(WiFi.RSSI());
	root["wifiQuality"] = GetRSSIasQuality(WiFi.RSSI());
	root["wifiSSID"] = WiFi.SSID();
	root["ipAddress"] = WiFi.localIP().toString();
	root["freeHeap"] = ESP.getFreeHeap();
	root["chipID"] = ESP.getChipId();
	root["cpuFreqMHz"] = ESP.getCpuFreqMHz();
	root["sleepMode"] = sleepMode;

	String json;
	root.printTo(json);

	return json;
}

void SendMatrixInfo(bool force)
{
	if (force)
	{
		OldGetMatrixInfo = "";
	}

	String matrixInfo;

	// Prüfen ob die ermittlung der MatrixInfo überhaupt erforderlich ist
	if ((mqttAktiv == true && mqttRetryCounter < mqttMaxRetrys) || (webSocket.connectedClients() > 0))
	{
		matrixInfo = GetMatrixInfo();
	}
	// Prüfen ob über MQTT versendet werden muss
	if (mqttAktiv == true && mqttRetryCounter < mqttMaxRetrys && OldGetMatrixInfo != matrixInfo)
	{
		client.publish((mqttMasterTopic + "matrixinfo").c_str(), matrixInfo.c_str(), true);
	}
	// Prüfen ob über Websocket versendet werden muss
	if (webSocket.connectedClients() > 0 && OldGetMatrixInfo != matrixInfo)
	{
		for (int i = 0; i < (int)sizeof websocketConnection / (int)sizeof websocketConnection[0]; i++)
		{
			if (websocketConnection[i] == "/dash" || websocketConnection[i] == "/matrixinfo")
			{
				webSocket.sendTXT(i, matrixInfo);
			}
		}
	}

	OldGetMatrixInfo = matrixInfo;
}

void SendLDR(bool force)
{
	if (force)
	{
		OldGetLuxSensor = "";
	}

	String luxSensor;

	// Prüfen ob die Abfrage des LuxSensor überhaupt erforderlich ist
	if ((mqttAktiv == true && mqttRetryCounter < mqttMaxRetrys) || (webSocket.connectedClients() > 0))
	{
		luxSensor = GetLuxSensor();
	}
	// Prüfen ob über MQTT versendet werden muss
	if (mqttAktiv == true && mqttRetryCounter < mqttMaxRetrys && OldGetLuxSensor != luxSensor)
	{
		client.publish((mqttMasterTopic + "luxsensor").c_str(), luxSensor.c_str(), true);
	}
	// Prüfen ob über Websocket versendet werden muss
	if (webSocket.connectedClients() > 0 && OldGetLuxSensor != luxSensor)
	{
		for (int i = 0; i < (int)sizeof websocketConnection / (int)sizeof websocketConnection[0]; i++)
		{
			if (websocketConnection[i] == "/dash" || websocketConnection[i] == "/lux")
			{
				webSocket.sendTXT(i, luxSensor);
			}
		}
	}

	OldGetLuxSensor = luxSensor;
}

void SendDHT(bool force)
{
	if (force)
	{
		OldGetDHTSensor = "";
	}

	String dhtSensor;

	// Prüfen ob die Abfrage des LuxSensor überhaupt erforderlich ist
	if ((mqttAktiv == true && mqttRetryCounter < mqttMaxRetrys) || (webSocket.connectedClients() > 0))
	{
		dhtSensor = GetDHTSensor();
	}
	// Prüfen ob über MQTT versendet werden muss
	if (mqttAktiv == true && mqttRetryCounter < mqttMaxRetrys && OldGetDHTSensor != dhtSensor)
	{
		client.publish((mqttMasterTopic + "dhtsensor").c_str(), dhtSensor.c_str(), true);
	}
	// Prüfen ob über Websocket versendet werden muss
	if (webSocket.connectedClients() > 0 && OldGetDHTSensor != dhtSensor)
	{
		for (int i = 0; i < (int)sizeof websocketConnection / (int)sizeof websocketConnection[0]; i++)
		{
			if (websocketConnection[i] == "/dash" || websocketConnection[i] == "/dht")
			{
				webSocket.sendTXT(i, dhtSensor);
			}
		}
	}

	OldGetDHTSensor = dhtSensor;
}

void SendConfig()
{
	if (webSocket.connectedClients() > 0)
	{
		for (int i = 0; i < (int)sizeof websocketConnection / (int)sizeof websocketConnection[0]; i++)
		{
			if (websocketConnection[i] == "/config")
			{
				String config = GetConfig();
				webSocket.sendTXT(i, config);
			}
		}
	}

}

void CreateFrames(JsonObject& json)
{
	String logMessage = F("Json contains ");

	// Ist eine Display Helligkeit übergeben worden?
	if (json.containsKey("brightness"))
	{
		logMessage += F("Brightness Control, ");
		matrixtBrightness = json["brightness"];
	}

	// SleepMode
	if (json.containsKey("sleepMode"))
	{
		logMessage += F("SleepMode Control, ");
		sleepMode = json["sleepMode"];
	}
	// SleepMode
	if (sleepMode)
	{
		matrix->setBrightness(0);
		matrix->show();
	}
	else
	{
		matrix->setBrightness(matrixtBrightness);

		// Prüfung für die Unterbrechnung der lokalen Schleifen
		if (json.containsKey("bitmap") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars") || json.containsKey("bitmapAnimation"))
		{
			clockAktiv = false;
			scrollTextAktivLoop = false;
			animateBMPAktivLoop = false;
		}

		// Ist eine Switch Animation übergeben worden?
		bool fadeAnimationAktiv = false;
		bool coloredBarWipeAnimationAktiv = false;
		if (json.containsKey("switchAnimation"))
		{
			logMessage += F("SwitchAnimation, ");
			// Switch Animation aktiv?
			if (json["switchAnimation"]["aktiv"])
			{
				// Fade Animation aktiv?
				if (json["switchAnimation"]["animation"] == "fade")
				{
					fadeAnimationAktiv = true;
				}
				else if (json["switchAnimation"]["animation"] == "coloredBarWipe")
				{
					coloredBarWipeAnimationAktiv = true;
				}
			}
		}

		// Fade aktiv?
		if (fadeAnimationAktiv)
		{
			FadeOut();
		}
		else if (coloredBarWipeAnimationAktiv)
		{
			ColoredBarWipe();
		}

		// Clock
		if (json.containsKey("clock"))
		{
			logMessage += F("InternalClock Control, ");
			if (json["clock"]["show"])
			{
				scrollTextAktivLoop = false;
				animateBMPAktivLoop = false;
				clockAktiv = true;

				clockCounterClock = 0;
				clockCounterDate = 0;

				clockSwitchAktiv = json["clock"]["switchAktiv"];

				if (clockSwitchAktiv == true)
				{
					clockSwitchSec = json["clock"]["switchSec"];
				}

				clockWithSeconds = json["clock"]["withSeconds"];

				if (json["clock"]["color"]["r"].as<char*>() != NULL)
				{
					clockColorR = json["clock"]["color"]["r"];
					clockColorG = json["clock"]["color"]["g"];
					clockColorB = json["clock"]["color"]["b"];
				}
				else
				{
					clockColorR = 255;
					clockColorG = 255;
					clockColorB = 255;
				}

				DrawClock(true);
			}
		}

		if (json.containsKey("bitmap") || json.containsKey("bitmapAnimation") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars"))
		{
			// Alle Pixel löschen
			matrix->clear();
		}

		// Bar
		if (json.containsKey("bar"))
		{
			logMessage += F("Bar, ");
			matrix->drawLine(json["bar"]["position"]["x"], json["bar"]["position"]["y"], json["bar"]["position"]["x2"], json["bar"]["position"]["y2"], matrix->Color(json["bar"]["color"]["r"], json["bar"]["color"]["g"], json["bar"]["color"]["b"]));
		}

		// Bars
		if (json.containsKey("bars"))
		{
			logMessage += F("Bars, ");
			for (JsonVariant x : json["bars"].as<JsonArray>())
			{
				matrix->drawLine(x["position"]["x"], x["position"]["y"], x["position"]["x2"], x["position"]["y2"], matrix->Color(x["color"]["r"], x["color"]["g"], x["color"]["b"]));
			}
		}

		// Ist ein Bitmap übergeben worden?
		if (json.containsKey("bitmap"))
		{
			logMessage += F("Bitmap, ");

			int16_t h = json["bitmap"]["size"]["height"].as<int16_t>();
			int16_t w = json["bitmap"]["size"]["width"].as<int16_t>();
			int16_t x = json["bitmap"]["position"]["x"].as<int16_t>();
			int16_t y = json["bitmap"]["position"]["y"].as<int16_t>();

			// Hier kann leider nicht die Funktion matrix->drawRGBBitmap() genutzt werde da diese Fehler in der Anzeige macht wenn es mehr wie 8x8 Pixel werden. 
			for (int16_t j = 0; j < h; j++, y++)
			{
				for (int16_t i = 0; i < w; i++)
				{
					matrix->drawPixel(x + i, y, json["bitmap"]["data"][j * w + i].as<uint16_t>());
				}
			}

			// JsonArray in IntArray konvertieren
			// dies ist nötik für diverse kleine Logiken z.B. Scrolltext
			json["bitmap"]["data"].as<JsonArray>().copyTo(bmpArray);
		}

		// Ist eine BitmapAnimation übergeben worden?
		if (json.containsKey("bitmapAnimation"))
		{
			logMessage += F("BitmapAnimation, ");
			// animationBmpList zurücksetzten um das ende nacher zu finden -1 (habe aktuell keine bessere Lösung)
			for (int i = 0; i < 10; i++)
			{
				animationBmpList[i][0] = 2;
			}

			int counter = 0;
			for (JsonVariant x : json["bitmapAnimation"]["data"].as<JsonArray>())
			{
				// JsonArray in IntArray konvertieren
				x.as<JsonArray>().copyTo(bmpArray);
				// Speichern für die Ausgabe
				for (int i = 0; i < 64; i++)
				{
					animationBmpList[counter][i] = bmpArray[i];
				}
				counter++;
			}

			animateBMPDelay = json["bitmapAnimation"]["animationDelay"];
			animateBMPRubberbandingAktiv = json["bitmapAnimation"]["rubberbanding"];

			animateBMPLimitLoops = 0;
			if (json["bitmapAnimation"]["limitLoops"])
			{
				animateBMPLimitLoops = json["bitmapAnimation"]["limitLoops"].as<int>();
			}

			// Hier einmal den Counter zurücksetzten
			animateBMPCounter = 0;
			animateBMPLoopCount = 0;
			animateBMPAktivLoop = true;
			animateBMPReverse = false;
			animateBMPPrevMillis = millis();
			AnimateBMP(false);

		}

		// Ist ein Text übergeben worden?
		bool scrollTextAktiv = false;
		if (json.containsKey("text"))
		{
			logMessage += F("Text");
			// Immer erstmal den Default Delay annehmen.
			scrollTextDelay = scrollTextDefaultDelay;

			// Ist ScrollText auto oder true gewählt?
			scrollTextAktiv = (json["text"]["scrollText"] == "auto" || ((json["text"]["scrollText"]).is<bool>() && json["text"]["scrollText"]));

			if (json["text"]["centerText"])
			{
				DrawTextCenter(json["text"]["textString"], json["text"]["bigFont"], json.containsKey("bitmap") || json.containsKey("bitmapAnimation"), json["text"]["color"]["r"], json["text"]["color"]["g"], json["text"]["color"]["b"]);
			}
			// Ist ScrollText auto oder true gewählt?
			else if (scrollTextAktiv)
			{

				bool withBMP = (json.containsKey("bitmap") || json.containsKey("bitmapAnimation"));

				bool fadeInRequired = ((json.containsKey("bars") || json.containsKey("bar") || json.containsKey("bitmap") || json.containsKey("bitmapAnimation")) && fadeAnimationAktiv);

				// Wurde ein Benutzerdefeniertes Delay übergeben? 
				if (json["text"]["scrollTextDelay"])
				{
					scrollTextDelay = json["text"]["scrollTextDelay"];
				}

				if (!(json["text"]["scrollText"]).is<bool>() && json["text"]["scrollText"] == "auto")
				{
					DrawAutoTextScrolled(json["text"]["textString"], json["text"]["bigFont"], withBMP, fadeInRequired, bmpArray, json["text"]["color"]["r"], json["text"]["color"]["g"], json["text"]["color"]["b"]);
				}
				else
				{
					DrawTextScrolled(json["text"]["textString"], json["text"]["bigFont"], withBMP, fadeInRequired, bmpArray, json["text"]["color"]["r"], json["text"]["color"]["g"], json["text"]["color"]["b"]);
				}
			}
			else
			{
				DrawText(json["text"]["textString"], json["text"]["bigFont"], json["text"]["color"]["r"], json["text"]["color"]["g"], json["text"]["color"]["b"], json["text"]["position"]["x"], json["text"]["position"]["y"]);
			}

		}

		// Fade aktiv?
		if (!scrollTextAktiv && (fadeAnimationAktiv || coloredBarWipeAnimationAktiv))
		{
			FadeIn();
		}
		else
		{
			// Fade nicht aktiv! 
			// Muss mich selbst um Show kümmern
			matrix->show();
		}
	}

	Log(F("CreateFrames"), logMessage);
}

void callback(char* topic, byte* payload, unsigned int length)
{
	if (payload[0] == '{')
	{
		payload[length] = '\0';
		String channel = String(topic);
		channel.replace(mqttMasterTopic, "");

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.parseObject(payload);

		Log("MQTT_callback", "Incomming Json length: " + String(json.measureLength()));

		if (channel.equals("setScreen"))
		{
			CreateFrames(json);
		}
		else if (channel.equals("getLuxsensor"))
		{
			client.publish((mqttMasterTopic + "luxsensor").c_str(), GetLuxSensor().c_str());
		}
		else if (channel.equals("getMatrixinfo"))
		{
			client.publish((mqttMasterTopic + "matrixinfo").c_str(), GetMatrixInfo().c_str());
		}
		else if (channel.equals("getConfig"))
		{
			client.publish((mqttMasterTopic + "config").c_str(), GetConfig().c_str());
		}
		else if (channel.equals("setConfig"))
		{
			SetConfig(json);
		}
	}
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {

	switch (type) {
		case WStype_DISCONNECTED:
		{
			Log("WebSocketEvent", "[" + String(num) + "] Disconnected!");
			websocketConnection[num] = "";
			break;
		}
		case WStype_CONNECTED:
		{
			// Merken für was die Connection hergstellt wurde
			websocketConnection[num] = String((char*)payload);

			// IP der Connection abfragen
			IPAddress ip = webSocket.remoteIP(num);

			// Logausgabe 
			Log("WebSocketEvent", "[" + String(num) + "] Connected from " + ip.toString() + " url: " + websocketConnection[num]);

			// send message to client
			SendMatrixInfo(true);
			SendLDR(true);
			SendDHT(true);
			SendConfig();
			break;
		}
		case WStype_TEXT:
		{
			if (((char*)payload)[0] == '{')
			{
				DynamicJsonBuffer jsonBuffer;
				JsonObject& json = jsonBuffer.parseObject(payload);

				// Logausgabe 
				Log("WebSocketEvent", "Incomming Json length: " + String(json.measureLength()));

				if (websocketConnection[num] == "/setScreen")
				{
					CreateFrames(json);
				}
				else if (websocketConnection[num] == "/setConfig")
				{
					SetConfig(json);
					delay(500);
					ESP.restart();
				}
			}
			break;
		}
		case WStype_BIN:
			//Serial.printf("[%u] get binary length: %u\n", num, length);
			//hexdump(payload, length);

			// send message to client
			// webSocket.sendBIN(num, payload, length);
			break;
		case WStype_FRAGMENT_TEXT_START:
			break;
		
		case WStype_FRAGMENT_BIN_START:
			break;
		
		case WStype_FRAGMENT:
			break;
		
		case WStype_FRAGMENT_FIN:
			break;
		
		case WStype_PING:
			break;
		
		case WStype_PONG:
			break;

		case WStype_ERROR:
			break;
	}
}

void MqttReconnect()
{
	// Loop until we're reconnected
	while (!client.connected() && mqttRetryCounter < mqttMaxRetrys)
	{
		bool connected = false;
		if (mqttUser != NULL && mqttUser.length() > 0 && mqttPassword != NULL && mqttPassword.length() > 0)
		{
			Log(F("MqttReconnect"), F("MQTT connect to server with User and Password"));
			connected = client.connect(("PixelIt_" + GetChipID()).c_str(), mqttUser.c_str(), mqttPassword.c_str(), "state", 0, true, "diconnected");
		}
		else
		{
			Log(F("MqttReconnect"), F("MQTT connect to server without User and Password"));
			connected = client.connect(("PixelIt_" + GetChipID()).c_str(), "state", 0, true, "diconnected");
		}

		// Attempt to connect
		if (connected)
		{
			Log(F("MqttReconnect"), F("MQTT connected!"));
			// ... and resubscribe
			client.subscribe((mqttMasterTopic + "setScreen").c_str());
			client.subscribe((mqttMasterTopic + "getLuxsensor").c_str());
			client.subscribe((mqttMasterTopic + "getMatrixinfo").c_str());
			client.subscribe((mqttMasterTopic + "getConfig").c_str());
			client.subscribe((mqttMasterTopic + "setConfig").c_str());
			// ... and publish
			client.publish((mqttMasterTopic + "state").c_str(), "connected");
		}
		else
		{
			Log(F("MqttReconnect"), F("MQTT not connected!"));
			Log(F("MqttReconnect"), F("Wait 5 seconds before retrying...."));
			mqttRetryCounter++;
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}

	if (mqttRetryCounter >= mqttMaxRetrys)
	{
		Log(F("MqttReconnect"), F("No connection to MQTT-Server, MQTT temporarily deactivated!"));
	}
}

void FadeOut(int dealy, int minBrightness)
{
	int currentBrightness = matrixtBrightness;

	int counter = 25;
	while (counter >= 0)
	{
		currentBrightness = map(counter, 0, 25, minBrightness, matrixtBrightness);
		matrix->setBrightness(currentBrightness);
		matrix->show();
		counter--;
		delay(dealy);
	}
}

void FadeIn(int dealy, int minBrightness)
{
	int currentBrightness = minBrightness;

	int counter = 0;
	while (counter <= 25)
	{
		currentBrightness = map(counter, 0, 25, minBrightness, matrixtBrightness);
		matrix->setBrightness(currentBrightness);
		matrix->show();
		counter++;
		delay(dealy);
	}
}

void ColorFlash(int red, int green, int blue)
{
	for (uint16_t row = 0; row < 8; row++)
	{
		for (uint16_t column = 0; column < 32; column++)
		{
			matrix->drawPixel(column, row, matrix->Color(red, green, blue));
		}
	}
	matrix->show();
}

void ShowBootAnimation()
{
	DrawTextHelper("MY PIXEL", false, false, false, false, false, false, NULL, 0, 255, 0, 2, 1);
	matrix->drawFastHLine(0, 7, 32, matrix->Color(255, 0, 0));
	FadeIn(60, 10);
	FadeOut(60, 10);
	FadeIn(60, 10);
	FadeOut(60, 10);
	FadeIn(60, 10);

	ColoredBarWipe();
	ColoredBarWipeVertical();
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

void ClearTextArea()
{
	int16_t h = 8;
	int16_t w = 24;
	int16_t x = 8;
	int16_t y = 0;

	for (int16_t j = 0; j < h; j++, y++)
	{
		for (int16_t i = 0; i < w; i++)
		{
			matrix->drawPixel(x + i, y, (uint16_t)0);
		}
	}
}

/////////////////////////////////////////////////////////////////////
/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

void sendNTPpacket(IPAddress& address)
{
	memset(packetBuffer, 0, NTP_PACKET_SIZE);

	packetBuffer[0] = 0b11100011;
	packetBuffer[1] = 0;
	packetBuffer[2] = 6;
	packetBuffer[3] = 0xEC;

	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;

	udp.beginPacket(address, 123);
	udp.write(packetBuffer, NTP_PACKET_SIZE);
	udp.endPacket();
}

time_t getNtpTime()
{
	while (udp.parsePacket() > 0);
	sendNTPpacket(timeServerIP);
	uint32_t beginWait = millis();
	while (millis() - beginWait < 1500)
	{
		int size = udp.parsePacket();
		if (size >= NTP_PACKET_SIZE)
		{
			udp.read(packetBuffer, NTP_PACKET_SIZE);
			time_t secsSince1900;

			secsSince1900 = (time_t)packetBuffer[40] << 24;
			secsSince1900 |= (time_t)packetBuffer[41] << 16;
			secsSince1900 |= (time_t)packetBuffer[42] << 8;
			secsSince1900 |= (time_t)packetBuffer[43];
			time_t secsSince1970 = secsSince1900 - 2208988800UL;
			int totalOffset = (clockTimeZone + DSToffset(secsSince1970, clockTimeZone));
			return secsSince1970 + (time_t)(totalOffset * SECS_PER_HOUR);
		}
		yield();

	}
	ntpMaxRetrys++;
	return 0;
}


void HandleNotFound()
{
	if (server.method() == HTTP_OPTIONS)
	{
		server.sendHeader("Access-Control-Allow-Origin", "*");
		server.send(204);
	}

	server.sendHeader("Location", "/update", true);
	server.send(302, "text/plain", "");
}

void HandleScreen()
{
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
	server.sendHeader("Connection", "close");
	server.sendHeader("Access-Control-Allow-Origin", "*");

	if (json.success())
	{
		server.send(200, "application/json", "{\"response\":\"OK\"}");
		Log("HandleScreen", "Incomming Json length: " + String(json.measureLength()));
		CreateFrames(json);
	}
	else
	{
		server.send(406, "application/json", "{\"response\":\"Not Acceptable\"}");
	}

}

void Handle_wifisetup()
{
	WifiSetup();
}

void HandleSetConfig()
{
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(server.arg("plain"));
	server.sendHeader("Connection", "close");

	if (json.success())
	{
		Log("SetConfig", "Incomming Json length: " + String(json.measureLength()));
		SetConfig(json);
		server.send(200, "application/json", "{\"response\":\"OK\"}");
		delay(500);
		ESP.restart();
	}
	else
	{
		server.send(406, "application/json", "{\"response\":\"Not Acceptable\"}");
	}
}

void HandleGetConfig()
{
	server.sendHeader("Connection", "close");
	server.send(200, "application/json", GetConfig());
}

void HandleGetLuxSensor()
{
	server.sendHeader("Connection", "close");
	server.send(200, "application/json", GetLuxSensor());
}

void HandleGetDHTSensor()
{
	server.sendHeader("Connection", "close");
	server.send(200, "application/json", GetDHTSensor());
}

void HandleGetMatrixInfo()
{
	server.sendHeader("Connection", "close");
	server.send(200, "application/json", GetMatrixInfo());
}

void Handle_factoryreset()
{
	File configFile = LittleFS.open("/config.json", "w");
	if (!configFile)
	{
		Log("Handle_factoryreset", "Failed to open config file for reset");
	}
	configFile.println("");
	configFile.close();

	server.sendHeader("Connection", "close");
	server.send(200, "application/json", "{\"response\":\"OK\"}");
	delay(1000);
	WifiSetup();
	ESP.restart();
}

void _setUpdaterError(){
	StreamString str;
	Update.printError(str);
	_updaterError = str.c_str();
}


void setup()
{
	Serial.begin(115200);

	// Mounting FileSystem
	Serial.println(F("Mounting file system..."));
	if(LittleFS.begin())
	{
		Serial.println(F("Mounted file system."));
		LoadConfig();
	}
	else
	{
		Serial.println(F("Failed to mount FS"));
	}

	// Matix Type 1
	if (matrixType == 1)
	{
		matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
	}
	// Matix Type 2
	else
	{
		matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
	}

	// Init LightSensor
	photocell.setPhotocellPositionOnGround(false);

	// Matrix Color Correction
	FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, NUMMATRIX).setCorrection(TypicalSMD5050);
	
	matrix->begin();
	matrix->setTextWrap(false);
	matrix->setBrightness(matrixtBrightness);
	matrix->clear();

	// Bootscreen
	if (bootScreenAktiv)
	{
		ShowBootAnimation();
	}

	// Set config save notify callback
	wifiManager.setSaveConfigCallback(SaveConfigCallback);
	wifiManager.setMinimumSignalQuality();
	wifiManager.setHostname("MyPixel");
	// Config menue timeout 180 seconds. 
	wifiManager.setConfigPortalTimeout(180);
	// Disable Debug-Output
	wifiManager.setDebugOutput(false);
	if (!wifiManager.autoConnect("MyPixel"))
	{
		Log(F("Setup"), F("Wifi failed to connect and hit timeout"));
		delay(3000);
		// Reset and try again, or maybe put it to deep sleep
		ESP.reset();
		delay(5000);
	}

	Log(F("Setup"), F("Wifi connected..."));
	SaveConfig();

	Log(F("Setup"), F("Local IP"));
	Log(F("Setup"), WiFi.localIP().toString());
	Log(F("Setup"), WiFi.gatewayIP().toString());
	Log(F("Setup"), WiFi.subnetMask().toString());

	Log(F("Setup"), F("Starting UDP"));
	udp.begin(2390);
	Log(F("Setup"), "Local port: " + String(udp.localPort()));


	server.on(F("/api/screen"), HTTP_POST, HandleScreen);
	server.on(F("/api/luxsensor"), HTTP_GET, HandleGetLuxSensor);
	server.on(F("/api/dhtsensor"), HTTP_GET, HandleGetDHTSensor);
	server.on(F("/api/matrixinfo"), HTTP_GET, HandleGetMatrixInfo);
	server.on(F("/api/factory_reset"), HTTP_POST, Handle_factoryreset);
	server.on(F("/api/config"), HTTP_POST, HandleSetConfig);
	server.on(F("/api/config"), HTTP_GET, HandleGetConfig);
	server.on(F("/"), HTTP_GET, HandleGetMainPage);
	server.on(F("/dash"), HTTP_GET, HandleGetDashPage);
	server.on(F("/config"), HTTP_GET, HandleGetConfigPage);
	server.on(F("/testarea"), HTTP_GET, HandleGetTestAreaPage);
	server.on(F("/update"), HTTP_GET, HandleGetUpdatePage);
	

	server.on(F("/update"), HTTP_POST, [&](){
      if (Update.hasError()) {
        server.send(200, "application/json", "{\"result\":\"failed\", \"message\":\"" + String(_updaterError) + "\"}"); 
      } else {
        server.client().setNoDelay(true);
        server.send(200, "application/json", "{\"result\":\"success\", \"message\":\"Update Success! Rebooting...\"}"); 
        delay(100);
        server.client().stop();
        ESP.restart();
      }
    },[&](){
      // handler for the file upload, get's the sketch bytes, and writes
      // them through the Update object
      HTTPUpload& upload = server.upload();

      if(upload.status == UPLOAD_FILE_START){
        _updaterError.clear();

        WiFiUDP::stopAll();
		Serial.printf("Update: %s %s\n", upload.filename.c_str(), upload.name.c_str());
        if (upload.name == "filesystem") {
          size_t fsSize = ((size_t) &_FS_end - (size_t) &_FS_start);
          close_all_fs();
          if (!Update.begin(fsSize, U_FS)){//start with max available size
          }
        } else {
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (!Update.begin(maxSketchSpace, U_FLASH)){//start with max available size
            _setUpdaterError();
          }
        }
      } else if(upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()){
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          _setUpdaterError();
        }
      } else if(upload.status == UPLOAD_FILE_END && !_updaterError.length()){
        if(Update.end(true)){ //true to set the size to the current progress
		 	Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          _setUpdaterError();
        }
      } else if(upload.status == UPLOAD_FILE_ABORTED){
        Update.end();
		Serial.println("Update was aborted");
      }
      delay(0);
    });


	server.on(F("/bootbox.all.min.js"), HTTP_GET, []() {server.send(200, "application/javascript", SCRIPT_JS_BOOTBOX);});
	server.on(F("/bootstrap.min.css"), HTTP_GET, []() {server.send(200, "text/css", STYLE_CSS_BOOTSTRAP);});
	server.on(F("/bootstrap.bundle.min.js"), HTTP_GET, []() {server.send(200, "application/javascript", SCRIPT_JS_BOOTSTRAP);});
	server.on(F("/feather.min.js"), HTTP_GET, []() {server.send(200, "application/javascript", SCRIPT_JS_FEATHER);});
	server.on(F("/huebee.min.js"), HTTP_GET, []() {server.send(200, "application/javascript", SCRIPT_JS_HUEBEE);});
	server.on(F("/huebee.min.css"), HTTP_GET, []() {server.send(200, "text/css", STYLE_CSS_HUEBEE);});
	server.on(F("/jquery.min.js"), HTTP_GET, []() {server.send(200, "application/javascript", SCRIPT_JS_JQUERY);});
	server.on(F("/pixelit.js"), HTTP_GET, []() {server.send(200, "application/javascript", SCRIPT_JS_PIXELIT);});
	server.on(F("/site.js"), HTTP_GET, []() {server.send(200, "application/javascript", SCRIPT_JS_SITE);});
	
	server.on(F("/dashboard.css"), HTTP_GET, []() {server.send(200, "text/css", STYLE_CSS_DASHBOARD);});
	
	server.onNotFound(HandleNotFound);

	const char * headerkeys[] = {"User-Agent","Caller"} ;
	size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
	//ask server to track these headers
	server.collectHeaders(headerkeys, headerkeyssize );
	server.begin();

	webSocket.begin();
	webSocket.onEvent(webSocketEvent);

	Log(F("Setup"), F("Webserver started"));

	if (mqttAktiv == true)
	{
		client.setServer(mqttServer.c_str(), mqttPort);
		client.setCallback(callback);
		Log(F("Setup"), F("MQTT started"));
	}

	dht.setup(D1, DHTesp::AM2302);
	Log(F("Setup"), F("DHT started"));

	softSerial.begin(9600);
	Log(F("Setup"), F("Software Serial started"));

}

void loop()
{
	server.handleClient();
	webSocket.loop();

	if (mqttAktiv == true && mqttRetryCounter < mqttMaxRetrys)
	{
		if (!client.connected())
		{			
			MqttReconnect();
		}
		client.loop();		
	}

	if (clockAktiv && now() != clockLastUpdate && ntpRetryCounter < ntpMaxRetrys)
	{
		if (timeStatus() != timeNotSet)
		{
			clockLastUpdate = now();
			DrawClock(false);
		}
		else
		{
			if (!timeServerIP.fromString(ntpServer))
			{
				WiFi.hostByName(ntpServer.c_str(), timeServerIP);
			}
			Log(F("Sync TimeServer"), ntpServer + ": " + timeServerIP.toString() + " waiting for sync");
			setSyncProvider(getNtpTime);
		}
	}


	if (millis() - sendLuxPrevMillis >= 1000)
	{
		sendLuxPrevMillis = millis();
		SendLDR(false);
	}

	if (millis() - sendDHTPrevMillis >= 3000)
	{
		sendDHTPrevMillis = millis();
		SendDHT(false);
	}

	if (millis() - sendInfoPrevMillis >= 3000)
	{
		sendInfoPrevMillis = millis();
		SendMatrixInfo(false);
	}


	if (animateBMPAktivLoop && millis() - animateBMPPrevMillis >= animateBMPDelay)
	{
		animateBMPPrevMillis = millis();
		AnimateBMP(true);
	}

	if (scrollTextAktivLoop && millis() - scrollTextPrevMillis >= scrollTextDelay)
	{
		scrollTextPrevMillis = millis();
		ScrollText(false);
	}	
}