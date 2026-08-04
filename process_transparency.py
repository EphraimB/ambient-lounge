import os
from PIL import Image, ImageOps

def process_transparency(src_path, dst_path, mirror=False):
    img = Image.open(src_path).convert("RGBA")
    if mirror:
        img = ImageOps.mirror(img)

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
    print(f"Processed transparency (mirror={mirror}): {src_path} -> {dst_path}")

if __name__ == "__main__":
    brain_dir = r"C:\Users\emb16\.gemini\antigravity\brain\49acb6b3-cbee-4090-9628-8699a05d8a1f"
    
    # Milo & Sky (Facing Right - 3/4 Profile generated)
    process_transparency(os.path.join(brain_dir, "milo_profile_right_1785801650732.jpg"), "assets/sprites/milo_spritesheet.png", mirror=False)
    process_transparency(os.path.join(brain_dir, "sky_profile_right_1785801658621.jpg"),   "assets/sprites/sky_spritesheet.png",  mirror=False)
    
    # Sam & Frank (Facing Left - Mirrored horizontally on disk)
    process_transparency(os.path.join(brain_dir, "sam_spritesheet_1785800092501.jpg"),   "assets/sprites/sam_spritesheet.png",  mirror=True)
    process_transparency(os.path.join(brain_dir, "frank_spritesheet_1785800084774.jpg"), "assets/sprites/frank_spritesheet.png", mirror=True)
