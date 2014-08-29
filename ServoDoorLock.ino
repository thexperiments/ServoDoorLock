//Servo door lock control software

#include <Servo.h>
#include <EEPROM.h>
#include <Time.h> 

Servo door_servo;
const int us_max = 2000;
const int us_min = 1000;
const int pin_button_ccw = 11;
const int pin_button_cw = 10;
const int pin_learning = 6;
const int pin_learning_gnd = 5;
const int pin_button_open = 7;
const int pin_led_info = 13;
const int pin_servo = 9;
const int step_size_deg = 90;
const int deg_max = 5 * 360;
const int us_per_10deg = ((us_max - us_min)*10) / deg_max;
const int initial_us = 1500; //for now in the middle, its a 5 turn servo so we have 2,5 turns each direction left
int current_us = 1500;

//eeprom addresses as int two byte steps because we store 16bit (2byte) integers
const int mem_addr_idle_int = 0;
const int mem_addr_opened_int = 2; 
const int mem_addr_locked_int = 4;
const int mem_addr_unlocked_int = 6;

//target values for movement
int target_us_idle = 0;
int target_us_opened = 0;
int target_us_locked = 0;
int target_us_unlocked = 0;

//learning states
const int learning_locked = 1;
const int learning_unlocked = 2;
const int learning_opened = 3;
int learning_state = 0;

//function prototypes
boolean learningActive();
void infoFlash(int time_ms, int repeat_count);
void EEPROMWriteInt(int p_address, int p_value);
unsigned int EEPROMReadInt(int p_address);

//time for auto reset to unlocked
time_t trigger_time = 0;
int wait_time = 0;

void setup() 
{ 
  door_servo.attach(pin_servo);  // attach the servo to the servo object
  pinMode(pin_button_cw,INPUT_PULLUP); //input for clockwise button (close)
  pinMode(pin_button_ccw,INPUT_PULLUP); //input for counterclockwise button (open)
  pinMode(pin_button_open, INPUT_PULLUP); //input for the save button for learning
  pinMode(pin_learning, INPUT_PULLUP); //input for the learning mode jumper
  pinMode(pin_led_info, OUTPUT); //led for infos
  pinMode(pin_learning_gnd, OUTPUT); //virtual gnd for learn jumper
  
  //make pin_learning_gnd low to work as virtual gnd for learning jumper
  digitalWrite(pin_learning_gnd, LOW);
  
  if (learningActive())
  {
    //we are starting up in learning mode, reset the eeprom
    EEPROMWriteInt(mem_addr_idle_int, 0);
    EEPROMWriteInt(mem_addr_opened_int, 0);
    EEPROMWriteInt(mem_addr_locked_int, 0);
    EEPROMWriteInt(mem_addr_unlocked_int, 0);
    //go to initial position
    current_us = initial_us;
    door_servo.writeMicroseconds(current_us);
    //signal learning with slow blinking
    infoFlash(500,3);
  }
  else
  {
    //read config from eeprom
    target_us_idle = EEPROMReadInt(mem_addr_idle_int);
    target_us_opened = EEPROMReadInt(mem_addr_opened_int);
    target_us_locked = EEPROMReadInt(mem_addr_locked_int);
    target_us_unlocked = EEPROMReadInt(mem_addr_unlocked_int);
    //and go to locked position, might have been power outtage and locked is the secure state
    current_us = target_us_locked;
    door_servo.writeMicroseconds(current_us);
    //signal normal mode with fast blinking
    infoFlash(100,5);
  }
} 
 
 
void loop() 
{
  int button_cw_state = digitalRead(pin_button_cw); //read the state of the cw button
  int button_ccw_state = digitalRead(pin_button_ccw); //read the state of the ccw button
  int button_open_state = digitalRead(pin_button_open); //read the state of the open button
  
  boolean value_changed = false;
  
  //check if we need to return from opened to unlocked
  if ((trigger_time != 0) && (now() - trigger_time > wait_time))
  {
    infoFlash(50,10);
    current_us = target_us_unlocked;
    value_changed = true;
    trigger_time = 0;
  }
  
  if (button_cw_state == LOW)
  {
    infoFlash(100,1);
    if(learningActive())
    {
      //when learning move in 10 deg steps
      current_us += (step_size_deg * us_per_10deg)/10;
    }
    else
    {
      //move to unlocked position
      current_us = target_us_unlocked;
    }
    value_changed = true;
  }
  else if (button_ccw_state == LOW)
  {
    infoFlash(100,1);
    if(learningActive())
    {
      //when learning move in 10 deg steps
      current_us -= (step_size_deg * us_per_10deg)/10;
    }
    else
    {
      //move to locked position
      current_us = target_us_locked;
    }
    value_changed = true;
  }
  else if (button_open_state == LOW)
  {
    infoFlash(100,1);
    if(learningActive())
    {
      //when learning save the current position for the according state
      delay(200); //short delay to make the blink-code more visible
      if(learning_state < learning_opened)
      {
        //increase learning state
        learning_state++;
        
        switch (learning_state) {
          case learning_locked:
            //blink one time
            infoFlash(500,1);
            //save current us to variable
            target_us_locked = current_us;
            break;
          case learning_unlocked:
            //blink two times
            infoFlash(500,2);
            //save current us to variable
            target_us_unlocked = current_us;
            break;
          case learning_opened:
            //blink three times
            infoFlash(500,3);
            //save current us to variable
            target_us_opened = current_us;
            
            //save all the variables in eeprom
            EEPROMWriteInt(mem_addr_opened_int, target_us_opened);
            EEPROMWriteInt(mem_addr_locked_int, target_us_locked);
            EEPROMWriteInt(mem_addr_unlocked_int, target_us_unlocked);
            break;
        }
      }
    }
    else
    {
      
      if (current_us == target_us_locked)
      {
        //previously locked, open
        //we need to return to unlocked after some time so we save the current time
        trigger_time = now();
        wait_time = 5; //5 seconds
        //finally move to opened position
        current_us = target_us_opened;   
      }
      else
      {
        //door is currently open, lock it
        //move to locked position
        current_us = target_us_lockeddqx; 
      }
      
        
    }
    value_changed = true;
  }
  
  //keep servo in the right ranges
  if (current_us >= us_max) 
  {
    current_us = us_max;
    //turn on led to show that we have reached the end
    digitalWrite(pin_led_info, HIGH);
  }
  else if (current_us <= us_min)
  {
    current_us = us_min;
    //turn on led to show that we have reached the end
    digitalWrite(pin_led_info, HIGH);
  }
  else
  {
    //turn of led
    digitalWrite(pin_led_info, LOW);
  }
  
  if (value_changed){
    //write to servo
    door_servo.writeMicroseconds(current_us);
    value_changed = false;
  }
  else
  {
    //write to servo
    door_servo.writeMicroseconds(0);
  }
  delay(50);
}

//checker for learning mode
boolean learningActive(){
  //return inverted pin readout as we use pullup
  return (!digitalRead(pin_learning));
}

//helper for flashing th light
void infoFlash(int time_ms, int repeat_count){
  boolean state_before = digitalRead(pin_led_info);
  for (int i = 0; i < repeat_count; i++)
  {
    digitalWrite(pin_led_info, LOW);
    delay(time_ms/2);
    digitalWrite(pin_led_info, HIGH);
    delay(time_ms);
    digitalWrite(pin_led_info, LOW);
    delay(time_ms/2);
  }
  digitalWrite(pin_led_info, state_before);
}

//EEPROM helpers from http://forum.arduino.cc/index.php/topic,37470.0.html

//This function will write a 2 byte integer to the eeprom at the specified address and address + 1
void EEPROMWriteInt(int p_address, int p_value)
{
  byte lowByte = ((p_value >> 0) & 0xFF);
  byte highByte = ((p_value >> 8) & 0xFF);

  EEPROM.write(p_address, lowByte);
  EEPROM.write(p_address + 1, highByte);
}

//This function will read a 2 byte integer from the eeprom at the specified address and address + 1
unsigned int EEPROMReadInt(int p_address)
{
  byte lowByte = EEPROM.read(p_address);
  byte highByte = EEPROM.read(p_address + 1);

  return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}
