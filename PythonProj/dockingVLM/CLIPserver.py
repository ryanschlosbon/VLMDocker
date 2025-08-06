import os
import base64
from io import BytesIO
from datetime import datetime
from PIL import Image
import torch
import clip  # pip install git+https://github.com/openai/CLIP.git
from flask import Flask, request, jsonify

app = Flask(__name__)

# Where to store dataset images & labels
DATASET_DIR = "vlm_dataset"
os.makedirs(DATASET_DIR, exist_ok=True)
LABELS_PATH = os.path.join(DATASET_DIR, "labels.csv")
# If labels.csv doesn’t exist yet, write header
if not os.path.exists(LABELS_PATH):
    with open(LABELS_PATH, "w") as f:
        f.write("filename,command\n")

# Device
device = "cuda" if torch.cuda.is_available() else "cpu"

# Load CLIP
model, preprocess = clip.load("ViT-B/32", device=device)
model.eval()

# Define actions
ACTIONS = {
    "forward": "move forward",
    "backward": "move backward",
    "rotate_cw": "rotate clockwise",
    "rotate_ccw": "rotate counter clockwise",
    "align": "align with the docking port",
    "hold": "hold position"
}
action_texts = list(ACTIONS.values())
action_keys  = list(ACTIONS.keys())

# Precompute text embeddings
with torch.no_grad():
    tokens     = clip.tokenize(action_texts).to(device)
    text_embs  = model.encode_text(tokens)
    text_embs /= text_embs.norm(dim=-1, keepdim=True)

def infer_action(image: Image.Image, command: str):
    image_input = preprocess(image).unsqueeze(0).to(device)
    text_input  = clip.tokenize([command]).to(device)
    with torch.no_grad():
        img_emb  = model.encode_image(image_input)
        txt_emb  = model.encode_text(text_input)
        img_emb /= img_emb.norm(dim=-1, keepdim=True)
        txt_emb /= txt_emb.norm(dim=-1, keepdim=True)
        combined = (img_emb + txt_emb) / 2
        combined /= combined.norm(dim=-1, keepdim=True)
        sims     = (combined @ text_embs.T).squeeze(0)
        idx      = sims.argmax().item()
        return action_keys[idx], sims[idx].item()

@app.route("/infer", methods=["POST"])
def infer_endpoint():
    data = request.json or {}
    img_b64 = data.get("image_base64")
    cmd     = data.get("command", "")
    if not img_b64:
        return jsonify({"error": "Missing image_base64"}), 400

    # Decode image
    try:
        raw = base64.b64decode(img_b64)
        img = Image.open(BytesIO(raw)).convert("RGB")
    except Exception as e:
        return jsonify({"error": f"Image decode error: {e}"}), 400

    # 1) Save to dataset
    ts       = datetime.utcnow().strftime("%Y%m%dT%H%M%S%f")
    fname    = f"{ts}.png"
    path     = os.path.join(DATASET_DIR, fname)
    img.save(path, format="PNG")
    with open(LABELS_PATH, "a") as f:
        f.write(f"{fname},{cmd}\n")

    # 2) Inference
    action, confidence = infer_action(img, cmd)
    return jsonify({"action": action, "confidence": float(confidence)})

if __name__ == "__main__":
    app.run(port=5001)
