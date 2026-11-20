# CalmSense: Contactless Pre-Episode Detection & Early-Warning System

**CalmSense** is a contactless monitoring and early-warning prototype built with the **Presage SmartSpectra C++ SDK** and **OpenCV**. It helps parents and caregivers detect early physiological and behavioral signs of sensory overload or an approaching meltdown in children on the autism spectrum—without requiring uncomfortable wearables or chest straps.

---

## Key Features
* **Contactless Camera Sensing:** Extracts pulse rate, breathing rate, Heart Rate Variability (HRV/RMSSD), Baevsky Stress Index, and blink cues from a standard video feed.
* **Hand-to-Ear / Facial Occlusion Detection:** Uses OpenCV YCrCb skin-masking and frame-differencing across upper ear/temple regions to detect when hands are lifted to cover the ears.
* **Personalized 3-Stage Pre-Episode Signature:**
  * **Stage 1 (Early ANS Arousal):** HRV (RMSSD) drop > 15%, rising Stress Index, or blink-rate spikes.
  * **Stage 2 (Sensory Escalation):** Shallow/rapid breathing paired with rocking/pacing cues.
  * **Stage 3 (Acute Overload):** Simultaneous pulse and breathing spikes or hands lifted to cover ears.
* **Proactive Parent Alerts & Auto-Remediations:** Triggers a parent alert banner and simulates smart-home calming routines (calming music, Alexa quiet-time routine, comfort TV show, and dimming lights).

---

## Architecture Overview

```text
[Recorded .mp4] ──► [C++ SmartSpectra + OpenCV Backend] ──► appends to vitals_stream.jsonl
                                                                           │
[Windows Browser (Chrome/Edge)] ◄──(GET /vitals.json)── [Python HTTP Server :8080]
```
The sample video is in this link

https://youtu.be/bbfvY9Ayhko

* **`main.cpp`:** Processes video frames with SmartSpectra SDK v3.5+ and OpenCV, evaluates the 3-stage risk model, and appends JSON telemetry lines to `vitals_stream.jsonl`.
* **`vitals_stream.jsonl`:** Local newline-delimited JSON queue generated at runtime by the C++ backend.
* **`server.py`:** Zero-dependency Python HTTP server that serves the web dashboard on port `8080` and tails `vitals_stream.jsonl` one reading per second.
* **`web/index.html`:** Real-time dashboard with baseline calibration modal, rolling vitals chart, 3-stage progression cards, and auto-remediation toggles.

---

## Prerequisites & Getting Your Presage API Key

1. **Operating System:** Windows 10/11 with **WSL2 (Ubuntu 24.04 Noble)**, or native Ubuntu 24.04 Linux.
2. **Get a Free Presage API Key:**
   * Go to the [Presage Developer Portal](https://presagetech.com).
   * Sign up or log in to create a new project/application.
   * Copy your **API Key** from the developer dashboard.

---

## Step 1: Set Up WSL2 (Ubuntu 24.04) on Windows

If you are running on Windows and do not have WSL2 installed yet, open **PowerShell as Administrator** and run:

```powershell
wsl --install -d Ubuntu-24.04
```

Restart your PC if prompted, launch **Ubuntu 24.04** from the Start menu, and set up your Linux username and password.

---

## Step 2: Install C++ Build Tools, OpenCV & SmartSpectra SDK

Inside your **Ubuntu terminal**, install the required compilers, CMake, OpenCV headers, Python 3, and Presage's official APT repository:

```bash
# 1. Install build tools, OpenCV, and Python 3
sudo apt update
sudo apt install -y build-essential cmake curl gnupg lsb-release libopencv-dev python3

# 2. Add Presage's official GPG signing key
sudo install -d -m 0755 /etc/apt/keyrings
curl -fsSL https://presagetech.com | sudo gpg --dearmor -o /etc/apt/keyrings/presage-archive-keyring.gpg
sudo chmod 644 /etc/apt/keyrings/presage-archive-keyring.gpg

# 3. Add the Presage Ubuntu 24.04 (noble) APT repository
echo "deb [signed-by=/etc/apt/keyrings/presage-archive-keyring.gpg] https://presagetech.com noble main" | sudo tee /etc/apt/sources.list.d/presage-technologies.list

# 4. Install the SmartSpectra C++ SDK
sudo apt update
sudo apt install -y libsmartspectra-dev
```

---

## Step 3: Clone & Configure the Project

1. **Clone this repository and enter the directory:**
   ```bash
   git clone https://github.com<your-username>/<your-repo-name>.git
   cd <your-repo-name>
   ```

2. **Add your Presage API Key:**
   Open `main.cpp` and replace `YOUR_PRESAGE_API_KEY` with your actual key (or run `sed`):
   ```bash
   sed -i 's/YOUR_PRESAGE_API_KEY/paste_your_actual_api_key_here/' main.cpp
   ```
   *(Security Tip: Never commit your real API key to GitHub. Reset it to `YOUR_PRESAGE_API_KEY` or load it from an environment variable before pushing changes.)*

3. **Add your test video:**
   Place a recorded `.mp4` clip named `test_video.mp4` in both the project root (for C++) and inside `web/` (for browser playback):
   ```bash
   cp /path/to/your_video.mp4 ./test_video.mp4
   cp ./test_video.mp4 ./web/test_video.mp4
   ```

---

## Step 4: Compile the C++ Backend

Build the C++ binary using CMake:

```bash
rm -rf build
cmake -B build
cmake --build build -j
```

This generates the executable binary at `./build/calmsense_backend`.

---

## Step 5: Run the Application (Two Terminals)

Run the Python web server and the C++ processing engine in two separate Ubuntu terminal windows/tabs:

### Terminal 1 — Start the Local Python Web Server
```bash
python3 server.py
```
* **Port:** `http://localhost:8080`
* **Function:** Serves `web/index.html` and reads newly appended lines from `vitals_stream.jsonl` via `/vitals.json`.

### Terminal 2 — Start the C++ SmartSpectra Backend
```bash
./build/calmsense_backend
```
* **Generated File:** Automatically creates and appends live JSON records to `vitals_stream.jsonl` in the project root.
* **Looping:** Automatically loops `test_video.mp4` continuously for live demos.

### Open the Dashboard
Open **Chrome** or **Edge** on Windows and navigate to:
```text
http://localhost:8080
```

---

## Recommended `.gitignore`

Before pushing to GitHub, create a `.gitignore` file so compiled binaries, local video files, and runtime JSON logs aren't checked in:

```gitignore
build/
vitals_stream.jsonl
*.mp4
.vscode/
```

---

## Disclaimer
CalmSense is a wellness and research prototype intended for informational and caregiver support purposes only. SmartSpectra SDK metrics are not cleared for medical diagnosis.
