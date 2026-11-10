# Dual-Axis Solar Tracker: Direct-Drive Kinematics & Control

## Abstract
This project is a functional Phase 1 prototype of a dual-axis solar tracker utilizing a closed-loop Bang-Bang control architecture. Designed to handle a 2.1kg asymmetric payload, the system abandons traditional continuous PID control in favor of decoupled deadbands and sequential motor execution, optimizing both mechanical stability and electrical power budgets.

## Hardware Stack
* **Microcontroller:** Arduino Nano
* **Motor Control:** CNC Shield V4 + 2x A4988 Stepper Drivers
* **Actuators:** 2x NEMA 17 Stepper Motors (Direct-Drive)
* **Sensors (Optical):** 4x Light Dependent Resistors (LDRs) with analog smoothing
* **Sensors (Positional):** 2x AS5600 Magnetic Encoders (True 12-bit I2C)
* **Thermals & Power:** 12V 2.5A DC Supply with a 2.5-inch 12VDC active cooling fan

## Component Faults & Engineering Troubleshooting Log

### 1. Sensor Resolution Fault: AS5600 Analog Limitation
* **Symptom:** The tilt axis repeatedly crashed into its physical metal safety strip without triggering the firmware stopping logic.
* **Diagnosis:** The AS5600 encoders were initially read via the Nano's analog pins. Arduino's 10-bit ADC caps at 1023, meaning the code could never evaluate mechanical boundaries set at 1400.
* **Resolution:** Abandoned analog PWM reads. Rewrote the sensor architecture to utilize the AS5600's native I2C registers, deploying the standard `Wire` library for the Pan axis and `SoftwareWire` for the Tilt axis to prevent address conflicts, unlocking the full 4095 resolution.

### 2. Power Supply Brownouts (Overcurrent Protection Trips)
* **Symptom:** The 12V 2.5A power supply LED flickered, and the Arduino reset during active tracking.
* **Diagnosis:** The A4988 VREF was tuned for maximum holding torque to support the cantilevered 2.1kg load, pulling a continuous ~0.5A per motor. When both motors attempted to accelerate simultaneously, the combined startup current spike exceeded the 2.5A limit, triggering the OCP.
* **Resolution:** Engineered a Strict Sequential Execution loop. The code now forces the Pan axis to wait until the Tilt motor has completely traversed its `distanceToGo() == 0`, ensuring only one motor accelerates at a time.

### 3. Kinematic "Hunting" (Mechanical Oscillation)
* **Symptom:** When the light source was centered, the Tilt axis bounced endlessly back and forth over the center point.
* **Diagnosis:** The heavy payload required a 10-step acceleration burst just to overcome static friction. However, a tight deadband (10 units) meant those 10 steps physically carried the sensor completely past the light threshold, triggering an immediate reverse command.
* **Resolution:** Decoupled the mechanical sensitivities. Increased the `TILT_DEADBAND` to 40 to create a wider center target, and reduced the momentum burst to 5 steps. The flat, stable Pan axis retained its highly accurate 10-unit deadband.

### 4. Thermal Throttling on A4988 Drivers
* **Symptom:** Driver ICs became dangerously hot to the touch during testing.
* **Diagnosis:** To prevent the 2.1kg payload from back-driving the stepper motors due to gravity, the `EN_PIN` is permanently held `LOW`, supplying 100% holding current 24/7.
* **Resolution:** Integrated a 2.5-inch (0.3A) 12V DC axial fan directly into the CNC shield's VIN/GND header, providing continuous, targeted cross-breeze over the aluminum heatsinks without drawing from the Nano's fragile 5V regulator.

### 5. CNC Shield V4 (HW-702) Clone Manufacturing Flaws
* **Symptom:** Motors aggressively vibrated without rotating, and installing microstepping jumpers had zero effect on motor resolution.
* **Diagnosis:** Reverse-engineered the PCB traces and discovered two documented manufacturing flaws common in generic clone boards: the `STEP` and `DIR` pins were physically routed backwards to the A4988 sockets, and the microstepping pins were improperly tied to ground rather than VCC.
* **Resolution:** Rather than physically cutting traces, the pin swap was bypassed in the Arduino firmware (`#define X_DIR 2`, `#define X_STEP 5`). Due to the dead microstepping traces, the system was mechanically tuned to operate smoothly on full steps using the customized Bang-Bang deadband control.

### 6. AS5600 Pin Constraints, SMD Configuration & Magnetic Flux Limits
* **Symptom:** Initial attempts to read the two AS5600 sensors via analog pins yielded highly erratic angle data, and the sensors randomly locked up into programming mode.
* **Diagnosis:** 
  1. **Hardware Bus Limitation:** The Arduino Nano possesses only one hardware I2C bus. Because the AS5600 IC has a fixed I2C address (0x36), two sensors cannot share the same hardware bus without an external I2C multiplexer. Analog mode was initially selected to bypass this address conflict.
  2. **Configuration Fault:** To utilize analog mode, the GIO pin cannot be pulled to ground. Measuring the breakout board revealed a 1kΩ resistance between GIO and GND, confirming the factory-installed R4 SMD resistor was erroneously forcing the chip into an analog programming state.
  3. **Magnetic Flux Fault:** After modifying the board for analog output, the generic diametric magnets provided with the sensors lacked the required magnetic field strength (flux) to generate a stable, linear analog voltage.
* **Resolution:** Physically desoldered and removed the R4 resistor from the PCB to disable the programming lock. *(Note: Removing R4 is strictly required only if attempting to use analog/PWM modes; developers utilizing I2C from the start can leave R4 intact).* Because the weak magnets made analog mode unusable, the sensor architecture was pivoted entirely back to digital I2C. To bypass the fixed address conflict on the Nano, the `SoftwareWire` library was deployed to instantiate a second, independent software I2C bus on standard digital pins. This solved the hardware limitation and provided perfectly stable 12-bit angular data despite the low magnetic flux.
