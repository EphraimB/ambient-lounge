import os
import math
from PIL import Image, ImageDraw, ImageFilter

def draw_character_frame(draw, width, height, char_type, frame_idx):
    # Colors for each character
    colors = {
        'frank': {'hoodie': (255, 105, 180, 255), 'skin': (245, 205, 175, 255), 'hair': (50, 40, 35, 255), 'accent': (255, 160, 210, 255)},
        'milo':  {'hoodie': (64, 156, 255, 255),  'skin': (240, 200, 170, 255), 'hair': (230, 200, 70, 255), 'accent': (120, 190, 255, 255)},
        'sam':   {'hoodie': (46, 204, 113, 255),  'skin': (245, 210, 180, 255), 'hair': (80, 50, 35, 255),   'accent': (100, 230, 150, 255)},
        'sky':   {'hoodie': (255, 191, 0, 255),   'skin': (235, 195, 165, 255), 'hair': (30, 30, 35, 255),   'accent': (255, 220, 100, 255)}
    }
    
    c = colors[char_type]
    
    # Animation offsets based on 3x3 frame index (0..8)
    breath_y = math.sin(frame_idx * 0.7) * 4.0
    talk_mouth = (frame_idx % 2 == 1)
    arm_angle = math.sin(frame_idx * 0.9) * 12.0
    
    cx = width / 2.0
    cy = height / 2.0 + breath_y

    # Outer glow / aura shadow
    draw.ellipse([cx - 75, cy + 50, cx + 75, cy + 90], fill=(0, 0, 0, 40))

    # Body / Hoodie
    body_box = [cx - 55, cy - 10, cx + 55, cy + 85]
    draw.rounded_rectangle(body_box, radius=20, fill=c['hoodie'])
    draw.rounded_rectangle([cx - 45, cy, cx + 45, cy + 75], radius=15, fill=c['accent'])

    # Head
    head_box = [cx - 42, cy - 85, cx + 42, cy - 5]
    draw.ellipse(head_box, fill=c['skin'])

    # Hair
    hair_box = [cx - 45, cy - 95, cx + 45, cy - 45]
    draw.chord(hair_box, start=180, end=360, fill=c['hair'])

    # Eyes
    eye_offset = math.sin(frame_idx * 0.5) * 3.0
    draw.ellipse([cx - 20 + eye_offset, cy - 50, cx - 8 + eye_offset, cy - 35], fill=(30, 30, 40, 255))
    draw.ellipse([cx + 8 + eye_offset, cy - 50, cx + 20 + eye_offset, cy - 35], fill=(30, 30, 40, 255))
    draw.ellipse([cx - 16 + eye_offset, cy - 47, cx - 12 + eye_offset, cy - 42], fill=(255, 255, 255, 255))
    draw.ellipse([cx + 12 + eye_offset, cy - 47, cx + 16 + eye_offset, cy - 42], fill=(255, 255, 255, 255))

    # Mouth (talk animation)
    if talk_mouth:
        draw.ellipse([cx - 10, cy - 28, cx + 10, cy - 16], fill=(200, 60, 80, 255))
    else:
        draw.arc([cx - 12, cy - 30, cx + 12, cy - 20], start=0, end=180, fill=(120, 50, 40, 255), width=3)

    # Arm gestures
    r_arm_y = cy + 15 + math.sin(frame_idx * 0.8) * 8.0
    draw.line([cx + 45, cy + 10, cx + 70, r_arm_y], fill=c['hoodie'], width=14)
    draw.ellipse([cx + 62, r_arm_y - 8, cx + 78, r_arm_y + 8], fill=c['skin'])

    l_arm_y = cy + 15 - math.sin(frame_idx * 0.8) * 8.0
    draw.line([cx - 45, cy + 10, cx - 70, l_arm_y], fill=c['hoodie'], width=14)
    draw.ellipse([cx - 78, l_arm_y - 8, cx - 62, l_arm_y + 8], fill=c['skin'])

def generate_sprite_sheet(char_type, output_path):
    frame_w, frame_h = 256, 256
    sheet_w, sheet_h = frame_w * 3, frame_h * 3
    
    sheet_img = Image.new("RGBA", (sheet_w, sheet_h), (0, 0, 0, 0))
    
    for row in range(3):
        for col in range(3):
            frame_idx = row * 3 + col
            frame_img = Image.new("RGBA", (frame_w, frame_h), (0, 0, 0, 0))
            draw = ImageDraw.Draw(frame_img)
            draw_character_frame(draw, frame_w, frame_h, char_type, frame_idx)
            
            sheet_img.paste(frame_img, (col * frame_w, row * frame_h))
            
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    sheet_img.save(output_path, format="PNG")
    print(f"Generated sprite sheet: {output_path} ({sheet_w}x{sheet_h})")

if __name__ == "__main__":
    generate_sprite_sheet("frank", "assets/sprites/frank_sheet.png")
    generate_sprite_sheet("milo",  "assets/sprites/milo_sheet.png")
    generate_sprite_sheet("sam",   "assets/sprites/sam_sheet.png")
    generate_sprite_sheet("sky",   "assets/sprites/sky_sheet.png")
