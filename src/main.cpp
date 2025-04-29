/*
 *  GearPulse - ESP Powered PC Hardware Monitor
 *  --------------------------------------
 *  Monitor your PC's CPU, GPU, RAM, Network usage, and more in real-time
 *  using a ESP 8266.
 *  
 *  Author: Shakil Ahmed
 *  Project Name: GearPulse
 *  Website/Portfolio: https://shakilahmeed.com
 *  GitHub: https://github.com/ShakilAhmed0369
 *  Created: 2025-04-29
 *  License: MIT
 */

#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// Serial port settings
const int SERIAL_BAUD_RATE = 115200;

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

// TTP223 Touch sensor
const int TOUCH_PIN = D5;

// Touch handling
unsigned long touchStartTime = 0;
const unsigned long LONG_PRESS_TIME = 2000;
bool touchActive = false;
bool lastTouchState = false;
bool isPowerOn = true;

// Display mode
enum DisplayMode { CPU, MEMORY, NETWORK, DIVERGENCE, TOTAL_MODES };
DisplayMode currentMode = CPU;

// Divergence meter states
enum DivergenceState { 
  ANIMATION,    // Running animation (15 seconds)
  DISPLAY_VALUE // Displaying current value (60 seconds)
};
DivergenceState divergenceState = DISPLAY_VALUE;

// Divergence meter animation
unsigned long lastStateChangeTime = 0;
const unsigned long ANIMATION_DURATION = 15000;   // 15 seconds animation
const unsigned long DISPLAY_DURATION = 60000;     // 60 seconds (1 minute) display
float currentDivergence = 1.048596;
float targetDivergence = 1.048596;
char divergenceBuffer[10]; // Buffer to store divergence value as string

// System data structure
struct SystemData {
  float cpuLoad, cpuTemp;
  float gpuLoad, gpuTemp;
  float ramTotal, ramUsed, ramPercent;
  float netUpload, netDownload;
} sysData;

// Custom character definitions - moved to PROGMEM to save RAM
const PROGMEM byte upArrow[8] = {
  0b00100, 0b01110, 0b10101, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000
};
const PROGMEM byte downArrow[8] = {
  0b00100, 0b00100, 0b00100, 0b00100, 0b10101, 0b01110, 0b00100, 0b00000
};
const PROGMEM byte barChars[6][8] = {
  { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000 }, // empty
  { 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000 },
  { 0b11000, 0b11000, 0b11000, 0b11000, 0b11000, 0b11000, 0b11000, 0b11000 },
  { 0b11100, 0b11100, 0b11100, 0b11100, 0b11100, 0b11100, 0b11100, 0b11100 },
  { 0b11110, 0b11110, 0b11110, 0b11110, 0b11110, 0b11110, 0b11110, 0b11110 },
  { 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111 }  // full
};

// Serial buffer
const size_t SERIAL_BUFFER_SIZE = 1024;
char serialBuffer[SERIAL_BUFFER_SIZE];
size_t serialBufferIndex = 0;

// Display tracking buffer to reduce flicker
char previousDisplayLines[2][17]; // 16 chars + null terminator for each line

// Function prototypes
void showMessage(const __FlashStringHelper* line1, const __FlashStringHelper* line2 = nullptr);
void showMessage(const __FlashStringHelper* line1, const String& line2);
void updateDisplayLine(uint8_t line, const char* newContent);
void powerOn();
void powerOff();
void changeDisplayMode();
void updateDisplay();
void drawProgressBar(uint8_t percent);
String formatNetSpeed(float bytesPerSec);
bool parseJsonData(const char* jsonString);
float generateRandomDivergenceValue();
void displayDivergence(float value);
void updateDivergenceAnimation();
void formatDivergenceValue(float value, char* buffer);
void updateDivergenceState();
void startDivergenceAnimation();
void addWorldLineInfo(float value);
void processSerialData();

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println(F("\nGearPulse - ESP Powered PC Hardware Monitor"));

  // Initialize random seed with a floating pin reading
  randomSeed(analogRead(A0));

  pinMode(TOUCH_PIN, INPUT);
  lcd.init();
  
  // Load custom characters from PROGMEM
  byte tempChar[8];
  memcpy_P(tempChar, upArrow, 8);
  lcd.createChar(0, tempChar);
  memcpy_P(tempChar, downArrow, 8);
  lcd.createChar(1, tempChar);
  
  for (uint8_t i = 0; i < 6; i++) {
    memcpy_P(tempChar, barChars[i], 8);
    lcd.createChar(i + 2, tempChar);
  }
  
  // Initialize display tracking buffer
  memset(previousDisplayLines, 0, sizeof(previousDisplayLines));
  
  powerOn();
  
  // Initial divergence value
  targetDivergence = generateRandomDivergenceValue();
  formatDivergenceValue(targetDivergence, divergenceBuffer);
  currentDivergence = targetDivergence;
  
  // Setup the initial timing
  lastStateChangeTime = millis();
}

void loop() {
  unsigned long currentTime = millis();
  
  if (isPowerOn) {
    // Process serial data
    processSerialData();
    
    // Handle divergence animation and display cycles
    if (currentMode == DIVERGENCE) {
      updateDivergenceState();
    }
  }

  // Handle touch sensor
  bool currentTouchState = digitalRead(TOUCH_PIN);

  // Touch state machine with debouncing
  if (currentTouchState && !lastTouchState) {
    touchStartTime = millis();
    touchActive = true;
  }

  if (!currentTouchState && lastTouchState && touchActive) {
    unsigned long duration = millis() - touchStartTime;
    if (duration >= LONG_PRESS_TIME) {
      isPowerOn ? powerOff() : powerOn();
    } else if (isPowerOn) {
      changeDisplayMode();
    }
    touchActive = false;
  }

  lastTouchState = currentTouchState;
  
  // Add a small delay to prevent CPU hogging
  delay(10);
}

// Process data from serial port
void processSerialData() {
  while (Serial.available()) {
    char c = Serial.read();
    
    // Add character to buffer if there's space
    if (serialBufferIndex < SERIAL_BUFFER_SIZE - 1) {
      serialBuffer[serialBufferIndex++] = c;
    }
    
    // If end of JSON detected, process it
    if (c == '\n' || c == '\r') {
      // Null terminate the string
      serialBuffer[serialBufferIndex] = '\0';
      
      // Only try to parse if there's actual content
      if (serialBufferIndex > 2) {  // Minimum valid JSON is "{}"
        if (parseJsonData(serialBuffer)) {
          if (currentMode != DIVERGENCE) {
            updateDisplay();
          }
        }
      }
      
      // Reset buffer for next JSON
      serialBufferIndex = 0;
    }
  }
}

void updateDivergenceState() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - lastStateChangeTime;
    
  switch (divergenceState) {
    case ANIMATION:
      // During animation, update animation frames
      updateDivergenceAnimation();
      
      // Check if animation time is complete
      if (elapsedTime >= ANIMATION_DURATION) {
        // Animation complete, transition to display state
        divergenceState = DISPLAY_VALUE;
        lastStateChangeTime = currentTime;
        
        // Set the current divergence to target and display with world line info
        currentDivergence = targetDivergence;
        displayDivergence(currentDivergence);
        addWorldLineInfo(currentDivergence);
        
        Serial.println(F("Divergence animation complete, displaying value"));
      }
      break;
      
    case DISPLAY_VALUE:
      // Just display the current value and world line info
      // Check if display time is complete
      if (elapsedTime >= DISPLAY_DURATION) {
        // Display time complete, start new animation cycle
        startDivergenceAnimation();
        divergenceState = ANIMATION;
        lastStateChangeTime = currentTime;
        
        Serial.println(F("Starting new divergence animation cycle"));
      }
      break;
  }
}

void startDivergenceAnimation() {
  // Generate a new target divergence value
  float newTarget = generateRandomDivergenceValue();
  
  // Make sure it's different from the current one
  while (abs(newTarget - currentDivergence) < 0.000001) {
    newTarget = generateRandomDivergenceValue();
  }
  
  targetDivergence = newTarget;
  formatDivergenceValue(targetDivergence, divergenceBuffer);
  
  Serial.print(F("New divergence target: "));
  Serial.println(targetDivergence, 6);
}

// Update a single line of the display only if its content has changed
void updateDisplayLine(uint8_t line, const char* newContent) {
  // Check if the new content is different from what's currently displayed
  if (strcmp(newContent, previousDisplayLines[line]) != 0) {
    // Content is different, update the display
    lcd.setCursor(0, line);
    
    // First clear the line with spaces
    lcd.print("                "); // 16 spaces
    
    // Then write the new content
    lcd.setCursor(0, line);
    lcd.print(newContent);
    
    // Save the new content
    strcpy(previousDisplayLines[line], newContent);
  }
}

void showMessage(const __FlashStringHelper* line1, const __FlashStringHelper* line2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  if (line2) {
    lcd.setCursor(0, 1); 
    lcd.print(line2);
  }
  
  // Update the tracking buffer with the new content
  strncpy(previousDisplayLines[0], (const char*)line1, 16);
  previousDisplayLines[0][16] = '\0';
  
  if (line2) {
    strncpy(previousDisplayLines[1], (const char*)line2, 16);
  } else {
    strcpy(previousDisplayLines[1], "                ");
  }
  previousDisplayLines[1][16] = '\0';
}

void showMessage(const __FlashStringHelper* line1, const String& line2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
  
  // Update the tracking buffer with the new content
  strncpy(previousDisplayLines[0], (const char*)line1, 16);
  previousDisplayLines[0][16] = '\0';
  
  strncpy(previousDisplayLines[1], line2.c_str(), 16);
  previousDisplayLines[1][16] = '\0';
}

void powerOn() {
  lcd.backlight();
  showMessage(F("GearPulse"));

  delay(600);

  showMessage(F("System Monitor"), F("Starting..."));

  // Initialize system data to zero
  memset(&sysData, 0, sizeof(sysData));
  
  // Clear previous display lines
  memset(previousDisplayLines, 0, sizeof(previousDisplayLines));
  strcpy(previousDisplayLines[0], "                ");
  strcpy(previousDisplayLines[1], "                ");
  
  isPowerOn = true;

  delay(1000);
  showMessage(F("System Ready"), F("Waiting for data"));
  delay(1000);
  updateDisplay();
}

void powerOff() {
  isPowerOn = false;
  
  // Clear system data
  memset(&sysData, 0, sizeof(sysData));
  currentMode = CPU;
  divergenceState = DISPLAY_VALUE;  // Reset state for next power on

  showMessage(F("Powering Off..."));
  delay(1000);
  lcd.noBacklight();

  Serial.println(F("System powered off"));
}

void changeDisplayMode() {
  currentMode = static_cast<DisplayMode>((currentMode + 1) % TOTAL_MODES);
  
  // When switching to divergence mode, reset the state and timers
  if (currentMode == DIVERGENCE) {
    divergenceState = DISPLAY_VALUE;
    lastStateChangeTime = millis();
    displayDivergence(currentDivergence);
    addWorldLineInfo(currentDivergence);
  } else {
    updateDisplay();
  }
}

void updateDisplay() {
  if (currentMode == DIVERGENCE) {
    displayDivergence(currentDivergence);
    addWorldLineInfo(currentDivergence);
    return;
  }
  
  // Create buffers for the new display content
  char newLine0[17] = {0}; // 16 characters + null terminator
  char newLine1[17] = {0};
  
  switch (currentMode) {
    case CPU:  // Show CPU + GPU info
      sprintf_P(newLine0, PSTR("CPU:  %d%cC %.1f%%"), 
               static_cast<int>(round(sysData.cpuTemp)), 
               223, // ° symbol
               sysData.cpuLoad);
               
      sprintf_P(newLine1, PSTR("GPU:  %d%cC %.1f%%"), 
               static_cast<int>(round(sysData.gpuTemp)), 
               223, // ° symbol
               sysData.gpuLoad);
      break;

    case MEMORY:
      sprintf_P(newLine0, PSTR("RAM: %d/%dGB %d%%"), 
               static_cast<int>(sysData.ramUsed),
               static_cast<int>(sysData.ramTotal),
               static_cast<int>(sysData.ramPercent));
      
      // For the progress bar, we'll update the entire line
      strcpy(newLine1, "                "); // 16 spaces
      break;

    case NETWORK:
      strcpy(newLine0, "NET:");
      
      char buffer[17] = {0};
      buffer[0] = 1; // down arrow character code
      strcat(buffer, ":");
      strcat(buffer, formatNetSpeed(sysData.netDownload).c_str());
      strcat(buffer, " ");
      int pos = strlen(buffer);
      buffer[pos] = 0; // up arrow character
      buffer[pos+1] = '\0';
      strcat(buffer, ":");
      strcat(buffer, formatNetSpeed(sysData.netUpload).c_str());
      strcpy(newLine1, buffer);
      break;
  }

  // Update only the parts of the display that have changed
  updateDisplayLine(0, newLine0);
  updateDisplayLine(1, newLine1);
  
  // Special case for memory progress bar
  if (currentMode == MEMORY) {
    lcd.setCursor(0, 1);
    drawProgressBar(static_cast<uint8_t>(sysData.ramPercent));
    
    // Update the tracking buffer with the progress bar
    char progressBuffer[17] = "                ";
    strcpy(previousDisplayLines[1], progressBuffer);
  }
  
  // Special case for network display
  if (currentMode == NETWORK) {
    // Handle special characters in the display manually
    lcd.setCursor(0, 1);
    lcd.write(1); // down arrow
    lcd.setCursor(strlen(newLine1) - formatNetSpeed(sysData.netUpload).length() - 2, 1);
    lcd.write(0); // up arrow
  }
}

String formatNetSpeed(float bytesPerSec) {
  char buffer[10];
  
  if (bytesPerSec < 1024.0) {
    sprintf_P(buffer, PSTR("%.0fB"), bytesPerSec);
  } else if (bytesPerSec < 1024.0 * 1024.0) {
    sprintf_P(buffer, PSTR("%.0fK"), bytesPerSec / 1024.0);
  } else {
    sprintf_P(buffer, PSTR("%.1fM"), bytesPerSec / (1024.0 * 1024.0));
  }
  
  return String(buffer);
}

void drawProgressBar(uint8_t percent) {
  constexpr uint8_t totalBlocks = 16;  // LCD width
  uint8_t fullChars = (percent * totalBlocks * 5) / 100 / 5;
  uint8_t remainder = ((percent * totalBlocks * 5) / 100) % 5;
  
  // Build the progress bar in our tracking buffer first
  char progressBar[17];
  
  for (uint8_t i = 0; i < totalBlocks; i++) {
    if (i < fullChars) {
      progressBar[i] = 7;  // full block character code
    } else if (i == fullChars && remainder > 0) {
      progressBar[i] = 2 + remainder; // partial block character code
    } else {
      progressBar[i] = 2; // empty block character code
    }
  }
  progressBar[16] = '\0';
  
  // Compare with previous state
  if (strcmp(progressBar, previousDisplayLines[1]) != 0) {
    // Only update if different
    lcd.setCursor(0, 1);
    
    for (uint8_t i = 0; i < totalBlocks; i++) {
      lcd.write(progressBar[i]);
    }
    
    // Update the tracking buffer
    strcpy(previousDisplayLines[1], progressBar);
  }
}

bool parseJsonData(const char* jsonString) {
  // Using StaticJsonDocument instead of DynamicJsonDocument to avoid heap fragmentation
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print(F("JSON parse error: "));
    Serial.println(error.c_str());
    return false;
  }

  // Use temporary variables to ensure atomic updates
  SystemData tempData;
  
  tempData.cpuLoad = doc["cpu"]["load"].as<float>();
  tempData.cpuTemp = doc["cpu"]["temp"].as<float>();
  tempData.gpuLoad = doc["gpu"]["load"].as<float>();
  tempData.gpuTemp = doc["gpu"]["temp"].as<float>();
  tempData.ramTotal = doc["ram"]["total"].as<float>();
  tempData.ramUsed = doc["ram"]["used"].as<float>();
  tempData.ramPercent = doc["ram"]["usagePercent"].as<float>();
  tempData.netUpload = doc["network"]["upload"].as<float>();
  tempData.netDownload = doc["network"]["download"].as<float>();
  
  // Atomic update of the system data
  memcpy(&sysData, &tempData, sizeof(SystemData));
  
  return true;
}

// Format a float value to standard divergence notation string (x.xxxxxx)
void formatDivergenceValue(float value, char* buffer) {
  dtostrf(value, 1, 6, buffer);
}

// Generate a random Steins;Gate style divergence value
float generateRandomDivergenceValue() {
  // Common divergence values from Steins;Gate with higher probability:
  // 0.000000% (Alpha Attractor Field)
  // 0.571024% (Beta Attractor Field) 
  // 1.048596% (Steins Gate world line)
  // 1.130426% (Gamma Attractor Field)
  
  // Generate a random value with preference for canonical ones
  int randomVal = random(100);
  
  if (randomVal < 25) {
    return 1.048596; // Steins Gate
  } else if (randomVal < 40) {
    return 0.000000; // Alpha
  } else if (randomVal < 55) {
    return 0.571024; // Beta
  } else if (randomVal < 70) {
    return 1.130426; // Gamma
  } else {
    // Generate a random divergence value between 0 and 3
    float randomDivergence = random(300000) / 100000.0;
    return randomDivergence;
  }
}

// Add world line information based on the divergence value
void addWorldLineInfo(float value) {
  char newLine0[17] = "                ";
  
  // Display the appropriate world line name - all strings should be 16 chars or less
  if (abs(value - 1.048596) < 0.000001) {
    strncpy(newLine0, "  STEINS;GATE   ", 16);
  } else if (value < 0.500000) {
    strncpy(newLine0, " ALPHA WORLDLINE", 16);
  } else if (value < 1.000000) {
    strncpy(newLine0, " BETA WORLDLINE ", 16);
  } else if (value < 1.100000) {
    strncpy(newLine0, "  STEINS GATE   ", 16);
  } else if (value < 2.000000) {
    strncpy(newLine0, " GAMMA WORLDLINE", 16);
  } else if (value < 3.000000) {
    strncpy(newLine0, " DELTA WORLDLINE", 16);
  } else {
    strncpy(newLine0, "UNKNOWN WORLDLIN", 16);
  }
  
  // Ensure null termination
  newLine0[16] = '\0';
  
  // Update only if different
  updateDisplayLine(0, newLine0);
}

void displayDivergence(float value) {
  char newLine0[17] = "   DIVERGENCE   ";
  
  // Format to standard divergence notation
  char buffer[10];
  formatDivergenceValue(value, buffer);
  
  // Center the value in the second line
  char newLine1[17] = "                ";
  int startPos = (16 - strlen(buffer)) / 2;
  memcpy(newLine1 + startPos, buffer, strlen(buffer));
  
  // Update only if different
  updateDisplayLine(0, newLine0);
  updateDisplayLine(1, newLine1);
}

// This function creates a smooth animation effect for the divergence meter
void updateDivergenceAnimation() {
  static unsigned long lastAnimationUpdate = 0;
  unsigned long currentTime = millis();
  
  // Update animation at appropriate intervals
  if (currentTime - lastAnimationUpdate < 40) {
    return;  // Don't update too frequently
  }
  lastAnimationUpdate = currentTime;
  
  // Set title 
  char newLine0[17] = "   DIVERGENCE   ";
  updateDisplayLine(0, newLine0);
  
  // Get the current displayed value as a string
  char displayBuffer[10];
  formatDivergenceValue(currentDivergence, displayBuffer);
  
  // Create animated display line
  char newLine1[17] = "                ";
  int startPos = 4;  // Starting position for centering
  
  // Calculate animation progress
  unsigned long elapsed = currentTime - lastStateChangeTime;
  float progress = static_cast<float>(elapsed) / ANIMATION_DURATION;
  
  // For smoother animation, slowly settle digits from left to right
  for (int i = 0; i < strlen(displayBuffer); i++) {
    if (displayBuffer[i] >= '0' && displayBuffer[i] <= '9') {
      float digitProgress = progress * 2.0 - (0.15 * i);
      
      if (digitProgress > 0.9 || random(100) < (digitProgress * 100)) {
        newLine1[startPos + i] = divergenceBuffer[i];
      } else {
        newLine1[startPos + i] = '0' + random(10);
      }
    } else {
      newLine1[startPos + i] = displayBuffer[i];
    }
  }
  
  // Update only the bottom line for animation
  updateDisplayLine(1, newLine1);
}