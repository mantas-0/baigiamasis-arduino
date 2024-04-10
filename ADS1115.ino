#include "ADS1115.h"
#include "string.h"

#define MAX_ARGUMENTS 6 //We should not need more than 6 arguments (for commands over Serial)

// Button (used to request voltage) pin
#define buttonPin 12

///// MOTOR
#define stepPin 7
#define dirPin 5

#define motorSpeed 500 //Lower - faster. Indicates the duration of break between pulses in micro-seconds

///// VOLTMETER SETUP
ADS1115 adc0(ADS1115_DEFAULT_ADDRESS);

///// RUNTIME VARIABLES
int buttonState = 0;

char receivedData[64]; // Used to store data received via Serial port. Adjust the size as needed

void setup() {    
  Wire.begin();
  Serial.begin(115200); // initialize serial communication 
  
  Serial.println("Testing device connections...");
  Serial.println(adc0.testConnection() ? "ADS1115 connection successful" : "ADS1115 connection failed");
  
  adc0.initialize(); // initialize ADS1115 16 bit A/D chip

  adc0.setMode(ADS1115_MODE_CONTINUOUS);
    
  // Set the gain (PGA) +/- 6.144v
  // Note that any analog input must be higher than –0.3V and less than VDD +0.3
  adc0.setGain(ADS1115_PGA_6P144);

  // Initialize the button pin as an input:
  pinMode(buttonPin, INPUT);

  // Initialize the motor pins as outputs:
  pinMode(stepPin,OUTPUT); 
  pinMode(dirPin,OUTPUT);
}

// Function to check if a string represents a valid integer
bool isNumber(char* str) {
  if (*str == '\0') { // Ensure the string is not empty
    return false;
  }
    
  if (*str == '-') { // Allow negative numbers
    ++str; // Skip the minus sign
    if (*str == '\0') { // Ensure the string is not just a "-"
      return false;
    }
  }

  while (*str != '\0') {
    if (isspace(*str)) { // Skip whitespace characters
      ++str;
      continue;
    }

    if (!isdigit(*str)) { // Check if the current character is not a digit
      return false; // If not, it's not a number
    }
    ++str; // Move to the next character
  }
  return true; // The string is a valid number
}

// Function to convert a string to an integer if it is a valid number
int convertToInt(char* str) {
  if (isNumber(str)) {
    return atoi(str); // Use atoi to convert string to integer
  } else {
    return 0; // Return 0 or some error value if not a number
  }
}

void readVoltage(int index, int readCount = 1) {
  float voltageSum = 0;
  if(index == 1){
    // Sensor is on P0/N1 (pins 4/5)
    adc0.setMultiplexer(ADS1115_MUX_P0_N1);
    adc0.setGain(ADS1115_PGA_6P144);
  } else if(index == 2){
    // 2nd sensor is on P2/N3 (pins 6/7)
    adc0.setMultiplexer(ADS1115_MUX_P2_N3);
    adc0.setGain(ADS1115_PGA_0P256); // +/- 0.256 V
  } else {
    Serial.println("Success: 0");
    Serial.print("Data: Invalid index - ");
    Serial.println(index);
    return;
  }


  for(int i = 0; i < readCount; i++){
    voltageSum += adc0.getMilliVolts();
    delay(10);
  }


  float voltage = voltageSum / readCount;


  Serial.println("Success: 1");
  Serial.print("Data: ");
  Serial.println(voltage);
}


void spinMotor(int pulses) {
  bool dirPinOutput = HIGH;
  if(pulses < 0){
    dirPinOutput = LOW;
    pulses = abs(pulses);
  }

  digitalWrite(dirPin, dirPinOutput);

  for(int x = 0; x < pulses; x++) {
    digitalWrite(stepPin,HIGH); 
    delayMicroseconds(motorSpeed);  // by changing this time delay between the steps we can change the rotation speed
    digitalWrite(stepPin,LOW); 
    delayMicroseconds(motorSpeed); 
  }

  delay(100);
}


void readSerialCommand(){    
  // Index to keep track of the number of received bytes:
  int index = 0;  
    
  // Read all available bytes:
  while (Serial.available() > 0 && index < sizeof(receivedData) - 1) {
    // Read one byte:
    char receivedByte = Serial.read();
    
    // Store the byte in the buffer:
    receivedData[index] = receivedByte;
    index++;
      
    // Simple delay to allow bytes to accumulate in the buffer:
    delay(10);
  }
    
  // Null-terminate the string:
  receivedData[index] = '\0';
}

// Function to split a char array into an array of words.
// Parameters:
//   input: The input char array to split.
//   words: An array of char pointers, each will point to a word in the input.
//   maxWords: The maximum number of words that the words array can hold.
//   wordCount: A reference to an integer where the function will store the actual number of words found.
void splitWords(char* input, char** words, int maxWords, int& wordCount) {
  wordCount = 0; // Initialize word count
  bool inWord = false; // Flag to keep track whether we are in a word or not

  for (; *input != '\0' && wordCount < maxWords; input++) { // Iterate through the char array
    if (*input != ' ' && !inWord) { // If the current char is not a space and we are not in a word
      inWord = true; // We are now in a word
      words[wordCount] = input; // Point the next word pointer to the current char
    } else if (*input == ' ') { // If the current char is a space
      *input = '\0'; // Null-terminate the current word if we are in a word
      if (inWord) {
        wordCount++; // Increment word count when ending a word
        inWord = false; // We are no longer in a word
      }
    }
  }

  if (inWord) { // If the input string ends with a word (not a space)
    wordCount++; // Ensure to count the last word
  }
}

void handleUnknownCommand() {
  Serial.println("Success: 0");
  Serial.print("Data: Unknown Command - ");
  Serial.println(receivedData);
}

void handleVoltageCommand(char** arguments){
  //Select proper voltage sub-command
  if(strcmp(arguments[1], "get") == 0) {
    int index = convertToInt(arguments[2]);
    readVoltage(index, 50);
  } else {
    handleUnknownCommand();
  }
}

void handleMotorCommand(char** arguments){
  //Select proper motor sub-command
  if(strcmp(arguments[1], "move") == 0) {
    int pulses = convertToInt(arguments[2]);
    spinMotor(pulses);
  } else {
    handleUnknownCommand();
  }

  Serial.println("Success: 1");
  Serial.println("Data: None");
}

void loop() {
  if(Serial.available() > 0){
    readSerialCommand();

    char* arguments[MAX_ARGUMENTS];
    int numberOfArguments = 0;

    splitWords(receivedData, arguments, MAX_ARGUMENTS, numberOfArguments);

    if(numberOfArguments > 0){
      // First argument is the name of the command.
      // The remaining arguments will be passed to the function of that command
      if(strcmp(arguments[0], "voltage") == 0){
        handleVoltageCommand(arguments);
      } else if(strcmp(arguments[0], "motor") == 0){
        handleMotorCommand(arguments);
      } else {
        Serial.println(arguments[0]);
        handleUnknownCommand();
      }
    }
  }

  int currentButtonState = digitalRead(buttonPin);
  if(currentButtonState == 1 && buttonState == 0){
    buttonState = 1;
    readVoltage(1);
    readVoltage(2);
  } else if (currentButtonState == 0 && buttonState == 1) {
    buttonState = 0;
  }
}