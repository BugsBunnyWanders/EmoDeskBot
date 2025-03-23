# EmoDeskBot: AI-Powered OLED Display

EmoDeskBot is a smart desk companion that uses OpenAI's API to process your requests and display information on an OLED screen connected to an ESP32. It can show facial expressions, display text, and respond to your questions through both spoken and written responses.

## Features

- **AI-Powered Responses**: Uses OpenAI GPT-3.5 to understand and respond to your questions
- **High-Quality Voice**: Speaks with premium voices from OpenAI's Text-to-Speech API
- **Real-time Voice Interaction**: Continuously listens for activation phrases like "Hey Bot"
- **Facial Expressions**: Shows happy, neutral, or sad faces based on the conversation context
- **Information Display**: Shows time, weather, and custom text on the OLED display
- **Web Interface**: Control and test the display through a simple web interface
- **Voice or Text Interaction**: Talk to the bot or use text commands
- **Text Display**: Can display text information like time or weather
- **High-Quality Voice**: Uses OpenAI's TTS for natural-sounding responses
- **Real-time Interaction**: Supports continuous listening for activation phrases and streaming responses

## Hardware Requirements

- ESP32 development board
- SSD1306 OLED display (128x64 resolution)
- Breadboard and jumper wires
- Microphone (for voice input)
- Speakers (for voice output)

## Wiring Instructions

Connect the OLED display to your ESP32 as follows:
- OLED VCC → 3.3V
- OLED GND → GND
- OLED SCL → GPIO 22
- OLED SDA → GPIO 21

## Setup Instructions

### ESP32 Setup

1. Open `test/esp32_ai_display.ino` in the Arduino IDE
2. Install required libraries:
   - ESP32 board support
   - Adafruit GFX Library
   - Adafruit SSD1306
3. Update WiFi credentials in the code:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
4. Upload the code to your ESP32
5. Note the IP address displayed on the OLED screen or in the Serial Monitor

### Python Setup

1. Create a virtual environment (recommended):
   ```
   python -m venv venv
   source venv/bin/activate  # On Windows: venv\Scripts\activate
   ```

2. Install dependencies:
   ```
   pip install -r requirements.txt
   ```

3. Create a `.env` file with your OpenAI API key (see `.env.example`):
   ```
   OPENAI_API_KEY=your_api_key_here
   ESP32_IP=192.168.x.x  # The IP address of your ESP32
   ESP32_PORT=80
   ```

## Usage

1. Start the AI Desk Bot with real-time voice interaction (recommended):
   ```
   python ai_desk_bot.py --realtime
   ```
   This mode continuously listens for activation phrases like "Hey Bot" and responds instantly

2. For standard voice input and output:
   ```
   python ai_desk_bot.py
   ```

3. For text input instead of voice:
   ```
   python ai_desk_bot.py --text
   ```

4. For no voice output:
   ```
   python ai_desk_bot.py --mute
   ```

5. To process all speech without needing an activation phrase:
   ```
   python ai_desk_bot.py --realtime --always-listen
   ```

6. To select a different OpenAI voice:
   ```
   python ai_desk_bot.py --openai-voice nova
   ```
   Available voices: alloy, echo, fable, onyx, nova, shimmer

7. If you prefer the fallback TTS instead of OpenAI's high-quality voices:
   ```
   python ai_desk_bot.py --fallback-tts
   ```

8. To use OpenAI's streaming API for faster responses:
   ```
   python ai_desk_bot.py --stream
   ```

9. To use OpenAI's Whisper model for more accurate speech recognition:
   ```
   python ai_desk_bot.py --use-openai-speech
   ```

## Voice Interaction

Talk to the bot using these commands:

1. Run the bot in voice mode (default):
   ```
   python ai_desk_bot.py
   ```

2. To enable high-quality OpenAI TTS voices:
   ```
   python ai_desk_bot.py --openai-voice nova
   ```

3. To use fallback TTS instead of OpenAI:
   ```
   python ai_desk_bot.py --fallback-tts
   ```

## Real-time Interaction

EmoDeskBot supports real-time interaction modes:

1. **Activation-based**: The bot listens continuously and responds when you say an activation phrase like "Hey Bot" or "Emo"
2. **Streaming responses**: The bot can stream responses word-by-word for more natural conversations
3. **Enhanced speech recognition**: Optional use of OpenAI's Whisper model for better speech understanding

Example commands:
```
# Run with real-time listening for activation phrases and streaming responses
python ai_desk_bot.py --realtime --stream

# Run with real-time listening, always processing all speech
python ai_desk_bot.py --realtime --always-listen

# Run with real-time listening and OpenAI's Whisper for better recognition
python ai_desk_bot.py --realtime --use-openai-speech

# Combine multiple options for the most natural experience
python ai_desk_bot.py --realtime --stream --use-openai-speech --openai-voice nova
```

When using real-time mode with activation phrases, the bot listens continuously and activates when you say one of these phrases:
- "Hey bot"
- "Hey desk bot"
- "Emo"
- "Emodesk"
- "Hi bot"
- "Hello bot"

## ESP32 Display

## Project Structure

- `ai_desk_bot.py`: Main Python script using OpenAI API
- `test/esp32_ai_display.ino`: ESP32 firmware for the OLED display
- `docs/`: Documentation and project notes
- `vision/`: Legacy computer vision components (not used in current version)

## Troubleshooting

- If the ESP32 won't connect, double-check your WiFi credentials
- If the Python script can't connect to the ESP32, verify the IP address is correct
- For voice input issues, ensure your microphone is working and properly configured
- If OpenAI TTS doesn't work, check your API key and internet connection
- In real-time mode, try speaking more clearly when using activation phrases

## License

This project is released under the MIT License.

## Credits

- Adafruit for the SSD1306 and GFX libraries
- OpenAI for the API and TTS services
- All contributors to this project 