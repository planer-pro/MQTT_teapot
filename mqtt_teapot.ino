#include <OneWire.h>

OneWire sensDs(4);//sensor pin

#define POWER_MODE 0 // sensor power mode, 0 - external, 1 - parasitic
#define relay 5//heater relay pin
#define butt 0//button pin
#define waterSenseLow 13//water level pin
#define waterSenseMed 12
#define waterSenseHi 14
#define led 2//led indicator pin

byte bufData[9];

float temperature;//measure temp from sensor
float oldTemp = -999;
bool tempChange = false;//temp change flag

bool checkBtn = true;
bool buttChange = false;//button change flag
int buttState;
int btnTime;//button time measure

int waterLevel = 0;
int oldWaterLevel = -999;
bool waterLevelChange = false;//water level change flag

float setTempBoil = 28.3;//working boil temp
float setTempTermo = 30.4;//working termopot temp
bool setHot = false;

int modeState = 0;//0 - all modes are off, 1 - boil, 2 - termopot

unsigned long milStr;
unsigned long debounce;

void setup()
{
	Serial.begin(115200);
	pinMode(butt, INPUT);
	pinMode(led, OUTPUT);
	pinMode(relay, OUTPUT);
	pinMode(waterSenseLow, INPUT);
	pinMode(waterSenseMed, INPUT);
	pinMode(waterSenseHi, INPUT);
	digitalWrite(led, 1);//off
	digitalWrite(relay, 1);//off
}

void GetButton()
{
	if (!digitalRead(0) && checkBtn)
	{
		milStr = millis();
		checkBtn = false;
		buttChange = false;
	}
	if (millis() >= debounce)
	{
		if (digitalRead(0) && !checkBtn)
		{
			btnTime = millis() - milStr;
			if (btnTime < 300) buttState = 1;//short tick
			else if (btnTime >= 300 && btnTime < 1500) buttState = 2;//medium press
			else if (btnTime >= 1500) buttState = 3;//long press
			checkBtn = true;
			buttChange = true;
		}
		debounce = millis() + 50;
	}
}

void GetTemp()
{
	sensDs.reset();  // bus reset
	sensDs.write(0xCC, POWER_MODE); // shift ROM
	sensDs.write(0x44, POWER_MODE); // init change
	sensDs.reset();  // сброс шины
	sensDs.write(0xCC, POWER_MODE); // shift ROM
	sensDs.write(0xBE, POWER_MODE); // command read memoty sensor
	sensDs.read_bytes(bufData, 9);  // read memoty sensor, 9 bytes

	if (OneWire::crc8(bufData, 8) == bufData[8])// verify CRC
	{
		temperature = (float)((int)bufData[0] | (((int)bufData[1]) << 8)) * 0.0625 + 0.03125;
	}

	if (temperature != oldTemp) 
	{
		oldTemp = temperature;
		tempChange = true;
	}
}

void GetWaterLevel()
{
	bool low = digitalRead(waterSenseLow);
	bool med = digitalRead(waterSenseMed);
	bool hi = digitalRead(waterSenseHi);

	if (low && med && hi) waterLevel = 0;//no water
	else if (!low && med && hi) waterLevel = 1;//low
	else if (!low && !med && hi) waterLevel = 2;//medium
	else if (!low && !med && !hi) waterLevel = 3;//hi
	else waterLevel = 4;//clear sensor

	if(waterLevel != oldWaterLevel)
	{
		oldWaterLevel = waterLevel;
		waterLevelChange = true;
	}
}

void doHot(float t)
{
	if (waterLevel > 0 && waterLevel != 4)
	{
		if (temperature <= t && t != 0)
		{
			digitalWrite(led, 0);//on
			digitalWrite(relay, 0);//on
		}
		if (temperature > t && t != 0)
		{
			if (modeState == 1)//if boiling mode
			{
				digitalWrite(led, 1);//off
				digitalWrite(relay, 1);//off
				setHot = false;
				modeState = 0;
				Serial.println("Boil complete");
			}
			if (modeState == 2)//if termopot mode
			{
				digitalWrite(led, 1);//off
				digitalWrite(relay, 1);//off
			}
		}
		if (t == 0)
		{
			digitalWrite(led, 1);//off
			digitalWrite(relay, 1);//off
			setHot = false;
			modeState = 0;
			Serial.println("Operation interupted");
		}
	}
	else
	{
		if (waterLevel == 0)
		{
			Serial.print("No water, code: ");
			Serial.println(waterLevel);
		}
		if (waterLevel == 4)
		{
			Serial.print("Clear sensor, code: ");
			Serial.println(waterLevel);
		}
		digitalWrite(led, 1);//off
		digitalWrite(relay, 1);//off

		modeState = 0;
		setHot = false;
	}
}

void loop()
{
	GetButton();
	GetWaterLevel();
	GetTemp();

	if (tempChange)//event temp change
	{
		tempChange = false;
		Serial.println(temperature);
	}

	if (waterLevelChange)//event water level change
	{
		waterLevelChange = false;
		Serial.println(waterLevel);
	}

	if (setHot)//activate operation
	{
		if (modeState == 1) doHot(setTempBoil);//kind of operation
		if (modeState == 2) doHot(setTempTermo);
	}

	if (buttChange && buttState == 1)//button pressed? what button?
	{
		setHot = !setHot;//use for interrupt by user
		if (!setHot)
		{
			doHot(0);//interrupt command
		}
		else
		{
			modeState = 1;
			Serial.println("Boil begin");
		}

		buttChange = false;
	}

	if (buttChange && buttState == 2)//button pressed? what button?
	{
		setHot = true;
		modeState = 2;
		Serial.println("Termopot begin");

		buttChange = false;
	}
}