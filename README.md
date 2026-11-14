# ECSE3038 Test 2 – Smart Bathroom Fan

## Project Overview

This project implements an IoT-based bathroom fan monitoring and control system using an ESP32, DHT22 humidity sensor, FastAPI, MongoDB Atlas and MQTT.

The system monitors bathroom humidity and allows the fan to operate automatically based on a configurable humidity threshold or through manual ON/OFF commands.

## System Features

- Measures humidity using a DHT22 sensor.
- Sends humidity readings to a FastAPI server every 10 seconds.
- Stores fan information and sensor readings in MongoDB Atlas.
- Uses API keys to authenticate devices.
- Receives fan settings through MQTT.
- Supports automatic, manual ON and manual OFF modes.
- Publishes fan operating state through MQTT.
- Publishes device online/offline status.

## Repository Structure

```text

ECSE3038-test2/
│
├── api/
│   ├── app.py
│   └── requirements.txt
│
├── board/
│   ├── include/
│   │   └── secrets.h (created locally; not committed)
│   ├── src/
│   │   └── main.cpp
│   ├── diagram.json
│   ├── platformio.ini
│   └── wokwi.toml
│
├── .gitignore
└── README.md

```

**Note:** `.env`, `secrets.h`, virtual environments and generated build files are excluded from GitHub.

## Hardware Components

| Component | Purpose |
|---|---|
| ESP32 DevKit | Main microcontroller |
| DHT22 | Humidity measurement |
| LED | Simulated bathroom fan indicator |
| 220 Ω resistor | LED current limiting |

## Pin Connections

| Component | ESP32 Pin |
|---|---|
| DHT22 VCC | 3V3 |
| DHT22 SDA/Data | GPIO 15 |
| DHT22 GND | GND |
| LED anode | GPIO 2 through 220 Ω resistor |
| LED cathode | GND through resistor |

## Software and Technologies

- Arduino framework
- PlatformIO
- Wokwi Simulator
- Python
- FastAPI
- MongoDB Atlas
- Paho MQTT
- PubSubClient
- ArduinoJson

## API Setup

### 1. Install dependencies

Navigate to the API directory:

```bash
cd api
```

Create and activate a virtual environment:

```bash
python -m venv venv
```

Windows:

```powershell
venv\Scripts\activate
```

Install dependencies:

```bash
pip install -r requirements.txt
```

### 2. Configure environment variables

Create an `.env` file inside the `api` directory:

```env
MONGODB_URI=YOUR_MONGODB_CONNECTION_STRING
MQTT_NAME=fan3038
```

Replace the MongoDB placeholder with your own connection string.

### 3. Start the API

```bash
uvicorn app:app --reload
```

The API documentation is available at:

```text
http://127.0.0.1:8000/docs
```

## API Endpoints

| Method | Endpoint | Description |
|---|---|---|
| POST | `/fans` | Register a fan |
| GET | `/fans` | Retrieve registered fans |
| GET | `/fans/{mac}` | Retrieve a specific fan |
| POST | `/readings` | Submit humidity readings |
| GET | `/readings` | Retrieve sensor readings |
| PUT | `/fans/{mac}/settings` | Update fan settings |
| DELETE | `/fans/{mac}` | Delete a registered fan |

API keys are generated during fan registration and are required for authenticated device operations.

## MQTT Configuration

**Broker:** `135.232.203.144`

**Port:** `1883`

The MQTT topics use the device name `fan3038`.

| Topic | Direction | Purpose |
|---|---|---|
| `ecse3038/fan3038/fan/settings` | API → ESP32 | Receives fan settings |
| `ecse3038/fan3038/fan/state` | ESP32 → Broker | Publishes fan state |
| `ecse3038/fan3038/status` | ESP32 → Broker | Publishes online/offline status |

MQTT messages use retained publishing where required so that the latest device status and fan state remain available to subscribers.

## ESP32 Configuration

Create `board/include/secrets.h` and configure the following values:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

#define API_URL "YOUR_API_BASE_URL"

#define DEVICE_MAC "YOUR_REGISTERED_FAN_MAC"
#define DEVICE_KEY "YOUR_GENERATED_API_KEY"

#endif
```

Use the API URL and credentials belonging to your registered fan.

**Do not upload `secrets.h` to GitHub.**

## Wokwi Simulation

1. Open the `board` directory in VS Code.
2. Install the Wokwi Simulator extension.
3. Open `diagram.json`.
4. Start the Wokwi simulation.
5. Monitor the Serial Monitor for Wi-Fi, MQTT and API responses.
6. Use the DHT22 controls to change the simulated humidity.

### Automatic Fan Operation

The fan operates according to the configured maximum humidity.

For example, with a threshold of 65%:

- Humidity at or below 65%: Fan OFF.
- Humidity above 65%: Fan ON.

### Manual Fan Operation

The API supports three operating modes:

- `auto`: Fan operates according to the humidity threshold.
- `on`: Fan remains ON.
- `off`: Fan remains OFF.

Example settings request:

```json
{
  "mode": "auto",
  "max_humidity": 65
}
```

## Building the ESP32 Firmware

Navigate to the board directory:

```bash
cd board
```

Compile the firmware using PlatformIO:

```bash
pio run
```

Start the simulation using the Wokwi VS Code extension.

## Security

- API keys are stored locally in `secrets.h`.
- MongoDB credentials are stored in `.env`.
- Secret files are excluded from version control.
- Device readings require API-key authentication.

The following files contain private credentials and are created locally:

- `api/.env` – stores the MongoDB connection string and MQTT device name.
- `board/include/secrets.h` – stores Wi-Fi credentials, API URL, registered device MAC address and API key.

These files are excluded from Git version control using `.gitignore` and must never be uploaded to the public GitHub repository.

Before running the project, create these files locally and enter your own configuration values.

## Project Repository

GitHub: https://github.com/JedeaM/ECSE3038-test2

## Conclusion

The project demonstrates an IoT bathroom fan system integrating sensor monitoring, REST API communication, cloud database storage and MQTT-based control. The ESP32 processes humidity readings and responds to remotely configured fan operating modes.
