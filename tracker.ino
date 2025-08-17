/*
  Arduino sketch for Raspberry Pi Pico (RP2040 Arduino core).
  - Reads BNO085 (I2C)
  - Uses built-in sensor fusion from BNO085
  - Outputs orientation (Yaw, Pitch, Roll) via Serial
  - Converts head movement to USB HID Joystick

  Libraries required (install via Library Manager):
   - Adafruit BNO08x (https://github.com/adafruit/Adafruit_BNO08x)
   - Adafruit TinyUSB Library (for HID Joystick support)

  Wiring (I2C):
   BNO085 VIN -> 3V3
   BNO085 GND -> GND
   BNO085 SDA -> GP4 (SDA)
   BNO085 SCL -> GP5 (SCL)

  Usage:
  - Yaw (left/right head turn) -> X-axis
  - Pitch (up/down head tilt) -> Y-axis
  - Roll (left/right head lean) -> Z-axis (or can be mapped to buttons)

  Author: KenshiHH
  License: MIT-style (adapt freely)
*/

#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <Adafruit_TinyUSB.h>

#define SAMPLE_FREQ 200.0f // Hz (update rate)

Adafruit_BNO08x bno = Adafruit_BNO08x();

sh2_SensorValue_t sensorValue;

// USB HID Joystick
Adafruit_USBD_HID usb_hid;
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_GAMEPAD()
};

bool report_values = false;

// Joystick state
hid_gamepad_report_t gamepad_report;

// Reset view offsets
float yaw_offset = 0.0f;
float pitch_offset = 0.0f;
float roll_offset = 0.0f;

void setup() {
  Serial.begin(115200);
  //while (!Serial) ;
  Serial.println("RP2040 BNO085 Headtracker with Joystick starting...");

  // Initialize USB HID
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();

  // Wait for USB HID to be ready
  while (!usb_hid.ready()) {
    delay(50);
  }

  Wire.begin();
  if (!bno.begin_I2C(0x4B)) {
    Serial.println("Failed to find BNO085 chip. Check wiring.");
    while (1) delay(10);
  }
  else{
    Serial.print("wire working");
  }

  if (!bno.enableReport(SH2_ROTATION_VECTOR)) {
    Serial.println("Could not enable rotation vector!");
    while (1) delay(10);
  }

  // Initialize gamepad report
  memset(&gamepad_report, 0, sizeof(gamepad_report));
  gamepad_report.x = 0;
  gamepad_report.y = 0;
  gamepad_report.z = 0;

  Serial.println("Yaw -> X-axis, Pitch -> Y-axis, Roll -> Z-axis");
  delay(100);
  initialCenter();
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');  // Read input until Enter key
    command.trim(); // Remove whitespace/newlines

    if (command == "reset" || command == "resetview") {
      resetView();
    } 
  }

  if (bno.getSensorEvent(&sensorValue)) {
    if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
      float qw = sensorValue.un.rotationVector.real;
      float qx = sensorValue.un.rotationVector.i;
      float qy = sensorValue.un.rotationVector.j;
      float qz = sensorValue.un.rotationVector.k;

        // Normalize quaternion (good practice)
        float norm = sqrtf(qw*qw + qx*qx + qy*qy + qz*qz);
        qw /= norm;
        qx /= norm;
        qy /= norm;
        qz /= norm;
        
        // Convert quaternion to Euler angles (ZYX convention)
        // Roll (x-axis rotation)
        float sinr_cosp = 2.0f * (qw * qx + qy * qz);
        float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
        float roll = atan2f(sinr_cosp, cosr_cosp);
        
        // Pitch (y-axis rotation)
        float sinp = 2.0f * (qw * qy - qz * qx);
        float pitch;
        if (fabsf(sinp) >= 1.0f)
            pitch = copysignf(PI / 2, sinp); // Use 90 degrees if out of range
        else
            pitch = asinf(sinp);
        
        // Yaw (z-axis rotation)
        float siny_cosp = 2.0f * (qw * qz + qx * qy);
        float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
        float yaw = atan2f(siny_cosp, cosy_cosp);
        
        // Convert to degrees
        roll *= 180.0f / PI;
        pitch *= 180.0f / PI;
        yaw *= 180.0f / PI;

        // Apply offsets to zero the view
        yaw -= yaw_offset;
        pitch -= pitch_offset;
        roll -= roll_offset;

        // Normalize angles to -180 to 180 range
        while (yaw > 180.0f) yaw -= 360.0f;
        while (yaw < -180.0f) yaw += 360.0f;
        while (pitch > 180.0f) pitch -= 360.0f;
        while (pitch < -180.0f) pitch += 360.0f;
        while (roll > 180.0f) roll -= 360.0f;
        while (roll < -180.0f) roll += 360.0f;

      // Convert to joystick values (-127 to 127)
      float joy_x = mapFloat(yaw, -180.0, 180.0, -127.0, 127.0);      // Yaw -> X-axis
      float joy_y = mapFloat(pitch, -180.0, 180.0, -127.0, 127.0);   // Pitch -> Y-axis (inverted)
      float joy_z = mapFloat(roll, -180.0, 180.0, -127.0, 127.0);     // Roll -> Z-axis

      // Update gamepad report
      gamepad_report.x = joy_x;
      gamepad_report.y = joy_y;
      gamepad_report.z = joy_z;
      gamepad_report.buttons = 0;

      // Send joystick report
      if (usb_hid.ready()) {
        usb_hid.sendReport(0, &gamepad_report, sizeof(gamepad_report));
      }

      // Serial output for debugging
      if (report_values){
        static uint32_t lastPrint = 0;
        if (millis() - lastPrint > 100) {
          lastPrint = millis();
          Serial.print("Yaw: "); Serial.print(yaw, 1);
          Serial.print(" Pitch: "); Serial.print(pitch, 1);
          Serial.print(" Roll: "); Serial.print(roll, 1);
          Serial.print(" | Joy X: "); Serial.print(joy_x);
          Serial.print(" Y: "); Serial.print(joy_y);
          Serial.print(" Z: "); Serial.println(joy_z);
          Serial.print(" Accuracy: ");
          switch (sensorValue.status) {
            case 0:
              Serial.println("Unreliable");
              break;
            case 1:
              Serial.println("Low accuracy");
              break;
            case 2:
              Serial.println("Medium accuracy");
              break;
            case 3:
              Serial.println("High accuracy");
              break;
            default:
              Serial.println("Unknown");
              break;
          }
        }
      }
    }
  }

  delay(1000 / SAMPLE_FREQ);
}

// converts the min/max of the tracker values to the min/max of the joystick
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void initialCenter() {
  if (bno.getSensorEvent(&sensorValue)) {
    if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
      float qw = sensorValue.un.rotationVector.real;
      float qx = sensorValue.un.rotationVector.i;
      float qy = sensorValue.un.rotationVector.j;
      float qz = sensorValue.un.rotationVector.k;

        // Normalize quaternion (good practice)
        float norm = sqrtf(qw*qw + qx*qx + qy*qy + qz*qz);
        qw /= norm;
        qx /= norm;
        qy /= norm;
        qz /= norm;
        
        // Convert quaternion to Euler angles (ZYX convention)
        // Roll (x-axis rotation)
        float sinr_cosp = 2.0f * (qw * qx + qy * qz);
        float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
        float roll = atan2f(sinr_cosp, cosr_cosp);
        
        // Pitch (y-axis rotation)
        float sinp = 2.0f * (qw * qy - qz * qx);
        float pitch;
        if (fabsf(sinp) >= 1.0f)
            pitch = copysignf(PI / 2, sinp); // Use 90 degrees if out of range
        else
            pitch = asinf(sinp);
        
        // Yaw (z-axis rotation)
        float siny_cosp = 2.0f * (qw * qz + qx * qy);
        float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
        float yaw = atan2f(siny_cosp, cosy_cosp);
        
        // Convert to degrees
        roll *= 180.0f / PI;
        pitch *= 180.0f / PI;
        yaw *= 180.0f / PI;
    }
  }
  resetView();
}

void resetView() {
  delay(100);
  Serial.println("Resetting view to center... in 3 seconds, keep head centered!");
  delay(3000);
  
  // Take several readings and average them for the reset position
  float yaw_sum = 0, pitch_sum = 0, roll_sum = 0;
  int samples = 10;
  
  for (int i = 0; i < samples; i++) {
    if (bno.getSensorEvent(&sensorValue)) {
      if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
        float qw = sensorValue.un.rotationVector.real;
        float qx = sensorValue.un.rotationVector.i;
        float qy = sensorValue.un.rotationVector.j;
        float qz = sensorValue.un.rotationVector.k;

        // Normalize quaternion (good practice)
        float norm = sqrtf(qw*qw + qx*qx + qy*qy + qz*qz);
        qw /= norm;
        qx /= norm;
        qy /= norm;
        qz /= norm;
        
        // Convert quaternion to Euler angles (ZYX convention)
        // Roll (x-axis rotation)
        float sinr_cosp = 2.0f * (qw * qx + qy * qz);
        float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
        float roll = atan2f(sinr_cosp, cosr_cosp);
        
        // Pitch (y-axis rotation)
        float sinp = 2.0f * (qw * qy - qz * qx);
        float pitch;
        if (fabsf(sinp) >= 1.0f)
            pitch = copysignf(PI / 2, sinp); // Use 90 degrees if out of range
        else
            pitch = asinf(sinp);
        
        // Yaw (z-axis rotation)
        float siny_cosp = 2.0f * (qw * qz + qx * qy);
        float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
        float yaw = atan2f(siny_cosp, cosy_cosp);
        
        // Convert to degrees and accumulate
        yaw_sum += yaw * 180.0f / PI;
        pitch_sum += pitch * 180.0f / PI;
        roll_sum += roll * 180.0f / PI;
      }
    }
    delay(10);
  }
  
  // Set the current position as the new center (zero point)
  yaw_offset = yaw_sum / samples;
  pitch_offset = pitch_sum / samples;
  roll_offset = roll_sum / samples;
  
  Serial.println("View reset complete!");
}
