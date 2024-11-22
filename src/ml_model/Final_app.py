import sys
import os
from PIL import Image, ImageDraw
from ultralytics import YOLO
import numpy as np
 
# Define class names
class_names = {
    0: "Panel", 1: "Transformer", 2: "C.Breaker", 3: "Breaker (Sub-Category)",
    4: "Feeder", 5: "Dis.Switch", 6: "Motor", 7: "Inverter", 8: "ATS", 9: "STS",
    10: "Key Transfer Block", 11: "Fuse", 12: "UPS", 13: "Surge Protective Device (SPD)",
    14: "SwitchBoard", 15: "Arrow", 16: "Generator", 17: "Sub-Transformer (Tx)",
    18: "Bus Terminal", 19: "Feeders Tag", 20: "Manual Transfer Switch",
    21: "Variable Frequency Drive (VFD)"
}
 
def enhance_dpi(image, target_dpi=300):
    scale_factor = target_dpi / 72.0
    new_size = (int(image.width * scale_factor), int(image.height * scale_factor))
    enhanced_image = image.resize(new_size, Image.LANCZOS)
    return enhanced_image
 
def process_image(file_path, model):
    # Load and enhance the image
    img_original = Image.open(file_path)
    img_original = enhance_dpi(img_original, target_dpi=300)
    img_resized = img_original.resize((2560, 1728), Image.LANCZOS)
 
    # Perform object detection
    results = model(img_resized, imgsz=(2560, 1728))
 
    # Extract bounding boxes
    detected_boxes = []
    if results is not None:
        detected_classes = results[0].boxes.cls.cpu().numpy()
        boxes = results[0].boxes.xyxy.cpu().numpy()
        for idx, box in zip(detected_classes, boxes):
            detected_boxes.append((box, int(idx)))
 
    # Draw bounding boxes on the image
    img_with_boxes = img_resized.copy()
    draw = ImageDraw.Draw(img_with_boxes)
    for box, class_idx in detected_boxes:
        draw.rectangle([box[0], box[1], box[2], box[3]], outline="yellow", width=3)
        # label = class_names.get(class_idx, "Unknown")
        # draw.text((box[0], box[1] - 10), label, fill="yellow")
 
    # Save the processed image
    base_name = os.path.splitext(os.path.basename(file_path))[0]
    output_path = f"{base_name}_processed.png"
    img_with_boxes.save(output_path, format="PNG", compress_level=0, quality=100, optimize=True)
 
    return output_path, detected_boxes
 
if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python Final_app.py <image_path>")
        sys.exit(1)
 
    input_image_path = sys.argv[1]
    script_dir = os.path.dirname(os.path.abspath(__file__))

# Construct the path to best.pt
    model_path = os.path.join(script_dir,'best.pt')

# Load the YOLO model
    model = YOLO(model_path)
 
    processed_image_path, detections = process_image(input_image_path, model)
    print(f"Processed image saved at: {processed_image_path}")
 
    # Print detection results
    print("\nDetection Results:")
    detection_counts = {}
    for _, class_idx in detections:
        class_name = class_names.get(class_idx, "Unknown")
        detection_counts[class_name] = detection_counts.get(class_name, 0) + 1
 
    for class_name, count in detection_counts.items():
        print(f"Detected: {count} {class_name}(s)")