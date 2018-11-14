#include <Arduino.h>

#include <pms.h>

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
Pmsx003 pms(D3, D4);

// epa bins are doubled to avoid floating point math
const uint16_t epa_pm2dot5_low[] = {0, 24, 70, 110, 300, 500, 700}; 
const uint16_t epa_pm2dot5_high[] = {24, 71, 111, 301, 501, 701, 0};
const uint16_t epa_pm10_low[] = {0, 108, 308, 508, 708, 848, 1008, 1208};
const uint16_t epa_pm10_high[] = {108, 308, 508, 708, 848, 1008, 1208, 0};
const uint16_t epa_aqi_low[] = {0, 51, 101, 151, 201, 301, 401}; // Not doubled, no floating point math necessary
const uint16_t epa_aqi_high[] = {50,100,150,200,300,400,500,0};
const uint16_t aqi_fg_color[] = {TFT_BLACK, TFT_BLACK, TFT_BLACK, TFT_WHITE, TFT_WHITE, TFT_WHITE, TFT_WHITE};
const uint16_t aqi_bg_color[] = {TFT_GREEN, TFT_YELLOW, TFT_ORANGE, TFT_RED, TFT_MAGENTA, TFT_MAROON};

// Given a sorted array haystack, find the index of the first element greater than needle
uint16_t find_bin(const uint16_t *haystack, uint16_t needle) {
  uint16_t i = 0;
  while (haystack[i] < needle && haystack[i] != 0) {
    i++;
  }
  return i;
}

uint16_t calc_AQI(uint16_t pm2dot5, uint16_t pm10) {
  uint16_t index, pm2dot5_index, pm10_index, i_high_2dot5, i_low_2dot5, i_high_10, i_low_10;
  uint16_t c_2dot5, c_high_2dot5, c_low_2dot5, c10, c_high_10, c_low_10, aqi_index, aqi_10, aqi_2dot5;
  pm2dot5_index = find_bin(epa_pm2dot5_high, pm2dot5 * 2);
  pm10_index = find_bin(epa_pm10_high, pm10 * 2);
  
  c_low_2dot5 = epa_pm2dot5_low[pm2dot5_index] / 2;
  c_high_2dot5 = epa_pm2dot5_high[pm2dot5_index] / 2;
  i_high_2dot5 = epa_aqi_high[pm2dot5_index];
  i_low_2dot5 = epa_aqi_low[pm2dot5_index];
  aqi_2dot5 = ((i_high_2dot5 - i_low_2dot5)*(pm2dot5 - c_low_2dot5))/(c_high_2dot5 - c_low_2dot5)+i_low_2dot5;

  c_low_10 = epa_pm10_low[pm10_index] / 2;
  c_high_10 = epa_pm10_high[pm10_index] / 2;
  i_high_10 = epa_aqi_high[pm10_index];
  i_low_10 = epa_aqi_low[pm10_index];
  aqi_10 = ((i_high_10 - i_low_10)*(pm10 - c_low_10))/(c_high_10 - c_low_10)+i_low_10;
  
  if (aqi_10 > aqi_2dot5) {
    return aqi_10;
  } else {
    return aqi_2dot5;
  }
}

////////////////////////////////////////

void setup(void) {
	Serial.begin(115200);
  Serial.println("Airquality monitor booting...");
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(4,4,1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.println("Screen initialized.");
	while (!Serial) {};
	Serial.println("Pmsx003");
  tft.println("PMSx003 initialized.");

	pms.begin();
	pms.waitForData(Pmsx003::wakeupTime);
	pms.write(Pmsx003::cmdModeActive);
  tft.println("Waiting for data...");
}

////////////////////////////////////////

auto lastRead = millis();

void loop(void) {

	const auto n = Pmsx003::Reserved;
	Pmsx003::pmsData data[n];
  uint16_t fgtextcolor, bgtextcolor, aqi, aqi_index;
  Pmsx003::pmsData pm2dot5;

	Pmsx003::PmsStatus status = pms.read(data, n);

	switch (status) {
		case Pmsx003::OK:
		{
			Serial.println("_________________");
      
			auto newRead = millis();
			Serial.print("Wait time ");
			Serial.println(newRead - lastRead);
			lastRead = newRead;
      aqi = calc_AQI(data[Pmsx003::PM2dot5], data[Pmsx003::PM10dot0]);
      // Set the color based on the PM2.5 value
      aqi_index = find_bin(epa_aqi_high, aqi);
      Serial.printf("aqi_index:       %d\r\n", aqi_index);
      fgtextcolor = aqi_fg_color[aqi_index];
      bgtextcolor = aqi_bg_color[aqi_index];

      tft.fillScreen(bgtextcolor);
      tft.setTextColor(fgtextcolor, bgtextcolor);
      tft.setCursor(0,4,2);
      tft.printf(" AIR PARTICULATE \n");
			// For loop starts from 3
			// Skip the first three data (PM1dot0CF1, PM2dot5CF1, PM10CF1)
			for (size_t i = Pmsx003::PM1dot0; i <= Pmsx003::PM10dot0; ++i) { 
				Serial.print(data[i]);
        // tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf(" %4d  %s\n",data[i], Pmsx003::dataNames[i]);
				Serial.print("\t");
				Serial.print(Pmsx003::dataNames[i]);
				Serial.print(" [");
				Serial.print(Pmsx003::metrics[i]);
				Serial.print("]");
				Serial.println();
			}
      Serial.printf("AQI: %d", aqi);
      tft.setTextSize(3);
      tft.printf("%4d", aqi);
      tft.setTextSize(1);
      tft.setCursor(32,96,1);
      tft.setTextColor(TFT_BLUE, bgtextcolor);
      tft.printf("\n\n\n    #hacktheplanet  ");
			break;
		}
		case Pmsx003::noData:
			break;
		default:
			Serial.println("_________________");
			Serial.println(Pmsx003::errorMsg[status]);
      tft.printf("Error!\n%s\n", Pmsx003::errorMsg[status]);
	};
}