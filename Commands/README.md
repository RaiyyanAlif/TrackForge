# 🤖 TrackForge — Quick Commands Guide

A quick-reference guide for opening, editing, and running the TrackForge robotics project on Windows.

**Project directory:** `D:\Operation1122`

**GitHub Repository:** https://github.com/RaiyyanAlif/TrackForge

---

## 📂 1. Open the Project Folder

Open the project directory in File Explorer:

```powershell
explorer D:\Operation1122
```

Open the project in the current PowerShell terminal:

```powershell
cd D:\Operation1122
```

---

## ⚙️ 2. Open `config.py`

Use this file to modify IP addresses when the ESP8266 or camera IP address changes.

If `config.py` is inside the `captain` folder:

```powershell
notepad D:\Operation1122\captain\config.py
```

If `config.py` is in the project root:

```powershell
notepad D:\Operation1122\config.py
```

**Note:** Use the path where your actual `config.py` file is stored. Keep it in the location expected by your Python imports.

---

## 🧠 3. Open Arduino Mega Firmware

Open the main robot controller code:

```powershell
notepad D:\Operation1122\mega_controller\mega_controller.ino
```

**File:** `mega_controller/mega_controller.ino`

Responsible for the robot's line-following logic, movement, and rotation control.

---

## 📡 4. Open ESP8266 Firmware

Open the wireless communication server code:

```powershell
notepad D:\Operation1122\esp8266_server\esp8266_server.ino
```

**File:** `esp8266_server/esp8266_server.ino`

Responsible for Wi-Fi communication and forwarding commands between the computer and Arduino Mega.

---

## 🎙️ 5. Open Python Voice-Control Code

Open the main Python application:

```powershell
notepad D:\Operation1122\captain\operation1122_voice.py
```

**File:** `captain/operation1122_voice.py`

Responsible for voice recognition, sending wireless commands, spoken feedback, and configured camera/data operations.

---

## 🚀 6. Run TrackForge Voice Control

Open PowerShell and execute:

```powershell
cd D:\Operation1122
.\.venv\Scripts\Activate.ps1
python .\captain\operation1122_voice.py
```

This activates the Python virtual environment and launches the voice-control application.

### If the Virtual Environment Is Already Activated

Run only:

```powershell
python .\captain\operation1122_voice.py
```

### Run Without Activating the Environment

```powershell
D:\Operation1122\.venv\Scripts\python.exe D:\Operation1122\captain\operation1122_voice.py
```

### If PowerShell Cannot Find the Virtual Environment

Check whether it exists:

```powershell
Test-Path D:\Operation1122\.venv\Scripts\Activate.ps1
```

If the result is `False`, create the environment and install the dependencies:

```powershell
cd D:\Operation1122
py -3.10 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install SpeechRecognition pyttsx3 requests PyAudio
```

---

## 📝 7. Open README.md

Open this documentation file in Notepad:

```powershell
notepad D:\Operation1122\README.md
```

---

## 🗂️ 8. Open All Important Files

Run these commands together to open the main project files in Notepad:

```powershell
notepad D:\Operation1122\captain\config.py
notepad D:\Operation1122\mega_controller\mega_controller.ino
notepad D:\Operation1122\esp8266_server\esp8266_server.ino
notepad D:\Operation1122\captain\operation1122_voice.py
```

If `config.py` is in the project root, replace its command with:

```powershell
notepad D:\Operation1122\config.py
```

---

## 🎮 9. Supported Voice Commands

| Voice Command | Action |
|---|---|
| `Do you copy` | Request robot status |
| `Captain` | Request robot status |
| `Status` | Request robot status |
| `How is the robot` | Request robot status |
| `Start following` | Start or resume line following |
| `Continue following` | Resume line following |
| `Resume following` | Resume line following |
| `Rotate left` | Rotate left |
| `Rotate right` | Rotate right |
| `Stop now` | Stop the robot |
| `Stop robot` | Stop the robot |
| `Halt` | Stop the robot |
| `Emergency stop` | Stop the robot |
| `Capture data` | Trigger configured camera capture |
| `Take a picture` | Trigger configured camera capture |
| `Send data` | Request mission-data/report handling |
| `Save mission data` | Request mission-data/report handling |

*Voice command availability depends on the implementation in the current Python application.*

---

## 🔧 10. Useful Git Commands

Run these from the project directory.

Check changed files:

```powershell
git status
```

Stage the updated README:

```powershell
git add README.md
```

Commit the changes:

```powershell
git commit -m "Update TrackForge quick commands guide"
```

Push the changes to GitHub:

```powershell
git push
```

---

## ⚠️ Important Notes

- Connect the computer and ESP8266 to the appropriate Wi-Fi network.
- Verify the IP addresses in the configuration expected by the Python application.
- Ensure the Arduino Mega and ESP8266 firmware are uploaded before testing the robot.
- Confirm the correct camera stream URL before using camera-based capture.
- Keep credentials and private configuration files out of public GitHub commits.
- Test rotation and movement with the wheels lifted off the ground before running the robot on a track.

---

<p align="center">
  <b>TrackForge — Code. Connect. Control. 🤖</b>
</p>
