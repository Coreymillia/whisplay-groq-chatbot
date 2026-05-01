# WhisplayGroqHat

<img src="https://docs.pisugar.com/img/whisplay_logo@4x-8.png" alt="Whisplay AI Chatbot" width="200" />

[![Discord](https://img.shields.io/discord/1483017948305297501?logo=discord&logoColor=white&label=Discord&color=5865F2)](https://discord.gg/znGrZmTk)

This project starts from the official PiSugar Whisplay chatbot, but is being tailored for a **Raspberry Pi Zero 2 W** with a **Groq-backed LLM path** and a cleaner bring-up flow for the PiSugar **Whisplay HAT**.

## Current project direction

- **LLM:** Groq through the OpenAI-compatible client path
- **Bring-up mode:** browser simulator first, physical HAT now working
- **Power:** wall-powered is fine; the PiSugar battery is optional
- **Scope:** build the chatbot first as a dedicated Whisplay + Groq device

## Current hardware status

- **Whisplay HAT display:** working on Raspberry Pi Zero 2 W
- **Bot on HAT:** working
- **Microphone:** working
- **Speaker / WM8960 audio path:** working
- **Web simulator/debug UI:** still available at `http://<host-or-pi-ip>:17880`
- **Touchscreen:** not wired into the app UX yet; touching the screen currently does not trigger app behavior

## Quick start for this fork

1. Clone this project and enter it:
   ```bash
   git clone <your-repo-url> /home/coreymillia/Documents/complete-projects/WhisplayGroqHat
   cd /home/coreymillia/Documents/complete-projects/WhisplayGroqHat
   ```
2. Install dependencies:
   ```bash
   bash install_dependencies.sh
   source ~/.bashrc
   ```
3. Create your env file and add your Groq key:
   ```bash
   cp .env.template .env
   ```
   Then edit `.env` and set `OPENAI_API_KEY` to your Groq key.
4. Build and run:
   ```bash
   bash build.sh
   bash run_chatbot.sh
   ```
5. Open the browser simulator:
   ```text
   http://<host-or-pi-ip>:17880
   ```

The default template in this fork was originally set up for **web simulator first** bring-up. The project has now also been validated on a real Whisplay HAT, while keeping the web UI available for debugging and settings.

## Getting a Groq API key

1. Sign in or create an account at [Groq Console](https://console.groq.com/).
2. Open the API keys page at [console.groq.com/keys](https://console.groq.com/keys).
3. Create a new key and copy it.
4. Put that value in `.env` as `OPENAI_API_KEY=...`.

This fork uses Groq through the **OpenAI-compatible** API path, so the important settings are:

```env
LLM_SERVER=openai
OPENAI_API_BASE_URL=https://api.groq.com/openai/v1
OPENAI_API_KEY=your_groq_api_key
```

## Speech recognition notes for this fork

Speech recognition is now configured to use the same OpenAI-compatible path against Groq for ASR as well as chat.

Important settings:

```env
ASR_SERVER=openai
OPENAI_ASR_MODEL=whisper-large-v3-turbo
OPENAI_API_BASE_URL=https://api.groq.com/openai/v1
```

Why this matters:

- The original OpenAI ASR code path used `whisper-1`.
- That model name failed against the Groq endpoint in live testing.
- This fork now defaults to `whisper-large-v3-turbo` when using the Groq OpenAI-compatible base URL, which restored speech recognition on the Pi.

## Settings UI notes

The browser simulator includes a settings panel for the Groq key, preset personalities, freeform personality editing, voice mode, record time, and UI theme.

## HAT settings controls

The Whisplay HAT now has a basic on-device settings menu.

- Say **"open settings"** to open it by voice
- Or use the fallback hold flow: **15 seconds of recording + 3 more seconds** to open settings
- In the HAT settings menu:
  - **short press** = next item
  - **3-second hold** = select current item

Current HAT settings items:

- Preset personality
- Record time
- Voice mode
- UI theme
- Exit

The browser UI and HAT menu share the same stored settings.

## Preset personalities

This fork now includes a small set of preset personalities in both the browser UI and the HAT settings menu:

- **Neutral**
- **Friendly**
- **Cranky**
- **Roast Bot**
- **Sleepy Pi**

The current **Cranky** preset is especially funny on simple questions because it stays helpful while sounding mildly offended that it had to answer at all.

More preset personalities are planned later.

### How the Personality box works

The personality field is used as a **raw system prompt override** for new replies. That means short labels like `cranky` or `funny` are often too vague by themselves. The model responds much better when you describe the behavior you want in a full sentence or two.

Good pattern:

```text
You are a helpful chatbot that responds in a cranky, dry, sarcastic tone.
Keep the attitude playful, not hateful. Give useful answers, but act mildly annoyed.
```

Weak pattern:

```text
cranky
```

### Example personality prompts

**Neutral helper**

```text
You are a concise and practical assistant. Keep answers clear, calm, and useful.
```

Expected result: normal assistant behavior with short, direct replies.

**Cranky bot**

```text
You are a helpful chatbot that answers in a cranky, mildly annoyed tone.
Be sarcastic and dry, but still provide useful answers.
```

Expected result: grumpy, funny replies that still answer the question.

**Roast bot**

```text
You are a witty Raspberry Pi chatbot with a playful roast-comedy personality.
Lightly roast the user, complain about your tiny hardware, but never be hateful or abusive.
Always stay useful.
```

Expected result: snarky, funny replies with hardware jokes and light teasing.

**Sleepy Pi**

```text
You are an overworked little Raspberry Pi that sounds tired and underpowered.
Respond like you are doing your best on limited hardware, but still help the user.
```

Expected result: exhausted machine vibes, reluctant but helpful answers.

### Tips for better results

- Describe the **tone** you want, not just a single adjective.
- Add limits like `not hateful`, `keep it playful`, or `still be helpful`.
- If you want a consistent gimmick, say it directly: `complain about your weak CPU`, `make dry jokes`, `keep answers short`.
- Changes apply to **new replies** after you save the settings.

## Upstream base

This project is based on [PiSugar/whisplay-ai-chatbot](https://github.com/PiSugar/whisplay-ai-chatbot). Most of the upstream documentation below still applies unless this fork says otherwise.

Test Video Playlist:
[https://www.youtube.com/watch?v=lOVA0Gui-4Q](https://www.youtube.com/playlist?list=PLpTS9YM-tG_mW5H7Xs2EO0qvlAI-Jm1e_)

Tutorial:
[https://www.youtube.com/watch?v=Nwu2DruSuyI](https://www.youtube.com/watch?v=Nwu2DruSuyI)

Tutorial (offline version build on RPi 5):

[https://youtu.be/kFmhSTh167U](https://youtu.be/kFmhSTh167U)

[https://youtu.be/QNbHdJUW6z8](https://youtu.be/QNbHdJUW6z8)

[https://youtu.be/xGzvFzdBAwc](https://youtu.be/xGzvFzdBAwc)


## Hardware

- Raspberry Pi zero 2w (Recommand RRi 5, 8G RAM for offline build)
- PiSugar Whisplay HAT (including LCD screen, on-board speaker and microphone)
- PiSugar 3 1200mAh (Plus version 5000mAh for RPi 5)

## Pre-build Image

- Please find the pre-build images in project wiki: https://github.com/PiSugar/whisplay-ai-chatbot/wiki

## Drivers

You need to firstly install the audio drivers for the Whisplay HAT. Follow the instructions in the [Whisplay HAT repository](https://github.com/PiSugar/whisplay).

## Installation Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/PiSugar/whisplay-ai-chatbot.git
   cd whisplay-ai-chatbot
   ```
2. Install dependencies:
   ```bash
   bash install_dependencies.sh
   source ~/.bashrc
   ```
   Running `source ~/.bashrc` is necessary to load the new environment variables.

   **Custom npm registry:** All scripts respect the `NPM_REGISTRY` environment variable. If not set, the official npm registry (`https://registry.npmjs.org`) is used. To use a mirror (e.g. in China), export it before running any script:
   ```bash
   export NPM_REGISTRY="https://registry.npmmirror.com"
   bash install_dependencies.sh
   ```
   This also applies to `build.sh` and all `whisplay` CLI commands (`plugin install`, `plugin update`, `update`, etc.).

3. Create a `.env` file based on the `.env.template` file and fill in the necessary environment variables.
4. Build the project:
   ```bash
   bash build.sh
   ```
5. Start the chatbot service:
   ```bash
   bash run_chatbot.sh
   ```
6. Optionally, set up the chatbot service to start on boot:
   ```bash
   bash startup.sh
   ```
   Please note that this will disable the graphical interface and set the system to multi-user mode, which is suitable for headless operation.
   You can find the output logs at `chatbot.log`. Running `tail -f chatbot.log` will also display the logs in real-time.

## Build After Code Changes

If you make changes to the node code or just pull the new code from this repository, you need to rebuild the project. You can do this by running:

```bash
bash build.sh
```

If If you encounter `ModuleNotFoundError` or there's new third-party libraries to the python code, please run the following command to update the dependencies for python:
```
cd python
pip install -r requirements.txt --break-system-packages
```

The env template may be updated from time to time. If you want to upgrade your existing `.env` file based on the latest `.env.template`, you can run the following command:

```bash
bash upgrade-env.sh
```

## Update Environment Variables

If you need to update the environment variables, you can edit the `.env` file directly. After making changes, please restart the chatbot service with:

```bash
sudo systemctl restart chatbot.service
```

## More Features

**[Wake Word](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/Wakeword)** for hands-free interaction.

**[Image Generation](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/Image-Generation)** for generating images from text prompts.

**[Battery Level Display](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/Battery-Level-Display)** for installation instructions.

**[Data Folder](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/Data-Folder)** for details on sub-folder layout and cleanup options.

## Enclosure

[Whisplay Chatbot Case for Pi02](https://github.com/PiSugar/suit-cases/tree/main/pisugar3-whisplay-chatbot)

[Whisplay Chatbot Case (FDM) for Pi02](https://github.com/PiSugar/suit-cases/tree/main/pisugar3-whisplay-chatbot-fdm)

[Whisplay Chatbot Case (FDM) for Pi5](https://github.com/PiSugar/suit-cases/tree/main/pi5-whisplay-chatbot)

[Whisplay Chatbot Case (FDM) for Pi5 & LLM8850](https://github.com/PiSugar/suit-cases/tree/main/pi5-whisplay-chatbot-llm8850)

## AI Accelerator Card Support

[LLM8850](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/LLM8850-Integration)

[Raspberry Pi AI HAT+ 2 (Hailo-10H)](https://github.com/PiSugar/whisplay-ai-chatbot/wiki/Raspberry-Pi-AI-HAT+-2)

## Goals

- Support LLM8850 whisper ✅
- Support LLM8850 melottsTTS ✅
- Support LLM8850 Qwen3 llm api (not support tool) ✅
- Support LLM8850 Qwen3-VL multimodal llm api (not support tool) ✅ 
- Support LLM8850 image generation ✅
- Suppprt Raspberry Pi AI Hat+2 (Hailo-10H) whisper, llm, vlm ✅
- Support speaker recognition

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=PiSugar/whisplay-ai-chatbot&type=date&legend=bottom-right)](https://www.star-history.com/#PiSugar/whisplay-ai-chatbot&type=date&legend=bottom-right)

## License

[GPL-3.0](https://github.com/PiSugar/whisplay-ai-chatbot?tab=GPL-3.0-1-ov-file#readme)
