import os
from PIL import Image

def process_transparency(src_path, dst_path):
    img = Image.open(src_path).convert("RGBA")
    datas = img.getdata()

    new_data = []
    for item in datas:
        # If pixel is near white (R > 235, G > 235, B > 235), make it transparent
        if item[0] > 235 and item[1] > 235 and item[2] > 235:
            new_data.append((255, 255, 255, 0))
        else:
            new_data.append(item)

    img.putdata(new_data)
    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    img.save(dst_path, "PNG")
    print(f"Processed transparency: {src_path} -> {dst_path}")

if __name__ == "__main__":
    brain_dir = r"C:\Users\emb16\.gemini\antigravity\brain\49acb6b3-cbee-4090-9628-8699a05d8a1f"
    
    process_transparency(os.path.join(brain_dir, "milo_spritesheet_1785800077354.jpg"), "assets/sprites/milo_spritesheet.png")
    process_transparency(os.path.join(brain_dir, "frank_spritesheet_1785800084774.jpg"), "assets/sprites/frank_spritesheet.png")
    process_transparency(os.path.join(brain_dir, "sam_spritesheet_1785800092501.jpg"),   "assets/sprites/sam_spritesheet.png")
    process_transparency(os.path.join(brain_dir, "sky_spritesheet_1785800101998.jpg"),   "assets/sprites/sky_spritesheet.png")
