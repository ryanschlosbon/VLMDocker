# test_infer.py
import sys
import base64
import requests

def encode_image(path):
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode()

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python test_infer.py <image_path> \"<command>\"")
        sys.exit(1)

    image_path = sys.argv[1]
    command = sys.argv[2]

    img_b64 = encode_image(image_path)
    payload = {
        "image_base64": img_b64,
        "command": command
    }

    try:
        response = requests.post("http://localhost:5001/infer", json=payload)
        print("Server response:", response.json())
    except Exception as e:
        print("Error connecting to server:", e)
