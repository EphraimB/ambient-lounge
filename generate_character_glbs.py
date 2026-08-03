import json
import struct
import math
import os

def create_icosphere(subdivisions=2):
    # Base icosahedron
    t = (1.0 + math.sqrt(5.0)) / 2.0
    verts = [
        (-1,  t,  0), ( 1,  t,  0), (-1, -t,  0), ( 1, -t,  0),
        ( 0, -1,  t), ( 0,  1,  t), ( 0, -1, -t), ( 0,  1, -t),
        ( t,  0, -1), ( t,  0,  1), (-t,  0, -1), (-t,  0,  1)
    ]
    # Normalize vertices
    verts = [[v[0]/math.sqrt(v[0]**2+v[1]**2+v[2]**2),
              v[1]/math.sqrt(v[0]**2+v[1]**2+v[2]**2),
              v[2]/math.sqrt(v[0]**2+v[1]**2+v[2]**2)] for v in verts]
    
    faces = [
        (0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
        (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
        (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
        (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)
    ]

    midpoint_cache = {}
    def get_midpoint(p1, p2):
        key = tuple(sorted((p1, p2)))
        if key in midpoint_cache:
            return midpoint_cache[key]
        v1, v2 = verts[p1], verts[p2]
        mid = [(v1[0]+v2[0])/2, (v1[1]+v2[1])/2, (v1[2]+v2[2])/2]
        length = math.sqrt(mid[0]**2 + mid[1]**2 + mid[2]**2)
        mid = [mid[0]/length, mid[1]/length, mid[2]/length]
        verts.append(mid)
        idx = len(verts) - 1
        midpoint_cache[key] = idx
        return idx

    for _ in range(subdivisions):
        new_faces = []
        for tri in faces:
            a = get_midpoint(tri[0], tri[1])
            b = get_midpoint(tri[1], tri[2])
            c = get_midpoint(tri[2], tri[0])
            new_faces.extend([
                (tri[0], a, c),
                (tri[1], b, a),
                (tri[2], c, b),
                (a, b, c)
            ])
        faces = new_faces

    return verts, faces

def create_cylinder(radius=0.4, height=1.2, slices=16):
    verts = []
    faces = []
    half_h = height / 2.0
    
    # Top and bottom center
    top_center = [0.0, half_h, 0.0]
    bot_center = [0.0, -half_h, 0.0]
    verts.append(top_center) # 0
    verts.append(bot_center) # 1

    # Ring vertices
    for i in range(slices):
        angle = (2.0 * math.pi * i) / slices
        x = radius * math.cos(angle)
        z = radius * math.sin(angle)
        verts.append([x, half_h, z])  # Top ring: 2 + i*2
        verts.append([x, -half_h, z]) # Bot ring: 3 + i*2

    for i in range(slices):
        next_i = (i + 1) % slices
        top1 = 2 + i * 2
        bot1 = 3 + i * 2
        top2 = 2 + next_i * 2
        bot2 = 3 + next_i * 2

        # Side quad (2 tris)
        faces.append((top1, bot1, top2))
        faces.append((top2, bot1, bot2))

        # Top cap
        faces.append((0, top2, top1))
        # Bot cap
        faces.append((1, bot1, bot2))

    return verts, faces

def build_character_mesh(torso_color_rgb, hair_color_rgb):
    all_positions = []
    all_normals = []
    all_indices = []

    def add_geom(verts, faces, offset_xyz, scale_xyz):
        base_idx = len(all_positions) // 3
        for v in verts:
            x = v[0] * scale_xyz[0] + offset_xyz[0]
            y = v[1] * scale_xyz[1] + offset_xyz[1]
            z = v[2] * scale_xyz[2] + offset_xyz[2]
            all_positions.extend([x, y, z])
            
            # Simple spherical/radial normal calculation
            length = math.sqrt(v[0]**2 + v[1]**2 + v[2]**2) or 1.0
            all_normals.extend([v[0]/length, v[1]/length, v[2]/length])

        for tri in faces:
            all_indices.extend([base_idx + tri[0], base_idx + tri[1], base_idx + tri[2]])

    # 1. Head (Sphere at y = 1.3)
    head_verts, head_faces = create_icosphere(subdivisions=2)
    add_geom(head_verts, head_faces, (0.0, 1.35, 0.0), (0.38, 0.42, 0.38))

    # 2. Hair (Sphere cap at y = 1.55)
    add_geom(head_verts, head_faces, (0.0, 1.52, 0.0), (0.41, 0.28, 0.41))

    # 3. Torso (Cylinder at y = 0.4)
    torso_verts, torso_faces = create_cylinder(radius=0.45, height=1.1, slices=16)
    add_geom(torso_verts, torso_faces, (0.0, 0.4, 0.0), (1.0, 1.0, 0.8))

    # 4. Left Arm (Cylinder at x = -0.55, y = 0.4)
    arm_verts, arm_faces = create_cylinder(radius=0.14, height=0.9, slices=12)
    add_geom(arm_verts, arm_faces, (-0.58, 0.4, 0.0), (1.0, 1.0, 1.0))

    # 5. Right Arm (Cylinder at x = 0.55, y = 0.4)
    add_geom(arm_verts, arm_faces, (0.58, 0.4, 0.0), (1.0, 1.0, 1.0))

    # 6. Left Leg (Cylinder at x = -0.22, y = -0.6)
    leg_verts, leg_faces = create_cylinder(radius=0.16, height=0.9, slices=12)
    add_geom(leg_verts, leg_faces, (-0.22, -0.6, 0.0), (1.0, 1.0, 1.0))

    # 7. Right Leg (Cylinder at x = 0.22, y = -0.6)
    add_geom(leg_verts, leg_faces, (0.22, -0.6, 0.0), (1.0, 1.0, 1.0))

    return all_positions, all_normals, all_indices

def export_character_glb(output_path, torso_color, hair_color):
    positions, normals, indices = build_character_mesh(torso_color, hair_color)
    
    vertex_count = len(positions) // 3
    index_count = len(indices)

    pos_bytes = bytearray()
    for p in positions:
        pos_bytes.extend(struct.pack('<f', p))

    norm_bytes = bytearray()
    for n in normals:
        norm_bytes.extend(struct.pack('<f', n))

    idx_bytes = bytearray()
    for idx in indices:
        idx_bytes.extend(struct.pack('<H', idx))

    # Align byte lengths to 4-byte boundaries
    def align4(b):
        while len(b) % 4 != 0:
            b.append(0)
        return b

    pos_bytes = align4(pos_bytes)
    norm_bytes = align4(norm_bytes)
    idx_bytes = align4(idx_bytes)

    pos_offset = 0
    norm_offset = len(pos_bytes)
    idx_offset = norm_offset + len(norm_bytes)
    total_bin_len = idx_offset + len(idx_bytes)

    bin_data = pos_bytes + norm_bytes + idx_bytes

    # Calculate min/max for position accessor
    min_x = min(positions[0::3]); max_x = max(positions[0::3])
    min_y = min(positions[1::3]); max_y = max(positions[1::3])
    min_z = min(positions[2::3]); max_z = max(positions[2::3])

    gltf = {
        "asset": { "version": "2.0", "generator": "AmbientLoungePythonGLB" },
        "scene": 0,
        "scenes": [{ "nodes": [0] }],
        "nodes": [{ "mesh": 0 }],
        "meshes": [{
            "name": "YoungAdultMaleMesh",
            "primitives": [{
                "attributes": {
                    "POSITION": 0,
                    "NORMAL": 1
                },
                "indices": 2,
                "material": 0
            }]
        }],
        "materials": [{
            "name": "CharacterMaterial",
            "pbrMetallicRoughness": {
                "baseColorFactor": [torso_color[0], torso_color[1], torso_color[2], 1.0],
                "metallicFactor": 0.1,
                "roughnessFactor": 0.7
            }
        }],
        "accessors": [
            { "bufferView": 0, "componentType": 5126, "count": vertex_count, "type": "VEC3", "max": [max_x, max_y, max_z], "min": [min_x, min_y, min_z] },
            { "bufferView": 1, "componentType": 5126, "count": vertex_count, "type": "VEC3" },
            { "bufferView": 2, "componentType": 5123, "count": index_count, "type": "SCALAR" }
        ],
        "bufferViews": [
            { "buffer": 0, "byteOffset": pos_offset, "byteLength": len(pos_bytes), "target": 34962 },
            { "buffer": 0, "byteOffset": norm_offset, "byteLength": len(norm_bytes), "target": 34962 },
            { "buffer": 0, "byteOffset": idx_offset, "byteLength": len(idx_bytes), "target": 34963 }
        ],
        "buffers": [{ "byteLength": len(bin_data) }]
    }

    json_str = json.dumps(gltf, indent=None, separators=(',', ':'))
    json_bytes = json_str.encode('utf-8')
    while len(json_bytes) % 4 != 0:
        json_bytes += b' '

    json_chunk_len = len(json_bytes)
    bin_chunk_len = len(bin_data)
    total_glb_len = 12 + 8 + json_chunk_len + 8 + bin_chunk_len

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'wb') as f:
        # GLB Header
        f.write(b'glTF')
        f.write(struct.pack('<I', 2))
        f.write(struct.pack('<I', total_glb_len))

        # JSON Chunk
        f.write(struct.pack('<I', json_chunk_len))
        f.write(b'JSON')
        f.write(json_bytes)

        # BIN Chunk
        f.write(struct.pack('<I', bin_chunk_len))
        f.write(b'BIN\x00')
        f.write(bin_data)

    print(f"Successfully exported {output_path} ({total_glb_len} bytes)")

if __name__ == "__main__":
    export_character_glb("assets/models/frank.glb", torso_color=(1.0, 0.41, 0.70), hair_color=(0.2, 0.15, 0.1)) # Frank Pink
    export_character_glb("assets/models/milo.glb",  torso_color=(0.25, 0.61, 1.0), hair_color=(0.9, 0.8, 0.3))  # Milo Blue
    export_character_glb("assets/models/sam.glb",   torso_color=(0.18, 0.80, 0.44), hair_color=(0.3, 0.2, 0.15)) # Sam Emerald
    export_character_glb("assets/models/sky.glb",   torso_color=(1.0, 0.75, 0.0), hair_color=(0.1, 0.1, 0.1))   # Sky Amber
