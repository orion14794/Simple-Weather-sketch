#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* WIFI_SSID = "your ssid";
const char* WIFI_PASSWORD = "password";
const char* OPEN_WEATHER_MAP_API_KEY = "openweather api key";
String LOCATION = "your location";

#define TFT_DARKBLUE 0x000D
#define TFT_LIGHTYELLOW 0xF7BE
#define TFT_GREY 0x8410
#define TFT_SKYBLUE 0x5D1C
#define TFT_BLACK 0x0000
#define TFT_GREEN 0x07E0

#define PIN_BUTTON1 0
#define PIN_BUTTON2 14

TFT_eSPI tft = TFT_eSPI(170, 320); // Adjusted resolution for your display
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -5 * 3600, 60000); // Adjust for your timezone

int iconX = 0; // X-coordinate for the icon
int iconDirection = 1; // 1 for moving right, -1 for moving left
int brightness = 255; // Initial screen brightness

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;
const int backlight[5] = {10, 30, 60, 120, 220};

void setup() {
  pinMode(15, OUTPUT); // Set pin 15 as output for external battery power
  digitalWrite(15, HIGH); // Turn on power to pin 15
  tft.init();
  tft.setRotation(0); // Set to portrait mode
  tft.fillScreen(TFT_DARKBLUE);
  tft.setTextColor(TFT_LIGHTYELLOW);
  tft.setTextSize(2); // Increase font size for better readability

  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  pinMode(PIN_BUTTON2, INPUT_PULLUP);

  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
  ledcAttachPin(TFT_BL, pwmLedChannelTFT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  timeClient.begin();
}

void loop() {
  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin(String("http://api.openweathermap.org/data/2.5/weather?q=") + LOCATION + "&appid=" + OPEN_WEATHER_MAP_API_KEY);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      JsonObject responseObject = doc.as<JsonObject>();
      if (!responseObject.isNull()) {
        // Assuming the responseObject contains "weather", "main", and "wind"
        String weatherDescription = responseObject["weather"][0]["description"];
        String icon = responseObject["weather"][0]["icon"];
        float windDirectionDegrees = responseObject["wind"]["deg"];
        float windSpeedMps = responseObject["wind"]["speed"]; // Wind speed in meters per second
        float windSpeedMph = windSpeedMps * 2.23694; // Convert wind speed to miles per hour
        String cardinalPoint = getCardinalDirection(windDirectionDegrees);

        // Change the background color according to weather conditions and day cycles
        if (icon.endsWith("d")) {
          if (timeClient.getHours() < 6 || timeClient.getHours() >= 18) {
            tft.fillScreen(TFT_BLACK); // Nighttime background
          } else if (timeClient.getHours() >= 6 && timeClient.getHours() < 12) {
            tft.fillScreen(TFT_SKYBLUE); // Morning background
          } else if (timeClient.getHours() >= 12 && timeClient.getHours() < 17) {
            tft.fillScreen(TFT_YELLOW); // Afternoon background
          } else {
            tft.fillScreen(TFT_ORANGE); // Evening background
          }
        } else {
          tft.fillScreen(TFT_BLACK); // Nighttime background
        }

        displayWeatherIcon(icon);
        displayTimeAndDate();
        displayLocation();
        displayWeather(weatherDescription, cardinalPoint, windSpeedMph);
        displayIPAddress();

        iconX += 5 * iconDirection; // Move the icon 5 pixels in the current direction
        if (iconX > tft.width() - 60) { // If the icon has reached the right edge (adjust the value 60 as needed)
          iconDirection = -1; // Reverse direction to left
        } else if (iconX < 0) { // If the icon has reached the left edge
          iconDirection = 1; // Reverse direction to right
        }
      }
    }
    http.end();
  }

  // Adjust screen brightness
  int brightnessStep = 255 * 0.10; // 10% of the maximum brightness
  if (digitalRead(PIN_BUTTON1) == LOW) { // Check if the button is pressed
    brightness -= brightnessStep;
    if (brightness < 0) {
      brightness = 0;
    }
    ledcWrite(pwmLedChannelTFT, brightness);
  }
  if (digitalRead(PIN_BUTTON2) == LOW) { // Check if the button is pressed
    brightness += brightnessStep;
    if (brightness > 255) {
      brightness = 255;
    }
    ledcWrite(pwmLedChannelTFT, brightness);
  }

  delay(1000); // Update every second for animated time display
}

void displayWeatherIcon(String icon) {
  // Clear the icon area before drawing a new icon
  tft.fillRect(iconX, 0, 60, 60, TFT_DARKBLUE);

  if (icon == "01d") {
    // Draw a larger sun icon
    tft.fillCircle(iconX + 30, 30, 20, TFT_YELLOW); // Draw the sun body
    tft.drawCircle(iconX + 30, 30, 25, TFT_YELLOW); // Draw the sun rays
  } else if (icon == "01n") {
    // Draw a larger moon icon
    tft.fillCircle(iconX + 30, 30, 20, TFT_WHITE); // Draw the moon body
    tft.drawCircle(iconX + 30, 30, 25, TFT_WHITE); // Draw the moon glow
  } else if (icon == "02d" || icon == "02n" || icon == "03d" || icon == "03n" || icon == "04d" || icon == "04n") {
    // Draw a larger cloud icon
    tft.fillCircle(iconX + 30, 30, 20, TFT_WHITE);
    tft.fillCircle(iconX + 20, 30, 20, TFT_WHITE);
    tft.fillCircle(iconX + 40, 30, 20, TFT_WHITE);
  } else if (icon == "09d" || icon == "09n") {
    // Draw a larger rain icon
    tft.fillCircle(iconX + 30, 30, 20, TFT_BLUE);
    tft.drawLine(iconX + 30, 30, iconX + 30, 50, TFT_BLUE); // Draw the rain drop
  } else if (icon == "10d" || icon == "10n") {
    // Draw a larger rain cloud icon
    tft.fillCircle(iconX + 30, 30, 20, TFT_GREY);
    tft.fillCircle(iconX + 20, 30, 20, TFT_GREY);
    tft.fillCircle(iconX + 40, 30, 20, TFT_GREY);
  } else if (icon == "11d" || icon == "11n") {
    // Draw a larger thunderstorm icon
    tft.fillCircle(iconX + 30, 30, 20, TFT_GREY);
    tft.fillCircle(iconX + 20, 30, 20, TFT_GREY);
    tft.fillCircle(iconX + 40, 30, 20, TFT_GREY);
    tft.drawLine(iconX + 30, 30, iconX + 30, 50, TFT_YELLOW); // Draw the lightning bolt
  } else if (icon == "13d" || icon == "13n") {
    // Draw a larger snow icon
    tft.fillCircle(iconX + 30, 30, 20, TFT_WHITE);
    tft.drawCircle(iconX + 30, 30, 25, TFT_WHITE); // Draw the snow glow
  } else if (icon == "50d" || icon == "50n") {
    // Draw a larger mist icon
    tft.fillCircle(iconX + 30, 30, 20, TFT_LIGHTGREY);
  }
}

void displayTimeAndDate() {
  timeClient.update();
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3); // Increase the font size for time
  tft.setCursor((tft.width() - tft.textWidth(timeClient.getFormattedTime())) / 2, 70);
  tft.println(timeClient.getFormattedTime());

  tft.setTextSize(1); // Reduce the font size for date
  tft.setCursor((tft.width() - tft.textWidth(timeClient.getFormattedDate())) / 2, 110); // Move date information 1 more line below time
  tft.println(timeClient.getFormattedDate());
}

void displayLocation() {
  tft.setTextColor(TFT_WHITE);
  tft.setCursor((tft.width() - tft.textWidth("Location: " + String(LOCATION))) / 2, 130); // Move location 2 lines below date
  tft.println("Location: " + String(LOCATION));
}

void displayWeather(String weatherDescription, String cardinalPoint, float windSpeedMph) {
  tft.setTextColor(TFT_LIGHTYELLOW);
  tft.setTextSize(1); // Reset the font size
  tft.setCursor((tft.width() - tft.textWidth("Weather: " + weatherDescription)) / 2, tft.height() - 90); // Move weather conditions 7 lines above IP address
  tft.println("Weather: " + weatherDescription);

  tft.setTextColor(TFT_SKYBLUE);
  tft.setCursor((tft.width() - tft.textWidth("Wind direction: " + cardinalPoint)) / 2, tft.height() - 70); // Add a blank line below
  tft.println("Wind direction: " + cardinalPoint);

  tft.setCursor((tft.width() - tft.textWidth("Wind speed: " + String(windSpeedMph) + " mph")) / 2, tft.height() - 50); // Add a blank line below
  tft.println("Wind speed: " + String(windSpeedMph) + " mph");
}

void displayIPAddress() {
  tft.setTextColor(TFT_WHITE);
  tft.setCursor((tft.width() - tft.textWidth("IP Address: " + WiFi.localIP().toString())) / 2, tft.height() - 30); // Move IP address to bottom left of screen
  tft.println("IP Address: " + WiFi.localIP().toString());
}

String getCardinalDirection(float degrees) {
  if (degrees >= 337.5 || degrees < 22.5)
    return "N";
  else if (degrees >= 22.5 && degrees < 67.5)
    return "NE";
  else if (degrees >= 67.5 && degrees < 112.5)
    return "E";
  else if (degrees >= 112.5 && degrees < 157.5)
    return "SE";
  else if (degrees >= 157.5 && degrees < 202.5)
    return "S";
  else if (degrees >= 202.5 && degrees < 247.5)
    return "SW";
  else if (degrees >= 247.5 && degrees < 292.5)
    return "W";
  else if (degrees >= 292.5 && degrees < 337.5)
    return "NW";
  else
    return "Invalid";
}
