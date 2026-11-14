
import os
import json
import secrets
from datetime import datetime, timezone
from typing import Literal

import paho.mqtt.client as mqtt
from dotenv import load_dotenv
from fastapi import FastAPI, Header, HTTPException, Query
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from pymongo import MongoClient
from pymongo.errors import DuplicateKeyError


# --------------------------------------------------
# Configuration
# --------------------------------------------------

load_dotenv()

MONGODB_URI = os.getenv("MONGODB_URI")
MQTT_NAME = os.getenv("MQTT_NAME")

MQTT_BROKER = "135.232.203.144"
MQTT_PORT = 1883

if not MONGODB_URI:
    raise RuntimeError("MONGODB_URI is missing from api/.env")

if not MQTT_NAME:
    raise RuntimeError("MQTT_NAME is missing from api/.env")


# --------------------------------------------------
# Database
# --------------------------------------------------

mongo_client = MongoClient(MONGODB_URI)

db = mongo_client["ecse3038"]
fans = db["fans"]
readings = db["readings"]


# --------------------------------------------------
# MQTT
# --------------------------------------------------

mqtt_client = mqtt.Client(
    callback_api_version=mqtt.CallbackAPIVersion.VERSION2
)


# --------------------------------------------------
# FastAPI application
# --------------------------------------------------

app = FastAPI()


@app.on_event("startup")
def startup():
    mongo_client.admin.command("ping")

    fans.create_index("mac", unique=True)
    readings.create_index("timestamp")

    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()


@app.on_event("shutdown")
def shutdown():
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    mongo_client.close()


# --------------------------------------------------
# Request models
# --------------------------------------------------

class FanCreate(BaseModel):
    mac: str
    room: str


class ReadingCreate(BaseModel):
    mac: str
    humidity: float = Field(ge=0, le=100)


class SettingsUpdate(BaseModel):
    mode: Literal["auto", "on", "off"]
    max_humidity: float = Field(ge=0, le=100)


# --------------------------------------------------
# Helper function
# --------------------------------------------------

def clean_fan(fan):
    return {
        "mac": fan["mac"],
        "room": fan["room"]
    }


# --------------------------------------------------
# A1 - Register a fan
# --------------------------------------------------

@app.post("/fans", status_code=201)
def create_fan(data: FanCreate):

    api_key = secrets.token_urlsafe(16)

    fan = {
        "mac": data.mac,
        "room": data.room,
        "key": api_key
    }

    try:
        fans.insert_one(fan)
    except DuplicateKeyError:
        raise HTTPException(
            status_code=409,
            detail="A fan with this MAC address already exists"
        )

    return {
        "mac": data.mac,
        "room": data.room,
        "key": api_key
    }


# --------------------------------------------------
# A1 - List all fans
# --------------------------------------------------

@app.get("/fans")
def get_fans():

    result = []

    for fan in fans.find():
        result.append(clean_fan(fan))

    return result


# --------------------------------------------------
# A1 - Get one fan
# --------------------------------------------------

@app.get("/fans/{mac}")
def get_fan(mac: str):

    fan = fans.find_one({"mac": mac})

    if fan is None:
        raise HTTPException(
            status_code=404,
            detail="Fan not found"
        )

    return clean_fan(fan)


# --------------------------------------------------
# A2 - Submit a humidity reading
# --------------------------------------------------

@app.post("/readings", status_code=201)
def create_reading(
    data: ReadingCreate,
    api_key: str | None = Header(default=None, alias="X-API-Key")
):

    if not api_key:
        raise HTTPException(
            status_code=401,
            detail="API key is required"
        )

    key_owner = fans.find_one({"key": api_key})

    if key_owner is None:
        raise HTTPException(
            status_code=401,
            detail="Invalid API key"
        )

    if key_owner["mac"] != data.mac:
        raise HTTPException(
            status_code=403,
            detail="API key does not belong to this fan"
        )

    reading = {
        "mac": data.mac,
        "humidity": data.humidity,
        "timestamp": datetime.now(timezone.utc)
    }

    result = readings.insert_one(reading)

    return {
        "id": str(result.inserted_id),
        "mac": data.mac,
        "humidity": data.humidity,
        "timestamp": reading["timestamp"]
    }


# --------------------------------------------------
# A3 - Retrieve readings
# --------------------------------------------------

@app.get("/readings")
def get_readings(
    mac: str | None = None,
    limit: int = Query(default=10, ge=1, le=100)
):

    query = {}

    if mac is not None:
        query["mac"] = mac

    result = []

    cursor = (
        readings.find(query)
        .sort("timestamp", -1)
        .limit(limit)
    )

    for reading in cursor:
        reading["id"] = str(reading.pop("_id"))
        result.append(reading)

    return result


# --------------------------------------------------
# A4 - Update fan settings and publish MQTT message
# --------------------------------------------------

@app.put("/fans/{mac}/settings")
def update_settings(mac: str, data: SettingsUpdate):

    fan = fans.find_one({"mac": mac})

    if fan is None:
        raise HTTPException(
            status_code=404,
            detail="Fan not found"
        )

    is_new = "settings" not in fan

    settings = {
        "mode": data.mode,
        "max_humidity": data.max_humidity
    }

    fans.update_one(
        {"mac": mac},
        {"$set": {"settings": settings}}
    )

    topic = f"ecse3038/{MQTT_NAME}/fan/settings"

    message = json.dumps(settings)

    publish_result = mqtt_client.publish(
        topic,
        message,
        retain=True
    )

    if publish_result.rc != mqtt.MQTT_ERR_SUCCESS:
        raise HTTPException(
            status_code=503,
            detail="Could not publish MQTT settings"
        )

    return JSONResponse(
        status_code=201 if is_new else 200,
        content={
            "mac": mac,
            "settings": settings
        }
    )


# --------------------------------------------------
# A5 - Delete a fan
# --------------------------------------------------

@app.delete("/fans/{mac}", status_code=204)
def delete_fan(
    mac: str,
    api_key: str | None = Header(default=None, alias="X-API-Key")
):

    fan = fans.find_one({"mac": mac})

    if fan is None:
        raise HTTPException(
            status_code=404,
            detail="Fan not found"
        )

    if not api_key:
        raise HTTPException(
            status_code=401,
            detail="API key is required"
        )

    key_owner = fans.find_one({"key": api_key})

    if key_owner is None:
        raise HTTPException(
            status_code=401,
            detail="Invalid API key"
        )

    if key_owner["mac"] != mac:
        raise HTTPException(
            status_code=403,
            detail="API key does not belong to this fan"
        )

    fans.delete_one({"mac": mac})

    return None
