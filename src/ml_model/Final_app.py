import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageTk, ImageDraw, UnidentifiedImageError
from ultralytics import YOLO
import numpy as np

# Define class names manually
# Define the new class names
class_names = {
    0: "Panel",
    1: "Transformer",
    2: "C.Breaker",
    3: "Breaker (Sub-Category)",
    4: "Feeder",
    5: "Dis.Switch",
    6: "Motor",
    7: "Inverter",
    8: "ATS",
    9: "STS",
    10: "Key Transfer Block",
    11: "Fuse",
    12: "UPS",
    13: "Surge Protective Device (SPD)",
    14: "SwitchBoard",
    15: "Arrow",
    16: "Generator",
    17: "Sub-Transformer (Tx)",
    18: "Bus Terminal",
    19: "Feeders Tag",
    20: "Manual Transfer Switch",
    21: "Variable Frequency Drive (VFD)"
}

# Initialize global variables
current_image_path = None
img_original = None
img_resized = None
results = None
zoom_level = 1.0
zoom_step = 0.1
original_img_size = None
detected_boxes = []  # Store detected boxes for scaling during zoom
drag_data = {"x": 0, "y": 0}  # Store dragging information
image_id = None  # Store canvas image ID for positioning

# Function to open the file dialog and select an image
def browse_image():
    global current_image_path
    file_path = filedialog.askopenfilename(
        filetypes=[("Image files", "*.jpg *.jpeg *.png *.bmp *.png")]
    )
    if file_path:
        try:
            current_image_path = file_path
            load_image(file_path)
            listbox.delete(0, tk.END)
        except UnidentifiedImageError:
            messagebox.showerror("Error", f"Cannot identify image file '{file_path}'")

# Function to load and enhance the DPI of the selected image
def load_image(file_path):
    global img_original, original_img_size, zoom_level
    # Load the image
    img_original = Image.open(file_path)
    
    # Enhance the DPI by resizing the image
    img_original = enhance_dpi(img_original, target_dpi=300)
    
    original_img_size = img_original.size  # Store the original size of the image
    zoom_level = 1.0  # Reset zoom level when a new image is loaded
    display_image()  # Display the image without any bounding boxes

# Function to enhance the DPI of an image
def enhance_dpi(image, target_dpi=300):
    # Calculate the new size based on the target DPI (current DPI is assumed to be 72)
    scale_factor = target_dpi / 72.0
    new_size = (int(image.width * scale_factor), int(image.height * scale_factor))
    
    # Resize the image to the new size for better clarity
    enhanced_image = image.resize(new_size, Image.LANCZOS)
    print(f"Enhanced image size: {enhanced_image.size}, DPI: {target_dpi}")
    return enhanced_image

# Function to display the image with bounding boxes according to the current zoom level
def display_image():
    global img_resized, image_id
    if not img_original:
        return

    # Resize the image to a fixed size for detection
    img_resized = img_original.resize((2560, 1728), Image.LANCZOS)
    img_tk = ImageTk.PhotoImage(img_resized)
    if image_id:
        image_canvas.delete(image_id)
    image_id = image_canvas.create_image(0, 0, anchor=tk.NW, image=img_tk)
    image_canvas.image = img_tk
    image_canvas.config(scrollregion=image_canvas.bbox(tk.ALL))

# Function to display the image with bounding boxes according to the current zoom level
def display_image_with_boxes():
    global img_resized, detected_boxes, image_id
    if not img_original or not img_resized:
        return

    # Create a copy of the resized image for drawing
    img_with_boxes = img_resized.copy()
    draw = ImageDraw.Draw(img_with_boxes)

    # Draw detected bounding boxes
    for box in detected_boxes:
        draw.rectangle([box[0], box[1], box[2], box[3]], outline="yellow", width=3)

    # Convert the image with boxes to a PhotoImage and display it
    img_tk_with_boxes = ImageTk.PhotoImage(img_with_boxes)
    if image_id:
        image_canvas.delete(image_id)
    image_id = image_canvas.create_image(0, 0, anchor=tk.NW, image=img_tk_with_boxes)
    image_canvas.image = img_tk_with_boxes
    image_canvas.config(scrollregion=image_canvas.bbox(tk.ALL))

# Function to perform object detection using YOLOv8 and selected classes
def detect_objects(file_path):
    global results, detected_boxes
    # Use the resized image for YOLO detection
    img_for_yolo = img_original.resize((2560, 1728), Image.LANCZOS)

    # Get the selected classes' indices
    selected_classes_indices = [i for i, var in enumerate(checkbox_vars) if var.get()]

    # Run inference with YOLOv8
    results = model(img_for_yolo, imgsz=(2560, 1728), classes=selected_classes_indices)

    # Extract bounding boxes from the results
    detected_boxes = []
    if results is not None:
        detected_classes = results[0].boxes.cls.cpu().numpy()
        boxes = results[0].boxes.xyxy.cpu().numpy()

        # Clear the listbox and display the results
        listbox.delete(0, tk.END)
        detections_count = {}

        for idx, box in zip(detected_classes, boxes):
            class_name = class_names.get(int(idx), "Unknown")
            detected_boxes.append(box)

            # Count detections by class
            detections_count[class_name] = detections_count.get(class_name, 0) + 1

        # Update the listbox with the detection counts
        if detections_count:
            for class_name, count in detections_count.items():
                listbox.insert(tk.END, f"Detected: {count} {class_name}(s)")
        else:
            listbox.insert(tk.END, "No detections found")

    # Display the image with the bounding boxes
    display_image_with_boxes()

# Function to trigger detection based on selected classes
def run_detection():
    if current_image_path is None:
        messagebox.showwarning("No Image Selected", "Please select an image first.")
    else:
        detect_objects(current_image_path)

# Function to zoom in
def zoom_in():
    global zoom_level
    zoom_level = min(zoom_level + zoom_step, 3.0)  # Limit zoom in level to 3.0
    display_zoomed_image()

# Function to zoom out
def zoom_out():
    global zoom_level
    zoom_level = max(zoom_level - zoom_step, 0.5)  # Limit zoom out level to 0.5
    display_zoomed_image()

# Function to display the zoomed image with boxes
def display_zoomed_image():
    global img_original, detected_boxes, zoom_level, image_id
    if not img_original:
        return

    # Resize the image according to the zoom level
    new_size = (int(2560 * zoom_level), int(1728 * zoom_level))
    img_zoomed = img_original.resize(new_size, Image.LANCZOS)

    # Create a new image draw object for drawing bounding boxes
    draw = ImageDraw.Draw(img_zoomed)
    
    # Adjust bounding boxes based on the zoom level
    scale_x = zoom_level
    scale_y = zoom_level
    for box in detected_boxes:
        x1, y1, x2, y2 = [int(coord * scale_x) for coord in box]
        draw.rectangle([x1, y1, x2, y2], outline="yellow", width=3)

    # Convert the zoomed image to a PhotoImage and update the label
    img_tk_with_boxes = ImageTk.PhotoImage(img_zoomed)
    if image_id:
        image_canvas.delete(image_id)
    image_id = image_canvas.create_image(0, 0, anchor=tk.NW, image=img_tk_with_boxes)
    image_canvas.image = img_tk_with_boxes
    image_canvas.config(scrollregion=image_canvas.bbox(tk.ALL))

# Function to handle mouse scroll for zooming
def on_mouse_wheel(event):
    if event.delta > 0:  # Scroll up to zoom in
        zoom_in()
    else:  # Scroll down to zoom out
        zoom_out()

# Function to start dragging
def start_drag(event):
    drag_data["x"] = event.x
    drag_data["y"] = event.y

# Function to perform dragging
def do_drag(event):
    delta_x = event.x - drag_data["x"]
    delta_y = event.y - drag_data["y"]
    image_canvas.move(image_id, delta_x, delta_y)
    drag_data["x"] = event.x
    drag_data["y"] = event.y

# Set up the YOLOv8 model
model_path = r'C:\Users\Sainath\Desktop\Nov_18\sumatrapdf\src\ml_model\best.pt'
model = YOLO(model_path)

# Set up the GUI
root = tk.Tk()
root.title("YOLOv8 Object Detection with Zoom and Drag")
root.geometry("1400x800")

# Create frames for the layout
frame_left = tk.Frame(root)
frame_left.pack(side=tk.LEFT, fill=tk.Y, padx=10, pady=10)

frame_right = tk.Frame(root)
frame_right.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=10, pady=10)

# Create a canvas to display the image
image_canvas = tk.Canvas(frame_right, bg="white", width=800, height=600)
image_canvas.grid(row=0, column=0, sticky="nsew")

# Configure the frame to expand with the window
frame_right.grid_rowconfigure(0, weight=1)
frame_right.grid_columnconfigure(0, weight=1)

# Bind mouse wheel event to zoom
image_canvas.bind("<MouseWheel>", on_mouse_wheel)
image_canvas.bind("<Button-4>", lambda event: zoom_in())  # For Linux systems (mouse wheel up)
image_canvas.bind("<Button-5>", lambda event: zoom_out())  # For Linux systems (mouse wheel down)

# Bind dragging events
image_canvas.bind("<ButtonPress-1>", start_drag)
image_canvas.bind("<B1-Motion>", do_drag)

# Button to browse for an image
button_browse = tk.Button(frame_left, text="Browse Image", command=browse_image)
button_browse.pack(pady=10, fill=tk.X)

# Checkboxes for each class
checkbox_vars = []
for i in range(len(class_names)):
    var = tk.BooleanVar(value=True)
    checkbox = tk.Checkbutton(frame_left, text=f"{i}: {class_names[i]}", variable=var)
    checkbox.pack(anchor='w', padx=5)
    checkbox_vars.append(var)

# Button to run detection
button_detect = tk.Button(frame_left, text="Run Detection", command=run_detection)
button_detect.pack(pady=10, fill=tk.X)

# Listbox to display detected labels
listbox = tk.Listbox(frame_left, width=30, height=10)
listbox.pack(pady=10, fill=tk.X)

# Add buttons for zoom in and zoom out
button_zoom_in = tk.Button(frame_left, text="Zoom In", command=zoom_in)
button_zoom_in.pack(pady=5, fill=tk.X)

button_zoom_out = tk.Button(frame_left, text="Zoom Out", command=zoom_out)
button_zoom_out.pack(pady=5, fill=tk.X)

# Start the Tkinter main loop
root.mainloop()
