import json
import socket
import time
from datetime import datetime
from pathlib import Path

import pyttsx3
import requests
import speech_recognition as sr

from config import (
    ESP8266_HOST, ESP8266_PORT, CAMERA_SHOT_URL, CAPTURE_DIR, LOG_DIR,
    VOICE_LANGUAGE, MICROPHONE_INDEX, CONNECTION_TIMEOUT,
)

PROJECT_DIR = Path(__file__).resolve().parent.parent
CAPTURE_PATH = Path(CAPTURE_DIR)
LOG_PATH = Path(LOG_DIR)
EVENT_LOG = LOG_PATH / "operation1122_events.jsonl"
CAPTURE_PATH.mkdir(parents=True, exist_ok=True)
LOG_PATH.mkdir(parents=True, exist_ok=True)

recognizer = sr.Recognizer()
recognizer.dynamic_energy_threshold = True
recognizer.pause_threshold = 0.7
speaker = pyttsx3.init()
speaker.setProperty("rate", 165)
session_events = []
awaiting_command = False
WAKE_WORD = "captain"
COMMAND_WINDOW_SECONDS = 8


def speak(message):
    print(f"\\nCAPTAIN: {message}")
    speaker.say(message)
    speaker.runAndWait()


def normalize(text):
    return " ".join(text.lower().strip().replace("-", " ").split())


def log_event(event, details=None):
    entry = {
        "timestamp": datetime.now().astimezone().isoformat(),
        "event": event,
        "details": details or {},
    }
    session_events.append(entry)
    try:
        with EVENT_LOG.open("a", encoding="utf-8") as file:
            file.write(json.dumps(entry, ensure_ascii=False) + "\\n")
    except OSError as exc:
        print(f"[ERROR] Could not append event log: {exc}")


def send_command(command):
    response = bytearray()
    try:
        with socket.create_connection(
            (ESP8266_HOST, ESP8266_PORT), timeout=CONNECTION_TIMEOUT
        ) as sock:
            sock.settimeout(0.4)
            sock.sendall((command.strip() + "\n").encode("utf-8"))
            deadline = time.monotonic() + 3.0
            while time.monotonic() < deadline:
                try:
                    chunk = sock.recv(1024)
                    if not chunk:
                        break
                    response.extend(chunk)
                except socket.timeout:
                    continue
        reply = response.decode("utf-8", errors="replace").strip()
        print(f"[ESP/Mega] {reply or '(no response received)'}")
        log_event("command", {"command": command, "response": reply})
        return reply
    except OSError as exc:
        print(f"[ERROR] ESP connection failed: {exc}")
        log_event("connection_error", {"command": command, "error": str(exc)})
        speak("I cannot reach the robot controller. Check the bridge connection.")
        return ""


def confirmed(reply, *tokens):
    upper = reply.upper()
    return bool(reply) and any(token.upper() in upper for token in tokens)


def stop_robot():
    reply = send_command("STOP")
    if confirmed(reply, "ACK:STOP", "STOPPED"):
        speak("Roger that. Robot stop command acknowledged.")
        log_event("stop_acknowledged")
    else:
        speak("Stop command was sent, but I could not confirm the controller acknowledged it.")
        log_event("stop_acknowledgement_missing", {"response": reply})


def check_status():
    reply = send_command("STATUS")
    speak("Status response received. Check the console for the controller's exact report."
          if reply else "I did not receive a status response.")
    log_event("status_checked", {"response": reply})


def capture_data():
    reply = send_command("STOP")
    if not confirmed(reply, "ACK:STOP", "STOPPED"):
        speak("Capture cancelled. I could not confirm the robot stopped.")
        log_event("capture_cancelled", {"reason": "Stop acknowledgement not confirmed"})
        return
    destination = CAPTURE_PATH / (
        "capture_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".jpg"
    )
    try:
        response = requests.get(CAMERA_SHOT_URL, timeout=10)
        response.raise_for_status()
        content_type = response.headers.get("Content-Type", "")
        if not content_type.lower().startswith("image/"):
            raise ValueError(f"Unexpected camera content type: {content_type or '(missing)'}")
        if not response.content:
            raise ValueError("Camera returned an empty image.")
        destination.write_bytes(response.content)
        log_event("camera_capture", {"file": str(destination), "bytes": len(response.content)})
        print(f"[OK] Saved image: {destination}")
        speak("Copy that. Image captured and saved on the laptop.")
    except (requests.RequestException, OSError, ValueError) as exc:
        print(f"[ERROR] Camera capture failed: {exc}")
        log_event("camera_capture_failed", {"error": str(exc)})
        speak("Camera capture failed. Check the phone camera connection.")


def send_data():
    reply = send_command("STATUS")
    report = {
        "created_at": datetime.now().astimezone().isoformat(),
        "robot_status_response": reply,
        "recent_session_events": session_events[-100:],
        "capture_directory": str(CAPTURE_PATH),
        "event_log": str(EVENT_LOG),
    }
    destination = LOG_PATH / (
        "mission_report_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".json"
    )
    try:
        destination.write_text(
            json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8"
        )
        log_event("mission_report_saved", {"file": str(destination)})
        print(f"[OK] Mission report saved: {destination}")
        speak("Roger that. Mission report saved on the laptop. No external transmission was performed.")
    except OSError as exc:
        print(f"[ERROR] Could not save report: {exc}")
        log_event("mission_report_save_failed", {"error": str(exc)})
        speak("I could not save the mission report.")


def rotate_left():
    reply = send_command("ROTATE LEFT")
    if confirmed(reply, "ACK:ROTATE:LEFT", "STATUS:ROTATING_LEFT"):
        speak("Roger. Rotating left until you give another command.")
        log_event("rotate_left_acknowledged", {"response": reply})
    else:
        speak("Left rotation was requested, but acknowledgement was not confirmed.")
        log_event("rotate_left_acknowledgement_missing", {"response": reply})


def rotate_right():
    reply = send_command("ROTATE RIGHT")
    if confirmed(reply, "ACK:ROTATE:RIGHT", "STATUS:ROTATING_RIGHT"):
        speak("Roger. Rotating right until you give another command.")
        log_event("rotate_right_acknowledged", {"response": reply})
    else:
        speak("Right rotation was requested, but acknowledgement was not confirmed.")
        log_event("rotate_right_acknowledgement_missing", {"response": reply})


def start_following():
    reply = send_command("START")
    if confirmed(reply, "ACK:START", "FOLLOWING", "STARTED", "REACQUIRING_LINE"):
        speak("Copy that. Searching in the last rotation direction until the center sensors detect the line.")
        log_event("start_acknowledged", {"response": reply})
    else:
        speak("Start command was sent, but acknowledgement was not confirmed.")
        log_event("start_acknowledgement_missing", {"response": reply})


def execute_command(text):
    command = normalize(text)
    print(f"[VOICE COMMAND] {text}")

    # Emergency stop aliases are accepted even without the wake phrase.
    if any(p in command for p in (
        "emergency stop", "stop now", "stop robot", "halt", "freeze", "abort mission",
    )):
        stop_robot()
    elif any(p in command for p in (
        "do you copy", "status", "status report", "how is the robot",
        "report status", "robot status",
    )):
        check_status()
    elif any(p in command for p in (
        "capture data", "take a picture", "take picture", "capture image",
        "take a photo", "save snapshot",
    )):
        capture_data()
    elif any(p in command for p in (
        "send data", "save mission data", "export mission data", "mission report",
    )):
        send_data()
    elif any(p in command for p in (
        "start following", "continue following", "resume following",
        "continue the mission", "resume mission", "start robot",
    )):
        start_following()
    elif any(p in command for p in ("rotate left", "turn left")):
        rotate_left()
    elif any(p in command for p in ("rotate right", "turn right")):
        rotate_right()
    elif any(p in command for p in ("what can you do", "help", "list commands", "commands")):
        speak("I can rotate left or right, start or resume line following, stop the robot, "
              "check status, capture data, and save a mission report.")
    elif any(p in command for p in (
        "exit assistant", "shutdown assistant", "quit assistant", "stand down captain",
    )):
        speak("Captain voice control shutting down. Over and out.")
        log_event("assistant_shutdown")
        return False
    else:
        speak("I did not recognize that command. Say Captain, then give me one clear instruction.")
        log_event("unrecognized_command", {"command": text})
    return True


def strip_wake_word(text):
    words = normalize(text).split()
    try:
        index = words.index(WAKE_WORD)
    except ValueError:
        return None
    return " ".join(words[index + 1:]).strip()


def listen_once(source, timeout=8, phrase_time_limit=6):
    audio = recognizer.listen(source, timeout=timeout, phrase_time_limit=phrase_time_limit)
    return recognizer.recognize_google(audio, language=VOICE_LANGUAGE)


def main():
    global awaiting_command
    print("=" * 58)
    print(" OPERATION 1122 - CAPTAIN V2")
    print("=" * 58)
    print(f"ESP8266: {ESP8266_HOST}:{ESP8266_PORT}")
    print(f"Camera: {CAMERA_SHOT_URL}")
    print("Wake phrase: Captain")
    print("Emergency stop aliases work without the wake phrase.")
    print("Rotation and center-sensor line reacquisition are enabled.")
    print("Say 'Captain, exit assistant' to quit.")

    try:
        microphones = sr.Microphone.list_microphone_names()
        if not microphones:
            print("[ERROR] No microphone detected.")
            return
        for index, name in enumerate(microphones):
            print(f"  [{index}] {name}")
        speak("Captain voice system online. Standing by.")

        with sr.Microphone(device_index=MICROPHONE_INDEX) as source:
            print("[INFO] Calibrating background noise. Please stay quiet briefly...")
            recognizer.adjust_for_ambient_noise(source, duration=1)
            while True:
                try:
                    if awaiting_command:
                        print("\\nCaptain is listening for your command...")
                        text = listen_once(source, timeout=COMMAND_WINDOW_SECONDS, phrase_time_limit=7)
                        awaiting_command = False
                        if not execute_command(text):
                            break
                        continue

                    print("\\nStanding by. Say 'Captain' to wake me...")
                    text = listen_once(source, timeout=8, phrase_time_limit=6)
                    normalized = normalize(text)
                    print(f"[HEARD] {text}")

                    if any(p in normalized for p in (
                        "emergency stop", "stop now", "stop robot", "halt", "abort mission",
                    )):
                        execute_command(text)
                        continue

                    after_wake = strip_wake_word(text)
                    if after_wake is None:
                        print("[INFO] Wake phrase not detected; ignoring utterance.")
                        continue
                    log_event("wake_phrase_detected", {"utterance": text})
                    if after_wake:
                        if not execute_command(after_wake):
                            break
                    else:
                        speak("Copy that. I'm listening.")
                        awaiting_command = True

                except sr.WaitTimeoutError:
                    if awaiting_command:
                        awaiting_command = False
                        speak("No command received. Returning to standby.")
                    else:
                        print("[INFO] No speech detected; continuing standby.")
                except sr.UnknownValueError:
                    print("[INFO] Speech was not understood.")
                    if awaiting_command:
                        awaiting_command = False
                        speak("I did not catch that. Say Captain to try again.")
                except sr.RequestError as exc:
                    print(f"[ERROR] Speech recognition service: {exc}")
                    speak("Speech recognition is unavailable. Check the internet connection.")
                    time.sleep(2)
    except KeyboardInterrupt:
        print("\\n[INFO] Stopped by keyboard.")
    except Exception as exc:
        print(f"[ERROR] Voice controller failed: {exc!r}")
        log_event("voice_controller_error", {"error": repr(exc)})
    finally:
        log_event("voice_controller_exit")
        print("[INFO] Voice controller stopped.")


if __name__ == "__main__":
    main()

