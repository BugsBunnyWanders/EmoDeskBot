import cv2
import time
import requests
import threading
import numpy as np
from datetime import datetime

# ESP32 IP address and port - you'll need to change this to your ESP32's IP
ESP32_IP = "192.168.29.240"  # Change this to your ESP32's IP address
ESP32_PORT = 80

# Parameters
WEBCAM_ID = 0  # Usually 0 for built-in webcam
DETECTION_INTERVAL = 0.1  # Seconds between detections
HAPPY_THRESHOLD = 2  # Number of consecutive happy detections before sending signal
NEUTRAL_THRESHOLD = 3  # Number of consecutive non-happy detections before sending neutral signal
SMILE_CONFIDENCE_THRESHOLD = 0.35  # Reduced for better sensitivity at distance
MIN_FACE_SIZE = (60, 60)  # Minimum face size to detect (smaller than before for better distance detection)

# Load the pre-trained models
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')
smile_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_smile.xml')

# Global state
last_state_change = time.time()
current_state = "neutral"
consecutive_happy = 0
consecutive_neutral = 0
last_esp_update = time.time()
esp_update_interval = 0.5  # Reduced from 1.0 to make ESP updates more responsive

# Emotion history for smoother transitions (moving average)
emotion_history = []
HISTORY_SIZE = 5

def calculate_smile_confidence(smiles, face_width, face_height):
    """Calculate a confidence score for smile detection based on size and number of detections"""
    # Fix for numpy array checking - properly check if smiles is empty
    if len(smiles) == 0:
        return 0.0
    
    # Calculate area coverage of smile regions relative to face
    face_area = face_width * face_height
    smile_area_sum = sum(sw * sh for _, _, sw, sh in smiles)
    
    # Area ratio (how much of the face is covered by smiles)
    area_ratio = min(smile_area_sum / face_area, 1.0) * 0.5
    
    # Number of detected smiles factor (more detections = higher confidence)
    # But cap it to avoid too many false positives
    num_smiles_factor = min(len(smiles) / 3.0, 1.0) * 0.5
    
    # Combine factors
    confidence = area_ratio + num_smiles_factor
    
    return confidence

def send_to_esp32(state):
    """Send the facial state to ESP32 via HTTP GET request"""
    global last_esp_update
    
    # Only send updates at a reasonable interval to avoid flooding
    if time.time() - last_esp_update < esp_update_interval:
        return
    
    try:
        url = f"http://{ESP32_IP}:{ESP32_PORT}/face?state={state}"
        response = requests.get(url, timeout=1)
        print(f"Sent {state} to ESP32. Response: {response.status_code}")
        last_esp_update = time.time()
    except Exception as e:
        print(f"Failed to send to ESP32: {e}")

def process_frame_loop():
    """Background thread to process webcam frames and detect emotions"""
    global consecutive_happy, consecutive_neutral, current_state, emotion_history
    
    # Initialize webcam
    cap = cv2.VideoCapture(WEBCAM_ID)
    
    # Set resolution higher for better detection
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    
    if not cap.isOpened():
        print("Error: Could not open webcam")
        return
    
    print("Webcam opened successfully. Starting face detection...")
    
    # Face tracking variables - remember last face position to focus detection
    last_face_region = None
    face_track_frames = 0
    MAX_TRACKING_FRAMES = 10  # How long to focus on last known face position
    
    while True:
        ret, frame = cap.read()
        
        if not ret:
            print("Error: Failed to capture frame")
            break
        
        # Resize for faster processing but maintain aspect ratio
        height, width = frame.shape[:2]
        
        # Convert to grayscale for face detection
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # Improve contrast to help with detection
        gray = cv2.equalizeHist(gray)
        
        # Region of interest for focused face detection
        search_area = gray
        if last_face_region is not None and face_track_frames < MAX_TRACKING_FRAMES:
            # Focus on last known face location with some margin
            x, y, w, h = last_face_region
            margin = min(width, height) // 4  # 25% margin
            
            # Create safe search region with margins
            top = max(0, y - margin)
            bottom = min(height, y + h + margin)
            left = max(0, x - margin)
            right = min(width, x + w + margin)
            
            # Draw focus region for debugging
            cv2.rectangle(frame, (left, top), (right, bottom), (0, 255, 255), 1)
            
            # Set search area to the focused region
            search_area = gray[top:bottom, left:right]
            face_track_frames += 1
        else:
            # Reset tracking if we've exceeded the frame limit
            last_face_region = None
        
        # Detect faces with improved parameters for distance detection
        # Lower scaleFactor and minNeighbors for better detection
        faces = face_cascade.detectMultiScale(
            search_area, 
            scaleFactor=1.08,  # Lower value to detect more faces at different scales
            minNeighbors=3,    # Lower value for less strict detection
            minSize=MIN_FACE_SIZE
        )
        
        # If we're using a focused region, adjust face coordinates
        if last_face_region is not None and face_track_frames < MAX_TRACKING_FRAMES:
            # Adjust detected face coordinates to the full frame
            x_offset = max(0, last_face_region[0] - min(width, height) // 4)
            y_offset = max(0, last_face_region[1] - min(width, height) // 4)
            
            # Adjust all face coordinates back to original frame
            adjusted_faces = []
            for (x, y, w, h) in faces:
                adjusted_faces.append((x + x_offset, y + y_offset, w, h))
            faces = adjusted_faces
        
        # If no faces detected in focused search, try again with full frame
        if len(faces) == 0 and last_face_region is not None:
            faces = face_cascade.detectMultiScale(
                gray,
                scaleFactor=1.1,
                minNeighbors=3,
                minSize=MIN_FACE_SIZE
            )
            # Reset tracking
            last_face_region = None
        
        smile_confidence = 0.0
        
        # If we found faces, reset face tracking
        if len(faces) > 0:
            # Use the largest face for tracking
            largest_face = max(faces, key=lambda f: f[2] * f[3])
            last_face_region = largest_face
            face_track_frames = 0
        
        for (x, y, w, h) in faces:
            # Draw rectangle around the face
            cv2.rectangle(frame, (x, y), (x+w, y+h), (255, 0, 0), 2)
            
            # Region of interest for the face
            roi_gray = gray[y:y+h, x:x+w]
            roi_color = frame[y:y+h, x:x+w]
            
            # Apply bilateral filter to smooth the ROI while preserving edges
            # This helps reduce noise in the image for better smile detection
            roi_gray = cv2.bilateralFilter(roi_gray, 9, 75, 75)
            
            # Distance-adaptive parameters for smile detection
            # Adjust parameters based on face size (smaller faces need more sensitive detection)
            scale_factor = max(1.05, 1.2 - (w * h) / (width * height))
            min_neighbors = max(8, int(12 * (w * h) / (width * height)))
            
            # Lower boundary for smile detection area - focus on bottom half of face
            # Most smiles occur in the lower 60% of the face
            smile_y_min = int(h * 0.4)  # Start at 40% from the top
            smile_region = roi_gray[smile_y_min:, :]
            
            # Draw smile region for debugging
            cv2.rectangle(roi_color, (0, smile_y_min), (w, h), (255, 255, 0), 1)
            
            # Detect smiles with adaptive parameters
            try:
                smiles = smile_cascade.detectMultiScale(
                    smile_region, 
                    scaleFactor=scale_factor,
                    minNeighbors=min_neighbors,
                    minSize=(int(w/12), int(h/20))
                )
                
                # Adjust smile coordinates to the face ROI
                adjusted_smiles = []
                for (sx, sy, sw, sh) in smiles:
                    adjusted_smiles.append((sx, sy + smile_y_min, sw, sh))
                
                # Calculate confidence based on smile detection
                current_confidence = calculate_smile_confidence(adjusted_smiles, w, h)
                smile_confidence = max(smile_confidence, current_confidence)
                
                # Draw rectangles around detected smiles
                for (sx, sy, sw, sh) in adjusted_smiles:
                    cv2.rectangle(roi_color, (sx, sy), (sx+sw, sy+sh), (0, 255, 0), 1)
                    # Display confidence for this particular smile
                    smile_conf_text = f"{sw*sh/(w*h):.2f}"
                    cv2.putText(roi_color, smile_conf_text, (sx, sy-5), 
                               cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)
            except Exception as e:
                print(f"Error in smile detection: {e}")
        
        # Update emotion history for smoothing
        emotion_history.append(smile_confidence)
        if len(emotion_history) > HISTORY_SIZE:
            emotion_history.pop(0)
        
        # Average confidence over recent history
        avg_confidence = sum(emotion_history) / len(emotion_history)
        
        # Update consecutive counters based on confidence threshold
        if avg_confidence >= SMILE_CONFIDENCE_THRESHOLD:
            consecutive_happy += 1
            consecutive_neutral = 0
        else:
            consecutive_neutral += 1
            consecutive_happy = 0
        
        # Determine emotional state
        if consecutive_happy >= HAPPY_THRESHOLD and current_state != "happy":
            current_state = "happy"
            send_to_esp32("happy")
            print(f"{datetime.now().strftime('%H:%M:%S')} - Detected HAPPY face!")
        
        elif consecutive_neutral >= NEUTRAL_THRESHOLD and current_state != "neutral":
            current_state = "neutral"
            send_to_esp32("neutral")
            print(f"{datetime.now().strftime('%H:%M:%S')} - Detected NEUTRAL face")
        
        # Display text showing current state
        cv2.putText(frame, f"State: {current_state.upper()}", (10, 30), 
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
        
        # Add confidence display with color coding
        conf_color = (0, int(255 * avg_confidence), int(255 * (1-avg_confidence)))
        cv2.putText(frame, f"Smile confidence: {avg_confidence:.2f}/{SMILE_CONFIDENCE_THRESHOLD:.2f}", 
                   (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, conf_color, 2)
                   
        cv2.putText(frame, f"Counter: {consecutive_happy}/{HAPPY_THRESHOLD}", 
                   (10, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        
        # Show the number of faces detected
        cv2.putText(frame, f"Faces: {len(faces)}", 
                   (width - 150, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 0, 0), 2)
                   
        # Display the frame
        cv2.imshow('Smile Detector', frame)
        
        # Exit on 'q' key press
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
        
        time.sleep(DETECTION_INTERVAL)
    
    # Release resources
    cap.release()
    cv2.destroyAllWindows()

def main():
    print("Starting EmoDeskBot Smile Detection")
    print(f"Will connect to ESP32 at {ESP32_IP}:{ESP32_PORT}")
    print("Press 'q' in the webcam window to quit")
    
    # Start processing in background thread
    thread = threading.Thread(target=process_frame_loop)
    thread.daemon = True
    thread.start()
    
    # Wait for the thread to finish (or Ctrl+C)
    try:
        while thread.is_alive():
            thread.join(1)
    except KeyboardInterrupt:
        print("\nExiting...")

if __name__ == "__main__":
    main() 