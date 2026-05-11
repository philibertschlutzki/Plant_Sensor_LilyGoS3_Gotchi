import urllib.request
import os
import sys

url = "https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"
output_path = os.path.join("data", "chart.min.js")

if not os.path.exists("data"):
    os.makedirs("data")

print(f"[PRE-BUILD] Downloading Chart.js...")
print(f"  URL: {url}")
print(f"  Target: {output_path}")

try:
    urllib.request.urlretrieve(url, output_path)
    size = os.path.getsize(output_path)
    print(f"[SUCCESS] Downloaded {size} bytes to {output_path}")
except Exception as e:
    print(f"[ERROR] Download failed: {e}")
    print(f"[WARNING] Build continues, but chart.min.js will be missing in Web-UI")
    # Nicht abbrechen — LittleFS wird trotzdem gebaut, nur ohne Chart.js
