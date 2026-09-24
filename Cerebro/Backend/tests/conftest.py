import os

# pytest collection must never open the user's serial device.
os.environ.setdefault("KIRA_SIN_SERIAL", "1")
