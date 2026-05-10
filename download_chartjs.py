import urllib.request
import os

url = "https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"
output_path = os.path.join("data", "chart.min.js")

if not os.path.exists("data"):
    os.makedirs("data")

print(f"Downloading Chart.js from {url} to {output_path}...")
try:
    urllib.request.urlretrieve(url, output_path)
    print("Download complete.")
except Exception as e:
    print(f"Error downloading Chart.js: {e}")
