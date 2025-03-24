import os
import time
import json
import requests
from datetime import datetime
import openai
import argparse
import speech_recognition as sr
import pyttsx3  # Fallback TTS engine
import io
import pygame  # For audio playback
import threading
import queue
import tempfile
import wave
from dotenv import load_dotenv

# Load environment variables from .env file
load_dotenv()

# Set up OpenAI API
OPENAI_API_KEY = os.getenv("OPENAI_API_KEY")
if not OPENAI_API_KEY:
    raise ValueError("No OpenAI API key found. Please set it in a .env file or as an environment variable.")

# ESP32 configuration
ESP32_IP = os.getenv("ESP32_IP", "192.168.29.240")  # Default IP or from env
ESP32_PORT = int(os.getenv("ESP32_PORT", "80"))     # Default port or from env

# Global variables for continuous listening
audio_queue = queue.Queue()
continue_listening = True
activation_phrases = ["hey bot", "hey desk bot", "emo", "emodesk", "hi bot", "hello bot"]

# Initialize conversation history
conversation_history = [
    {"role": "system", "content": """You are Emo, an AI powered cute Desk bot that controls a small OLED display on an ESP32 device.
You can show different faces (happy, neutral, sad, angry, grinning, scared) and display simple information.
When asked about time, weather, or other data that would be helpful to display, you should both
answer conversationally AND tell the system to display relevant information on the OLED.
     Also your responses should be short, as cute as possible and funny. Behave like a 10 year old baby.

Example responses:
1. If someone asks if you're happy: "I'm feeling great today! [DISPLAY:happy]"
2. If someone asks for the time: "It's currently 3:45 PM. [DISPLAY:time]"
3. If someone asks if you're upset: "Grrrr, I'm a bit mad right now! [DISPLAY:angry]"
4. If someone shares bad news: "I'm sorry to hear that. [DISPLAY:sad]"
5. If someone says something funny: "Hehe, that's hilarious! [DISPLAY:grinning]"
6. If someone asks about weather: "It's sunny and 72°F today. [DISPLAY:weather]"
7. If someone startles you or mentions something scary: "Eek! That's frightening! [DISPLAY:scared]"

Try to be helpful, friendly, and use the display capabilities when relevant.
"""}
]

def create_temp_directory():
    """Create a temporary directory for audio files"""
    global temp_audio_dir
    temp_audio_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "temp")
    os.makedirs(temp_audio_dir, exist_ok=True)
    print(f"Using temporary directory: {temp_audio_dir}")


def initialize_pygame():
    """Initialize pygame for audio playback"""
    try:
        pygame.mixer.init()
        pygame.mixer.set_num_channels(8)  # Allow multiple sounds to play
        print("Pygame mixer initialized successfully")
    except Exception as e:
        print(f"Warning: Failed to initialize pygame mixer: {e}")


def cleanup_temp_files():
    """Clean up any old temporary files"""
    # Delete existing audio files to avoid permission issues
    try:
        if os.path.exists(temp_audio_dir):
            for file in os.listdir(temp_audio_dir):
                try:
                    file_path = os.path.join(temp_audio_dir, file)
                    if os.path.isfile(file_path) and (file.endswith('.mp3') or file.endswith('.wav')):
                        os.remove(file_path)
                        print(f"Removed old file: {file}")
                except Exception as e:
                    print(f"Warning: Could not remove old file {file}: {e}")
    except Exception as e:
        print(f"Warning: Error cleaning temp directory: {e}")


def initialize_system():
    """Initialize all system components"""
    # Create the temp directory
    create_temp_directory()
    
    # Clean up any old files
    cleanup_temp_files()
    
    # Initialize pygame for audio
    initialize_pygame()
    
    # Initialize the fallback TTS engine
    try:
        global tts_engine
        tts_engine = pyttsx3.init()
        
        # Set fallback voice properties
        voices = tts_engine.getProperty('voices')
        # Try to find a female voice
        for i, voice in enumerate(voices):
            if "female" in voice.name.lower():
                tts_engine.setProperty('voice', voice.id)
                break
        
        # Set rate and volume
        tts_engine.setProperty('rate', 180)    # Speed - words per minute
        tts_engine.setProperty('volume', 0.9)  # Volume (0-1)
        
        print("Local TTS engine initialized")
    except Exception as e:
        print(f"Warning: Failed to initialize local TTS engine: {e}")
    
    # Set up speech recognition
    global recognizer
    recognizer = sr.Recognizer()
    print("Speech recognition initialized")

def speak_with_openai_tts(text, voice="alloy"):
    """
    Use OpenAI's TTS API to convert text to speech and play it
    By making an HTTP request directly to the API endpoint
    """
    if not text:
        return
    
    try:
        # Use path for our output files
        current_file = os.path.join(temp_audio_dir, "current_speech.mp3")
        new_file = os.path.join(temp_audio_dir, "new_speech.mp3")
        
        # API endpoint and headers
        url = "https://api.openai.com/v1/audio/speech"
        headers = {
            "Authorization": f"Bearer {OPENAI_API_KEY}",
            "Content-Type": "application/json"
        }
        
        # Request data
        data = {
            "model": "tts-1",
            "input": text,
            "voice": voice
        }
        
        # Make the POST request to generate speech
        response = requests.post(url, headers=headers, json=data)
        
        # Check if the request was successful
        if response.status_code == 200:
            # Write the audio to the new file first to avoid conflicts
            with open(new_file, 'wb') as f:
                f.write(response.content)
            
            # Play the speech
            try:
                # Initialize pygame mixer
                pygame.mixer.init()
                
                # Load and play the speech
                sound = pygame.mixer.Sound(new_file)
                sound.play()
                
                # Wait for the audio to finish playing
                while pygame.mixer.get_busy():
                    pygame.time.delay(100)  # 100ms delay to reduce CPU usage
                
                # Explicitly unload the sound object after playback
                sound = None
                
                # Stop and quit the mixer to fully release resources
                pygame.mixer.stop()
                pygame.time.delay(200)  # Add a small delay to ensure resources are released
                
                # Try to update the current_speech.mp3 file
                try:
                    # If previous file exists, try to remove it
                    if os.path.exists(current_file):
                        try:
                            os.remove(current_file)
                        except Exception as e:
                            print(f"Note: Could not remove existing file, but audio played successfully: {e}")
                    
                    # Try to rename the new file
                    try:
                        os.rename(new_file, current_file)
                    except Exception as e:
                        print(f"Note: Could not rename file, but audio played successfully: {e}")
                        
                except Exception as e:
                    print(f"Note: File management issue: {e}")
                    # Audio was already played successfully, so not critical
            
            except Exception as e:
                print(f"Error playing audio: {e}")
        else:
            print(f"Error: Received status code {response.status_code} from OpenAI API")
            print(f"Response: {response.text}")
    
    except Exception as e:
        print(f"Error with OpenAI TTS: {e}")
        # Clean up any partial file
        try:
            if os.path.exists(new_file):
                os.remove(new_file)
        except:
            pass

def speak_text(text, use_openai_tts=True, voice="nova"):
    """Speak the given text using either OpenAI TTS or pyttsx3"""
    print(f"Speaking: {text}")
    
    success = False
    if use_openai_tts:
        try:
            speak_with_openai_tts(text, voice)
            success = True
        except Exception as e:
            print(f"OpenAI TTS failed, falling back to local TTS: {e}")
            success = False
    
    # If OpenAI TTS is disabled or failed, use the local TTS engine
    if not use_openai_tts or not success:
        try:
            # Initialize the pyttsx3 engine
            engine = pyttsx3.init()
            engine.say(text)
            engine.runAndWait()
        except Exception as e:
            print(f"Error with local TTS: {e}")

def send_to_esp32(command):
    """Send a command to the ESP32"""
    try:
        url = f"http://{ESP32_IP}:{ESP32_PORT}/face?state={command}"
        response = requests.get(url, timeout=2)
        print(f"Sent '{command}' to ESP32. Response: {response.status_code}")
        return True
    except Exception as e:
        print(f"Failed to communicate with ESP32: {e}")
        return False

def get_weather():
    """Simplified weather function - in a real app, you'd use a weather API"""
    # This is a placeholder. In a real app, you would use a weather API
    return {"condition": "sunny", "temperature": 72}

def get_time():
    """Get current time"""
    now = datetime.now()
    return now.strftime("%I:%M %p")

def process_display_commands(response):
    """Process any display commands in the response"""
    original_response = response
    if "[DISPLAY:" in response:
        # Extract display command
        start_idx = response.find("[DISPLAY:")
        end_idx = response.find("]", start_idx)
        if end_idx > start_idx:
            command = response[start_idx + 9:end_idx].lower()
            
            # Handle different display commands
            if command == "happy":
                send_to_esp32("happy")
            elif command == "sad":
                send_to_esp32("sad")
            elif command == "neutral":
                send_to_esp32("neutral")
            elif command == "angry":
                send_to_esp32("angry")
            elif command == "grinning":
                send_to_esp32("grinning")
            elif command == "scared":
                send_to_esp32("scared")
            elif command == "time":
                current_time = get_time()
                send_to_esp32(f"text:Time: {current_time}")
            elif command == "weather":
                weather = get_weather()
                weather_text = f"{weather['condition'].capitalize()}, {weather['temperature']}°F"
                send_to_esp32(f"text:{weather_text}")
            else:
                # Generic text display
                if command.startswith("text:"):
                    send_to_esp32(command)
                else:
                    print(f"Unknown display command: {command}")
            
            # Remove the command from the response for clean output to user
            response = response[:start_idx] + response[end_idx + 1:]
    
    return response

def listen_for_speech(timeout=5, phrase_time_limit=10):
    """Use speech recognition to convert speech to text"""
    recognizer = sr.Recognizer()
    with sr.Microphone() as source:
        print("Listening...")
        # Keep neutral face instead of showing "Listening..." text
        send_to_esp32("neutral")
        
        recognizer.adjust_for_ambient_noise(source)
        try:
            audio = recognizer.listen(source, timeout=timeout, phrase_time_limit=phrase_time_limit)
            print("Processing speech...")
            # Keep neutral face instead of showing "Processing..." text
            send_to_esp32("neutral")
            
            text = recognizer.recognize_google(audio)
            print(f"You said: {text}")
            return text
        except sr.WaitTimeoutError:
            print("No speech detected")
            send_to_esp32("neutral")
            return None
        except sr.UnknownValueError:
            print("Could not understand audio")
            # send_to_esp32("text:I couldn't hear you")
            return None
        except Exception as e:
            print(f"Error in speech recognition: {e}")
            send_to_esp32("text:Speech error")
            return None

def continuous_listen_worker():
    """Background thread function for continuous listening"""
    global continue_listening
    recognizer = sr.Recognizer()
    
    # Adjust noise threshold once at the beginning
    with sr.Microphone() as source:
        print("Calibrating microphone...")
        recognizer.adjust_for_ambient_noise(source, duration=2)
    
    while continue_listening:
        try:
            with sr.Microphone() as source:
                audio = recognizer.listen(source, phrase_time_limit=5)
                audio_queue.put(audio)
        except Exception as e:
            print(f"Error in continuous listening: {e}")
            time.sleep(1)

def process_audio_chunk_with_openai(audio_data, args):
    """Process a chunk of audio data using OpenAI's real-time audio endpoint"""
    try:
        # Create temp files with different names to avoid conflicts
        temp_wav = os.path.join(temp_audio_dir, "current_recording.wav")
        temp_wav_new = os.path.join(temp_audio_dir, "new_recording.wav")
            
        # Write audio data to the new WAV file
        with wave.open(temp_wav_new, 'wb') as wf:
            wf.setnchannels(1)  # Mono
            wf.setsampwidth(2)  # 16-bit
            wf.setframerate(16000)  # 16kHz
            wf.writeframes(audio_data.get_wav_data())
            
        # Send the WAV file to OpenAI's real-time transcription endpoint
        url = "https://api.openai.com/v1/audio/transcriptions"
        headers = {
            "Authorization": f"Bearer {OPENAI_API_KEY}"
        }
        
        with open(temp_wav_new, 'rb') as audio_file:
            files = {
                'file': (os.path.basename(temp_wav_new), audio_file, 'audio/wav'),
                'model': (None, 'whisper-1')
            }
            response = requests.post(url, headers=headers, files=files)
        
        # After successful processing, try to clean up/rename files
        try:
            # Try to clean up the old file
            if os.path.exists(temp_wav):
                try:
                    os.remove(temp_wav)
                except Exception as e:
                    print(f"Note: Could not remove previous WAV file, but it's safe to continue: {e}")
            
            # Rename the new file to the standard name
            try:
                os.rename(temp_wav_new, temp_wav)
            except Exception as e:
                print(f"Note: Could not rename WAV file, but processing was successful: {e}")
        except Exception as e:
            print(f"Note: File cleanup issue: {e}")
            
        if response.status_code != 200:
            print(f"Error: Received status code {response.status_code} from OpenAI API")
            print(f"Response: {response.text}")
            return None
            
        # Extract the transcribed text
        response_json = response.json()
        transcribed_text = response_json.get('text', '').lower()
        
        if transcribed_text:
            print(f"Heard: {transcribed_text}")
            
            # Check if the text contains any activation phrase
            activated = False
            for phrase in activation_phrases:
                if phrase in transcribed_text:
                    activated = True
                    # Remove the activation phrase from the text
                    transcribed_text = transcribed_text.replace(phrase, "").strip()
                    break
            
            if activated or args.always_listen:
                # If activated or we're in always listen mode, process the command
                if transcribed_text:
                    # User said something after the activation phrase
                    return transcribed_text
                else:
                    # Just the activation phrase - indicate we're ready
                    # Keep neutral face instead of showing happy face
                    send_to_esp32("neutral")
                    speak_text("I'm listening", not args.fallback_tts, args.voice)
                    
                    # Get the follow-up command
                    follow_up = listen_for_speech(timeout=5, phrase_time_limit=10)
                    return follow_up
        
        return None
        
    except Exception as e:
        print(f"Error processing audio with OpenAI: {e}")
        # Clean up any partial file
        try:
            if 'temp_wav_new' in locals() and os.path.exists(temp_wav_new):
                os.remove(temp_wav_new)
        except:
            pass
        return None

def process_audio(recognizer, audio, args):
    """Process audio data and return the transcribed text"""
    try:
        # Use OpenAI's API for speech recognition if specified
        if args.use_openai_speech:
            # Show neutral face while processing
            send_to_esp32("neutral")
            text = process_audio_chunk_with_openai(audio, args)
            return text
        
        # Otherwise use Google Speech Recognition
        text = recognizer.recognize_google(audio).lower()
        print(f"Heard: {text}")
        
        # Check if the text contains any activation phrase
        activated = False
        for phrase in activation_phrases:
            if phrase in text:
                activated = True
                # Remove the activation phrase from the text
                text = text.replace(phrase, "").strip()
                break
        
        if activated or args.always_listen:
            # If activated or we're in always listen mode, process the command
            if text:
                # User said something after the activation phrase
                return text
            else:
                # Just the activation phrase - indicate we're ready
                # Keep neutral face
                send_to_esp32("neutral")
                speak_text("I'm listening", not args.fallback_tts, args.voice)
                
                # Get the follow-up command
                follow_up = listen_for_speech(timeout=5, phrase_time_limit=10)
                return follow_up
        
        return None
    except sr.UnknownValueError:
        print("Could not understand audio")
        return None
    except sr.RequestError as e:
        print(f"Could not request results; {e}")
        return None
    except Exception as e:
        print(f"Error processing audio: {e}")
        return None

def chat_with_ai_stream(user_input):
    """Stream response from OpenAI in real-time for faster feedback"""
    # Add user message to conversation history
    conversation_history.append({"role": "user", "content": user_input})
    
    try:
        # Make a direct HTTP request to the OpenAI Chat API with streaming
        url = "https://api.openai.com/v1/chat/completions"
        headers = {
            "Authorization": f"Bearer {OPENAI_API_KEY}",
            "Content-Type": "application/json"
        }
        data = {
            "model": "gpt-4o",
            "messages": conversation_history,
            "max_tokens": 150,
            "stream": True  # Enable streaming
        }
        
        # Keep neutral face instead of showing "Thinking..." text
        send_to_esp32("neutral")
        
        # Initialize storage for streamed response
        full_response = ""
        buffer = ""
        display_cmd = None
        
        # Send the request and process the stream
        with requests.post(url, headers=headers, json=data, stream=True) as response:
            if response.status_code != 200:
                print(f"Error: Received status code {response.status_code} from OpenAI API")
                print(f"Response: {response.text}")
                return "Sorry, I encountered an error while processing your request."
                
            # Process the streaming response
            for line in response.iter_lines():
                if line:
                    # Filter out keep-alive new lines
                    line = line.decode('utf-8')
                    
                    # Format: data: {json}
                    if line.startswith("data:"):
                        if line.strip() == "data: [DONE]":
                            break
                            
                        try:
                            json_str = line[5:].strip()  # Remove 'data: ' prefix
                            chunk = json.loads(json_str)
                            
                            # Extract the content delta if present
                            delta_content = chunk.get("choices", [{}])[0].get("delta", {}).get("content", "")
                            if delta_content:
                                full_response += delta_content
                                buffer += delta_content
                                
                                # Check for display commands as they come in
                                if "[DISPLAY:" in buffer and "]" in buffer and not display_cmd:
                                    start_idx = buffer.find("[DISPLAY:")
                                    end_idx = buffer.find("]", start_idx)
                                    if end_idx > start_idx:
                                        display_cmd = buffer[start_idx + 9:end_idx].lower()
                                        
                                        # Handle the display command
                                        if display_cmd == "happy":
                                            send_to_esp32("happy")
                                        elif display_cmd == "sad":
                                            send_to_esp32("sad")
                                        elif display_cmd == "neutral":
                                            send_to_esp32("neutral") 
                                        elif display_cmd == "angry":
                                            send_to_esp32("angry")
                                        elif display_cmd == "grinning":
                                            send_to_esp32("grinning")
                                        elif display_cmd == "scared":
                                            send_to_esp32("scared")
                                        elif display_cmd == "time":
                                            current_time = get_time()
                                            send_to_esp32(f"text:Time: {current_time}")
                                        elif display_cmd == "weather":
                                            weather = get_weather()
                                            weather_text = f"{weather['condition'].capitalize()}, {weather['temperature']}°F"
                                            send_to_esp32(f"text:{weather_text}")
                                        else:
                                            # Generic text display
                                            if display_cmd.startswith("text:"):
                                                send_to_esp32(display_cmd)
                                
                                # Print partial responses for terminal feedback
                                print(delta_content, end="", flush=True)
                                
                        except json.JSONDecodeError:
                            print(f"Error parsing JSON from stream: {line}")
                            continue
        
        print()  # Newline after streaming finishes
        
        # Add AI response to conversation history
        conversation_history.append({"role": "assistant", "content": full_response})
        
        # Process the final display command if needed
        cleaned_response = process_display_commands(full_response)
        
        return cleaned_response
    
    except Exception as e:
        print(f"Error communicating with OpenAI: {e}")
        return "Sorry, I encountered an error while processing your request."

def chat_with_ai(user_input):
    """Send user input to OpenAI and get a response"""
    # Add user message to conversation history
    conversation_history.append({"role": "user", "content": user_input})
    
    try:
        # Make a direct HTTP request to the OpenAI Chat API
        url = "https://api.openai.com/v1/chat/completions"
        headers = {
            "Authorization": f"Bearer {OPENAI_API_KEY}",
            "Content-Type": "application/json"
        }
        data = {
            "model": "gpt-3.5-turbo",
            "messages": conversation_history,
            "max_tokens": 150
        }
        
        response = requests.post(url, headers=headers, json=data)
        
        if response.status_code != 200:
            print(f"Error: Received status code {response.status_code} from OpenAI API")
            print(f"Response: {response.text}")
            return "Sorry, I encountered an error while processing your request."
        
        # Extract the response text
        response_json = response.json()
        ai_response = response_json["choices"][0]["message"]["content"]
        
        # Add AI response to conversation history
        conversation_history.append({"role": "assistant", "content": ai_response})
        
        # Process any display commands
        cleaned_response = process_display_commands(ai_response)
        
        return cleaned_response
    
    except Exception as e:
        print(f"Error communicating with OpenAI: {e}")
        return "Sorry, I encountered an error while processing your request."

def main():
    """Main function to run the AI desk bot"""
    # Process command line arguments
    parser = argparse.ArgumentParser(description='AI Desk Bot')
    parser.add_argument('--text', action='store_true', help='Use text input instead of voice')
    parser.add_argument('--mute', action='store_true', help='Mute the bot\'s voice responses')
    parser.add_argument('--fallback-tts', action='store_true', help='Use pyttsx3 instead of OpenAI TTS')
    parser.add_argument('--activation-only', action='store_true', help='Only process speech with activation phrases (default: process all speech)')
    parser.add_argument('--realtime', action='store_true', help='Use real-time audio streaming')
    parser.add_argument('--use-openai-speech', action='store_true', help='Use OpenAI\'s API for speech recognition') 
    parser.add_argument('--stream', action='store_true', help='Stream AI responses in real-time')
    parser.add_argument('--voice', type=str, default='nova', choices=['alloy', 'echo', 'fable', 'onyx', 'nova', 'shimmer'], 
                        help='Voice to use for OpenAI TTS (default: nova)')
    
    args = parser.parse_args()
    
    # Set always_listen to True by default (opposite of activation-only flag)
    args.always_listen = not args.activation_only
    
    # Initialize all system components
    initialize_system()
    
    # Display and announce welcome message
    print("Welcome to the AI Desk Bot!")
    send_to_esp32("happy")
    
    if not args.mute:
        speak_text("Welcome to the AI Desk Bot! How can I help you today?", not args.fallback_tts, args.voice)
    
    print("Say 'quit', 'exit', or 'bye' to exit.")
    if not args.text and args.realtime:
        if args.activation_only:
            print(f"Say one of these phrases to activate me: {', '.join(activation_phrases)}")
        else:
            print("Listening continuously for any speech...")
    
    # Start continuous listening if realtime mode is enabled
    if not args.text and args.realtime:
        global continue_listening, audio_queue
        
        # Start the listening thread
        listen_thread = threading.Thread(target=continuous_listen_worker)
        listen_thread.daemon = True
        listen_thread.start()
        
        # Keep neutral face instead of showing "Ready! Say 'Hey Bot'" text
        send_to_esp32("neutral")
        
        try:
            while True:
                try:
                    # Get audio from the queue with a timeout
                    audio = audio_queue.get(timeout=0.5)
                    
                    # Process the audio
                    user_input = process_audio(recognizer, audio, args)
                    
                    if user_input:
                        # Check for exit commands
                        if user_input.lower() in ['quit', 'exit', 'bye']:
                            goodbye = "Goodbye! Have a great day!"
                            print(f"AI: {goodbye}")
                            if not args.mute:
                                speak_text(goodbye, not args.fallback_tts, args.voice)
                            break
                        
                        # Get AI response - use streaming if enabled
                        if args.stream:
                            ai_response = chat_with_ai_stream(user_input)
                        else:
                            ai_response = chat_with_ai(user_input)
                        
                        # Display response
                        print(f"AI: {ai_response}")
                        
                        # Speak the response
                        if not args.mute:
                            speak_text(ai_response, not args.fallback_tts, args.voice)

                        # Return to ready state
                        send_to_esp32("neutral")
                
                except queue.Empty:
                    # No audio data available, continue loop
                    pass
        
        finally:
            # Stop the listening thread
            continue_listening = False
            if listen_thread.is_alive():
                listen_thread.join(timeout=2)
    
    else:
        # Traditional input mode
        while True:
            if not args.text:  # Voice is the default
                user_input = listen_for_speech()
                if not user_input:
                    continue
            else:
                user_input = input("You: ")
            
            if user_input.lower() in ['quit', 'exit', 'bye']:
                goodbye = "Goodbye! Have a great day!"
                print(f"AI: {goodbye}")
                if not args.mute:
                    speak_text(goodbye, not args.fallback_tts, args.voice)
                break
            
            # Get AI response - use streaming if enabled
            if args.stream:
                ai_response = chat_with_ai_stream(user_input)
            else:
                ai_response = chat_with_ai(user_input)
            
            # Display response
            print(f"AI: {ai_response}")
            
            # Speak the response
            if not args.mute:
                speak_text(ai_response, not args.fallback_tts, args.voice)
            
            # Return to neutral face
            send_to_esp32("neutral")

if __name__ == "__main__":
    main() 