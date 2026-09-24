import requests
import csv
import time
from datetime import datetime

ESP32_URL = "http://192.168.10.113/data"
CSV_FILE = "monitor.csv"

fields = [
    "timestamp",
    "request_count",
    "free_heap",
    "uptime_sec",
    "rssi",
    "cpu_temp",
    "cpu_total",
    "cpu_core0",
    "cpu_core1"
]

# CSVファイルを新規作成してヘッダーを書く
with open(CSV_FILE, "w", newline="", encoding="utf-8") as file:
    writer = csv.DictWriter(file, fieldnames=fields)
    writer.writeheader()

print("ESP32 logging started")
print("Stop: Ctrl+C")

try:
    while True:

        try:
            # ESP32の /data からJSONを取得
            response = requests.get(
                ESP32_URL,
                timeout=2
            )

            response.raise_for_status()

            data = response.json()

            # PC側の取得時刻を追加
            data["timestamp"] = datetime.now().isoformat(
                timespec="milliseconds"
            )

            # CSVへ1行追加
            with open(
                CSV_FILE,
                "a",
                newline="",
                encoding="utf-8"
            ) as file:

                writer = csv.DictWriter(
                    file,
                    fieldnames=fields
                )

                writer.writerow(data)

            print(
                data["timestamp"],
                f'CPU={data["cpu_total"]:.1f}%',
                f'Temp={data["cpu_temp"]:.1f}'
            )

        except Exception as e:
            print("Data acquisition error:", e)

        time.sleep(1)

except KeyboardInterrupt:
    print("\nLogging stopped")