import requests
import threading
import time

URL = "http://192.168.10.113/data"  # ESP32のIP
THREADS = 5                          # 同時接続数
DURATION = 30                        # 実行時間(秒)

stop = False


def attack():
    while not stop:
        try:
            requests.get(URL, timeout=1)
        except Exception:
            pass


threads = []

print("Attack Start!")

for _ in range(THREADS):
    t = threading.Thread(target=attack)
    t.start()
    threads.append(t)

time.sleep(DURATION)

stop = True

for t in threads:
    t.join()

print("Attack End!")