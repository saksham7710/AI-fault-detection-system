# ⚡ AI Fault Detection System for DC Motors

> **An intelligent, multi-layered predictive maintenance system for DC motor health monitoring — combining edge-computing hardware, real-time telemetry, and unsupervised anomaly detection.**

---

## 📋 Table of Contents
- [Project Overview](#project-overview)
- [System Architecture](#system-architecture)
- [Hardware Foundation](#hardware-foundation)
- [Reactive Safety Logic (Arduino)](#reactive-safety-logic-arduino)
- [Hardware Failure & Diagnostic Post-Mortem](#hardware-failure--diagnostic-post-mortem)
- [Pivot to Predictive AI (Isolation Forest)](#pivot-to-predictive-ai-isolation-forest)
- [Tech Stack](#tech-stack)
- [Future Roadmap](#future-roadmap)
- [Lessons Learned](#lessons-learned)
- [Author](#author)

---

## 🎯 Project Overview

This project implements a **two-tier fault detection pipeline** for DC motor monitoring:

1. **Edge Layer (Reactive):** An Arduino-based hardware node with physical sensors acts as an immediate, hard-fail safety net. It cuts power when thresholds are breached.
2. **Cloud/Analytics Layer (Predictive):** An Isolation Forest ML model analyzes streaming telemetry to detect **creeping degradation** (e.g., slow temperature rise, current drift) *before* catastrophic thresholds are reached.

The system bridges **embedded systems**, **IoT telemetry**, and **machine learning** into a cohesive predictive maintenance solution.

---

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    PHYSICAL HARDWARE LAYER                   │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────┐  │
│  │  DC Motor   │◄──►│   L298N     │◄──►│     Arduino     │  │
│  │   (12V)     │    │Motor Driver │    │  (ATmega328P)   │  │
│  └─────────────┘    └─────────────┘    └─────────────────┘  │
│         ▲                                      ▲             │
│         │           ┌─────────────┐            │             │
│         └───────────┤  Sensors    ├────────────┘             │
│                     │• ACS712     │  Current                 │
│                     │• Thermistor │  Temperature             │
│                     │• SW-420    │  Vibration               │
│                     │• Potentiometer (Speed Control)        │
│                     └─────────────┘                          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼ Serial (pyserial)
┌─────────────────────────────────────────────────────────────┐
│                  TELEMETRY & AI LAYER                        │
│  ┌─────────────────┐      ┌─────────────────────────────┐  │
│  │  Python Script  │─────►│  Isolation Forest Model     │  │
│  │  (CSV Streamer) │      │  (Anomaly Detection)        │  │
│  └─────────────────┘      └─────────────────────────────┘  │
│                                    │                         │
│                                    ▼                         │
│                           ┌─────────────────┐                │
│                           │  Alert / Override│               │
│                           │  Command to Edge │               │
│                           └─────────────────┘                │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔧 Hardware Foundation

### Components

| Component | Role | Specification |
|-----------|------|---------------|
| **Microcontroller** | Logic Controller | Arduino Uno (ATmega328P) |
| **Motor Driver** | High-Power Control | L298N Dual H-Bridge |
| **Current Sensor** | Real-time current monitoring | ACS712 Hall-Effect Sensor |
| **Temperature Sensor** | Thermal monitoring | Analog Thermistor |
| **Vibration Sensor** | Mechanical fault detection | SW-420 Digital Vibration Module |
| **User Input** | Manual speed control | 50kΩ Potentiometer |
| **Power Supply** | Motor power | 12V DC |
| **Indicator** | Fault warning | LED + Digital Output |

### Wiring Notes
- **CRITICAL:** Common Ground (GND) between Arduino and L298N **must be established BEFORE** applying 12V motor power.
- Arduino logic pins operate at **5V tolerance** — any reverse voltage surge will destroy the ATmega328P silicon gates.
- L298N control pins accept 5V logic signals from Arduino to switch the 12V motor supply.

---

## ⚡ Reactive Safety Logic (Arduino)

The Arduino firmware implements **hard-coded safety thresholds** as an immediate fail-safe:

| Parameter | Threshold | Action |
|-----------|-----------|--------|
| Temperature | ≥ 55°C | **EMERGENCY STOP** |
| Current Draw | ≥ 1.0 Amps | **EMERGENCY STOP** |
| Vibration | ≥ 7 triggers/cycle | **EMERGENCY STOP** |

### Behavior
- If **any** threshold is breached → Motor power is **instantly cut** + Warning LED illuminates.
- **Auto-recovery:** System resets **1500ms** after the environment normalizes.
- This layer ensures **physical safety** even if the AI layer is offline.

```cpp
// Pseudocode of the reactive logic
if (temperature >= 55 || current >= 1.0 || vibrationCount >= 7) {
    digitalWrite(MOTOR_PIN, LOW);   // Cut power
    digitalWrite(LED_PIN, HIGH);    // Warning indicator
    delay(1500);                     // Recovery window
    resetSystem();
}
```

---

## 🔥 Hardware Failure & Diagnostic Post-Mortem

### The Incident
During integration of the 5V Arduino logic board with the 12V motor driver, the Arduino suffered a **catastrophic failure** — multiple analog pins (A0–A5) and digital pins (D9–D13) were permanently damaged.

### Diagnostic Process

| Step | Test | Result |
|------|------|--------|
| 1 | **Hardware Bypass:** Hardwired L298N control pins directly to 5V source | ✅ Motor spun flawlessly — high-power components operational |
| 2 | **LED Probe:** Manual pin-by-pin test of Arduino I/O | ❌ Analog pins A0–A5 and digital pins D9–D13 dead |

### Root Cause Analysis
**Ground Loop Discrepancy**

> When the 12V main power was applied, the **shared Ground (GND) wire** between the L298N and Arduino was **not yet established**. The 12V return current sought an alternate path to ground — flowing backward through the Arduino's control wires. This created a **reverse-voltage surge** that exceeded the ATmega328P's 5V-tolerant logic gates, destroying the silicon.

```
INCORRECT SEQUENCE (What Happened):
    1. Power ON 12V supply
    2. Current seeks return path → flows through Arduino control pins
    3. 12V surge hits 5V-tolerant pins → PERMANENT DAMAGE

CORRECT SEQUENCE (SOP Established):
    1. Connect GND between Arduino and L298N FIRST
    2. Verify common ground continuity
    3. THEN apply 12V motor power
```

### Corrective Action
A **Standard Operating Procedure (SOP)** was established for all future hardware builds:

> **"The common ground wire must be securely connected BEFORE any high-voltage power is introduced to the system."**

---

## 🤖 Pivot to Predictive AI (Isolation Forest)

With the hardware offline, the project evolved from **reactive** to **predictive** fault detection using machine learning.

### Strategy
Instead of relying solely on hard-coded thresholds, the system now streams sensor data via serial communication to a Python-based **Isolation Forest** model.

### Why Isolation Forest?
- **Unsupervised Learning:** No need for pre-labeled "fault" data.
- **Anomaly Detection:** Learns the "normal" operational profile of the motor.
- **Proactive:** Flags deviations (creeping degradation) *before* thresholds are breached.

### Implementation (Google Colab)
The model was developed and tested in Google Colab with simulated data:
- **Normal Operations:** Baseline current, temperature, and vibration profiles.
- **Degradation Scenarios:** Slow, steady rise in heat and current draw.
- **Hard Faults:** Sudden spikes mimicking real failure modes.

```python
# Core concept: Isolation Forest for anomaly detection
from sklearn.ensemble import IsolationForest

# Train on "normal" motor behavior
model = IsolationForest(contamination=0.1, random_state=42)
model.fit(normal_sensor_data)

# Predict on real-time stream
anomaly_scores = model.predict(real_time_data)
# -1 = Anomaly detected | 1 = Normal operation
```

### Key Advantage
| Reactive (Arduino) | Predictive (AI) |
|-------------------|-----------------|
| Acts **after** threshold breach | Acts **before** threshold breach |
| Hard stop = downtime | Early warning = scheduled maintenance |
| Cannot detect gradual wear | Detects creeping degradation patterns |

---

## 🛠️ Tech Stack

| Layer | Technology |
|-------|------------|
| **Embedded** | C++ (Arduino IDE), ATmega328P |
| **Sensors** | ACS712, Thermistor, SW-420, Potentiometer |
| **Motor Control** | L298N Dual H-Bridge |
| **Telemetry** | Python, `pyserial`, CSV streaming |
| **Machine Learning** | Python, scikit-learn (IsolationForest) |
| **Development** | Google Colab, Arduino IDE |
| **Version Control** | Git, GitHub |

---

## 🚀 Future Roadmap

- [ ] Replace damaged Arduino and re-establish hardware with corrected GND SOP
- [ ] Implement real-time serial streaming from Arduino to Python backend
- [ ] Deploy Isolation Forest model for live anomaly detection
- [ ] Add MQTT/HTTP telemetry for remote monitoring dashboard
- [ ] Integrate alert system (email/SMS) for predictive maintenance notifications
- [ ] Expand sensor array to include acoustic emission (bearing fault detection)

---

## 🎓 Lessons Learned

1. **Ground Loops Kill:** Always establish common ground before applying high voltage. This failure, while costly, produced a critical safety SOP.
2. **Resilience Through Pivot:** Hardware failures are not project-enders. The pivot to predictive AI transformed a reactive safety system into a proactive ML pipeline.
3. **Two-Tier Safety:** The best systems combine **fast reactive** (hardware) and **smart predictive** (software) layers.
4. **Document Everything:** The diagnostic post-mortem is as valuable as the working prototype. It demonstrates systematic troubleshooting — a core engineering skill.

---

## 📁 Repository Structure (Recommended)

```
ai-fault-detection/
├── README.md                 # This file
├── hardware/
│   ├── arduino_firmware/
│   │   └── fault_detection.ino
│   ├── circuit_diagrams/
│   │   └── wiring_schematic.png
│   └── bom.md                # Bill of Materials
├── software/
│   ├── telemetry/
│   │   └── serial_reader.py
│   └── ml_model/
│       ├── isolation_forest.ipynb
│       └── train_model.py
├── docs/
│   ├── failure_postmortem.md
│   └── sop_ground_loop.md
└── data/
    └── sample_sensor_data.csv
```

---

## 👤 Author

**[Your Name]**  
Electrical Engineering Intern | MITS Gwalior  
📧 [your.email@example.com] | 🔗 [LinkedIn](https://linkedin.com/in/yourprofile)

> *"Engineering is not about perfect first attempts — it's about systematic diagnosis, learning from failure, and building resilient systems."*

---

## 📜 License

This project is open-source under the [MIT License](LICENSE).

---

<p align="center">
  <b>⭐ Star this repo if you found it useful!</b><br>
  <i>Built with curiosity, resilience, and a burnt Arduino.</i>
</p>
