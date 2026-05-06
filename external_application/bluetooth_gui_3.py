import asyncio
from bleak import BleakScanner, BleakClient
import ctypes as ct
import struct
import csv

#  GUI
import struct
from qasync import QEventLoop, asyncSlot

from enum import Enum, auto



import sys
from datetime import datetime, timedelta

import numpy as np
import pandas as pd
from PyQt5 import QtCore
from PyQt5.QtWidgets import (
    QApplication,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
    QWidget,
)
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure




# SAMPLE_STRUCT_FORMAT = "<HBBBBBBff"   # must match C struct
HEADER_STRUCT_FORMAT = "<50sQBBBBH"   # must match C struct
HEADER_STRUCT_SIZE = struct.calcsize(HEADER_STRUCT_FORMAT)

SAMPLE_STRUCT_FORMAT = "<Qff"   # must match C struct
SAMPLE_STRUCT_SIZE = struct.calcsize(SAMPLE_STRUCT_FORMAT)

MESSAGE_STRUCT_FORMAT = "<16s"   # must match C struct
MESSAGE_STRUCT_SIZE = struct.calcsize(SAMPLE_STRUCT_FORMAT)


# Replace these with your ESP32's UUIDs
DEVICE_MAC = "ec:e3:34:78:59:ee" # <-- your ESP32 MAC
# SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHAR_UUID    = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
RX_UUID  = "12345678-1234-1234-1234-123456789abc"


device_name = "ESP32"
device_mac = "ec:e3:34:78:59:ee"

# log_file = None



MAX_POINTS = 1000

class State(Enum):
    SCAN = auto()
    CONNECT = auto()
    SUBSCRIBE = auto()
    RECEIVE = auto()
    DISCONNECT = auto()
    DONE = auto()

state = State.SCAN

device = None
client = None
writer = None
log_file = None



# new_rows = []




# -------------------------------------------------
# Mock UroSMART data
# Replace later with BLE / cloud / app data source
# -------------------------------------------------
def generate_mock_data(days: int = 56) -> pd.DataFrame:
    end = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
    dates = [end - timedelta(days=i) for i in range(days)][::-1]

    rng = np.random.default_rng(8)
    avg_flow = 12 + 1.8 * np.sin(np.linspace(0, 4 * np.pi, days)) + rng.normal(0, 0.7, days)
    daily_voided = 1600 + 220 * np.sin(np.linspace(0, 2.8 * np.pi, days) + 0.5) + rng.normal(0, 90, days)
    bladder_remaining = 110 + 16 * np.sin(np.linspace(0, 3 * np.pi, days) + 1.3) + rng.normal(0, 8, days)
    void_count = rng.integers(5, 10, days)

    return pd.DataFrame(
        {
            "date": dates,
            "avg_flow_rate_ml_s": np.round(avg_flow, 1),
            "daily_voided_volume_ml": np.round(daily_voided, 0),
            "estimated_bladder_remaining_ml": np.round(bladder_remaining, 0),
            "void_count": void_count,
        }
    )


class TrendCanvas(FigureCanvas):
    def __init__(self, parent=None):
        self.figure = Figure(figsize=(5, 3), tight_layout=True)
        self.ax = self.figure.add_subplot(111)
        super().__init__(self.figure)
        self.setParent(parent)

    def plot_metric(self, dates, values, title, ylabel):
        self.ax.clear()
        self.ax.plot(dates, values, linewidth=2)
        self.ax.set_title(title, fontsize=11, pad=10)
        self.ax.set_ylabel(ylabel)
        self.ax.set_xlabel("Date")
        self.ax.grid(True, alpha=0.3)
        self.figure.autofmt_xdate(rotation=25)
        self.draw()


class MetricCard(QFrame):
    def __init__(self, title: str, value: str, subtitle: str):
        super().__init__()
        self.setObjectName("MetricCard")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(18, 16, 18, 16)
        layout.setSpacing(6)

        title_label = QLabel(title)
        title_label.setObjectName("CardTitle")

        value_label = QLabel(value)
        value_label.setObjectName("CardValue")

        subtitle_label = QLabel(subtitle)
        subtitle_label.setObjectName("CardSubtitle")
        subtitle_label.setWordWrap(True)

        layout.addWidget(title_label)
        layout.addWidget(value_label)
        layout.addWidget(subtitle_label)

class UroSmartWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("UroSMART GUI Prototype")
        self.resize(1450, 900)

        # BLE Widgets
        self.sync_status_label = QLabel("Disconnected")
        self.sync_status_label.setObjectName("SideBody")
        self.sync_status_label.setWordWrap(True)


        self.new_rows = []

        # GUI Data - Old method
        self.flow_data = []
        self.volume_data = []
        self.time_data = []

        # self.df = generate_mock_data()
        self.df = pd.DataFrame(columns=[
            "date", "avg_flow_rate_ml_s", "daily_voided_volume_ml"
        ])
        self.filtered_df = self.df.copy()

        # --- BLE State ---
        self.ble_state = State.SCAN
        self.ble_device = None
        self.ble_client = None
        self.ble_writer = None
        self.ble_log_file = None
        self.ble_status = "Disconnected"

        self.setStyleSheet(
            """
            QMainWindow {
                background: #f7f4f4;
            }
            QLabel#HeaderTitle {
                font-size: 32px;
                font-weight: 700;
                color: #102b5c;
            }
            QLabel#HeaderSubtitle {
                font-size: 14px;
                color: #56616f;
            }
            QFrame#Sidebar {
                background: #edd8d8;
                border-radius: 22px;
            }
            QFrame#Panel {
                background: white;
                border-radius: 20px;
                border: 1px solid #ece6e6;
            }
            QFrame#MetricCard {
                background: #f3e3e3;
                border-radius: 18px;
                border: 1px solid #ead1d1;
            }
            QLabel#CardTitle {
                font-size: 13px;
                color: #7b5a5a;
                font-weight: 600;
            }
            QLabel#CardValue {
                font-size: 28px;
                color: #102b5c;
                font-weight: 700;
            }
            QLabel#CardSubtitle {
                font-size: 12px;
                color: #5f6672;
            }
            QLabel#SectionTitle {
                font-size: 18px;
                font-weight: 700;
                color: #102b5c;
            }
            QLabel#SideHeading {
                font-size: 14px;
                font-weight: 700;
                color: #102b5c;
            }
            QLabel#SideBody {
                font-size: 13px;
                color: #394351;
            }
            QPushButton {
                background: #c95f58;
                color: white;
                border: none;
                border-radius: 12px;
                padding: 10px 16px;
                font-weight: 600;
            }
            QPushButton:hover {
                background: #b84d46;
            }
            QComboBox {
                background: white;
                border: 1px solid #d8caca;
                border-radius: 10px;
                padding: 8px 10px;
                min-height: 20px;
            }
            QListWidget {
                background: #fffafb;
                border: 1px solid #eadede;
                border-radius: 14px;
                padding: 6px;
            }
            """
        )

        self._build_ui()
        self.update_dashboard()

    def _build_ui(self):
        root = QWidget()
        self.setCentralWidget(root)

        main_layout = QHBoxLayout(root)
        main_layout.setContentsMargins(24, 24, 24, 24)
        main_layout.setSpacing(20)


        disconnect_button = QPushButton("Disconnect BLE")
        disconnect_button.clicked.connect(self.disconnect_ble)


        sidebar = QFrame()
        sidebar.setObjectName("Sidebar")
        sidebar.setFixedWidth(320)
        side_layout = QVBoxLayout(sidebar)
        side_layout.setContentsMargins(22, 22, 22, 22)
        side_layout.setSpacing(18)

        app_label = QLabel("UroSMART")
        app_label.setObjectName("HeaderTitle")
        app_label.setStyleSheet("font-size: 26px;")

        side_intro = QLabel(
            "Receives BLE uploads from the ESP32, stores urinary metrics, and displays multi-day and multi-week trends for patients and clinicians."
        )
        side_intro.setObjectName("SideBody")
        side_intro.setWordWrap(True)

        mode_title = QLabel("User Type")
        mode_title.setObjectName("SideHeading")
        self.user_type = QComboBox()
        self.user_type.addItems(["Patient View", "Clinician View"])
        self.user_type.currentIndexChanged.connect(self.update_dashboard)

        range_title = QLabel("Trend Window")
        range_title.setObjectName("SideHeading")
        self.range_box = QComboBox()
        self.range_box.addItems(["7 Days", "14 Days", "30 Days", "8 Weeks"])
        self.range_box.setCurrentText("30 Days")
        self.range_box.currentIndexChanged.connect(self.update_dashboard)

        sync_title = QLabel("BLE Sync")
        sync_title.setObjectName("SideHeading")
        sync_status = QLabel(
            "Last packet upload: 24 hours ago\nNext sync: scheduled after next device handshake"
        )
        sync_status.setObjectName("SideBody")
        sync_status.setWordWrap(True)

        refresh_button = QPushButton("Refresh Dashboard")
        refresh_button.clicked.connect(self.update_dashboard)

        notes_title = QLabel("Recent Alerts / Notes")
        notes_title.setObjectName("SideHeading")
        self.notes_list = QListWidget()
        self.notes_list.addItem(QListWidgetItem("Flow rate decreased slightly over the last 3 days"))
        self.notes_list.addItem(QListWidgetItem("Bladder residual trend is stable this week"))
        self.notes_list.addItem(QListWidgetItem("No missed BLE uploads in current window"))

        side_layout.addWidget(app_label)
        side_layout.addWidget(side_intro)
        side_layout.addSpacing(4)
        side_layout.addWidget(mode_title)
        side_layout.addWidget(self.user_type)
        side_layout.addWidget(range_title)
        side_layout.addWidget(self.range_box)
        side_layout.addWidget(sync_title)
        side_layout.addWidget(sync_status)
        side_layout.addWidget(refresh_button)
        side_layout.addWidget(notes_title)
        side_layout.addWidget(self.notes_list, 1)

        side_layout.addWidget(sync_title)
        side_layout.addWidget(self.sync_status_label)
        side_layout.addWidget(disconnect_button)
        

        content = QVBoxLayout()
        content.setSpacing(18)

        header_wrap = QFrame()
        header_wrap.setObjectName("Panel")
        header_layout = QVBoxLayout(header_wrap)
        header_layout.setContentsMargins(24, 20, 24, 20)

        title = QLabel("Design Details – User Application")
        title.setObjectName("HeaderTitle")

        subtitle = QLabel(
            "Desktop GUI prototype for visualizing urinary flow rate, cumulative voided volume, and estimated bladder volume remaining across days and weeks."
        )
        subtitle.setObjectName("HeaderSubtitle")
        subtitle.setWordWrap(True)

        header_layout.addWidget(title)
        header_layout.addWidget(subtitle)

        cards_layout = QGridLayout()
        cards_layout.setHorizontalSpacing(16)
        cards_layout.setVerticalSpacing(16)

        self.card_flow = MetricCard("Average Flow Rate", "--", "Daily mean urinary flow rate")
        self.card_voided = MetricCard("Daily Voided Volume", "--", "Total voided volume over selected window")
        self.card_remaining = MetricCard("Bladder Volume Left", "--", "Estimated post-void residual trend")
        self.card_count = MetricCard("Void Count", "--", "Average number of daily voiding events")

        cards_layout.addWidget(self.card_flow, 0, 0)
        cards_layout.addWidget(self.card_voided, 0, 1)
        # cards_layout.addWidget(self.card_remaining, 1, 0)
        # cards_layout.addWidget(self.card_count, 1, 1)

        charts_panel = QFrame()
        charts_panel.setObjectName("Panel")
        charts_layout = QGridLayout(charts_panel)
        charts_layout.setContentsMargins(18, 18, 18, 18)
        charts_layout.setSpacing(16)

        charts_title = QLabel("Trend Visualization")
        charts_title.setObjectName("SectionTitle")

        self.flow_chart = TrendCanvas()
        self.voided_chart = TrendCanvas()
        self.remaining_chart = TrendCanvas()

        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update_dashboard)
        self.timer.start(50)

        charts_layout.addWidget(charts_title, 0, 0, 1, 2)
        charts_layout.addWidget(self.flow_chart, 1, 0)
        charts_layout.addWidget(self.voided_chart, 1, 1)
        # charts_layout.addWidget(self.remaining_chart, 2, 0, 1, 2)

        content.addWidget(header_wrap)
        content.addLayout(cards_layout)
        content.addWidget(charts_panel, 1)

        main_layout.addWidget(sidebar)
        main_layout.addLayout(content, 1)

    def _selected_days(self) -> int:
        mapping = {
            "7 Days": 7,
            "14 Days": 14,
            "30 Days": 30,
            "8 Weeks": 56,
        }
        return mapping[self.range_box.currentText()]


    # ----------------------------------------------------------------
    # BLE
    # ----------------------------------------------------------------

    async def ble_notification_handler(self, sender, data):
        """
        Called whenever the ESP32 sends telemetry.
        `data` is a bytes object.
        """

        count = len(data) // SAMPLE_STRUCT_SIZE

        print(f"DATA BYTES: {len(data)}")

        if len(data) == HEADER_STRUCT_SIZE:
            print("-----------------------------------------")
            print("-------------    Header    --------------")
            print("-----------------------------------------")    
            device_name, time_started, sample_rate_hz, dh, dm, ds, padding = \
                struct.unpack(HEADER_STRUCT_FORMAT, data)
            print(f"Device: {c_str_to_string(device_name)}, started: {time_started}, rate: {sample_rate_hz} Hz")
            self.ble_status = f"Connected — {c_str_to_string(device_name)}"
            self._update_ble_status_label()  # see below
        elif (len(data) == MESSAGE_STRUCT_SIZE):
            message = struct.unpack(MESSAGE_STRUCT_FORMAT, data)
            print("MESSAGE RECIEVED")
            print(message)
            self.ble_state = State.SCAN
        else:
            print("-----------------------------------------")
            print("------------- Sample Packet -------------")
            print("-----------------------------------------")    
            for sample in struct.iter_unpack(SAMPLE_STRUCT_FORMAT, data):
                timestamp, volume, flow = sample
                print(sample)

                # PROPPER WASY BUT DOES NOT YET WORK
                self.new_rows.append({
                    "date": datetime.fromtimestamp(timestamp / 1e6),
                    "avg_flow_rate_ml_s": flow,
                    "daily_voided_volume_ml": volume,
                })

                # ------------------------------ Hackey way
                self.time_data.append(timestamp)
                self.flow_data.append(flow)
                self.volume_data.append(volume)

                # Keep the window under the max points
                if len(self.time_data) > MAX_POINTS:
                    self.time_data.pop(0)
                    self.flow_data.pop(0)
                    self.volume_data.pop(0)


                # ------------------------------------------------------


                if self.ble_writer:
                    self.ble_writer.writerow(sample)
                    self.ble_log_file.flush()

        asyncio.create_task(
            self.ble_client.write_gatt_char(RX_UUID, b"ACK", response=False)
        )

    async def run_ble(self):

        i = 0
        while True:

            if self.ble_state == State.SCAN:
                
                # Update Status
                self.ble_status = "Scanning" + "."*i + " "*(4-i)
                self._update_ble_status_label()
                print(f"\r{self.ble_status}", end="")
                i = (i + 1)%4

                devices_original = await BleakScanner.discover()
                devices = [d for d in devices_original if d.name]
                
                # # Print discovered devices
                # for i, d in enumerate(devices):
                #     print(f"{i}: {d.name} [{d.address}]")

                for d in devices:
                    if d.address.lower() == device_mac.lower():
                        self.ble_device = d
                        self.ble_state = State.CONNECT
                        break

            elif self.ble_state == State.CONNECT:

                # update status
                self.ble_status = "Connecting..."
                self._update_ble_status_label()
                print("Recieving...")

                self.ble_client = BleakClient(self.ble_device.address)
                await self.ble_client.connect(timeout=10.0)
                if self.ble_client.is_connected:
                    self.ble_log_file = open("python_out_logs/log.csv", "w", newline="")
                    self.ble_writer = csv.writer(self.ble_log_file)
                    self.ble_state = State.SUBSCRIBE
                else:
                    print("Connection failed, scanning...")
                    self.ble_state = State.SCAN

            elif self.ble_state == State.SUBSCRIBE:
                await self.ble_client.start_notify(CHAR_UUID, self.ble_notification_handler)
                self.ble_state = State.RECEIVE

                # update status
                self.ble_status = "Receiving data"
                self._update_ble_status_label()
                print(self.ble_status)

                print(self.ble_status)
 
                await self.ble_client.write_gatt_char(RX_UUID, b"READY_TO_RECIEVE", response=False)



            elif self.ble_state == State.RECEIVE:
                if not self.ble_client.is_connected:
                    
                    # update status
                    self.ble_status = "Disconnected — retrying"
                    self._update_ble_status_label()
                    print(self.ble_status)

                    self.ble_state = State.SCAN
                    if self.ble_log_file:
                        self.ble_log_file.close()
                await asyncio.sleep(1)

            elif self.ble_state == State.DISCONNECT:
                await self.ble_client.disconnect()

                # update status
                self.ble_status = "Disconnected"
                self._update_ble_status_label()
                print(self.ble_status)

                self.ble_state = State.DONE

            elif self.ble_state == State.DONE:
                if self.ble_client.is_connected:
                    self.ble_state = State.RECEIVE
                await asyncio.sleep(1)

    def disconnect_ble(self):
        """Call this from a GUI button to cleanly disconnect."""
        self.ble_state = State.DISCONNECT

    def _update_ble_status_label(self):
        self.sync_status_label.setText(self.ble_status)


    # def update_plots(self):
    #     if len(time_data) > 2:
    #         self.load_curve.setData(time_data, flow_data)
    #         self.vol_curve.setData(time_data, volume_data)

    def update_dashboard(self):

        # Update the data frame from the list of new samples recieved
        if self.new_rows is not None:
            self.df = pd.concat(
                [self.df, pd.DataFrame(self.new_rows)],
                ignore_index=True
            )
            self.new_rows = []
        
        

        days = self._selected_days()
        self.filtered_df = self.df.tail(days).copy()

        if self.filtered_df.empty:
            self._update_card(self.card_flow, "-- mL/s")
            self._update_card(self.card_voided, "-- ml")
            self._update_card(self.card_remaining, "-- mL")
            self._update_card(self.card_count, "-- / day")
            return

        avg_flow = self.filtered_df["avg_flow_rate_ml_s"].mean()
        avg_voided = self.filtered_df["daily_voided_volume_ml"].mean()
        # avg_remaining = self.filtered_df["estimated_bladder_remaining_ml"].mean()
        # avg_count = self.filtered_df["void_count"].mean()

        # self._update_card(self.card_flow, f"{avg_flow:.1f} mL/s")
        # self._update_card(self.card_voided, f"{avg_voided:.0f} ml")

        self._update_card(self.card_flow, f"{sum(self.flow_data)/len(self.flow_data):.1f} mL/s")
        self._update_card(self.card_voided, f"{self.volume_data[-1]:.0f} ml")




        # self._update_card(self.card_remaining, f"{avg_remaining:.0f} mL")
        # self._update_card(self.card_count, f"{avg_count:.1f} / day")


        # ---- Plot BLE data if available, otherwise fall back to mock ----
        if len(self.flow_data) > 1:
            # Convert raw uint64 timestamps to datetime for readable x-axis
            timestamps = [datetime.fromtimestamp(t / 1e6) for t in self.time_data]  # adjust divisor if needed
            self.flow_chart.plot_metric(timestamps, self.flow_data, "Urinary Flow Rate (Live)", "mL/s")
            self.voided_chart.plot_metric(timestamps, self.volume_data, "Cumulative Voided Volume (Live)", "mL")
        # else:
        #     dates = self.filtered_df["date"]
        #     self.flow_chart.plot_metric(dates, self.filtered_df["avg_flow_rate_ml_s"], "Urinary Flow Rate", "mL/s")
        #     self.voided_chart.plot_metric(dates, self.filtered_df["daily_voided_volume_ml"], "Cumulative Daily Voided Volume", "mL")
        #     self.remaining_chart.plot_metric(dates, self.filtered_df["estimated_bladder_remaining_ml"], "Estimated Bladder Volume Left", "mL")

        self.notes_list.clear()
        if self.user_type.currentText() == "Patient View":
            self.notes_list.addItem("Your weekly flow rate trend is mostly stable.")
            self.notes_list.addItem("Residual bladder volume is slightly lower this week.")
            self.notes_list.addItem("BLE sync completed successfully.") 
        else:
            self.notes_list.addItem("Patient trend summary: no major deviation from baseline.")
            self.notes_list.addItem("Review residual spikes on higher-volume days.")
            self.notes_list.addItem("Compare trends against void diary and catheter events.")

    @staticmethod
    def _update_card(card: QFrame, value: str):
        labels = card.findChildren(QLabel)
        if len(labels) >= 2:
            labels[1].setText(value)



# ------------------------- BLUETOOTH FUNCTIONALITY -------------------------

# async def notification_handler(sender, data):
#     """
#     Called whenever the ESP32 sends telemetry.
#     `data` is a bytes object.
#     """
#     global writer
#     global client


#     if writer == None:
#         print("Writer is None, not handling notifications...")
#         return -1
    
#     count = len(data) // SAMPLE_STRUCT_SIZE


#     # for i in range(count):
#     #     offset = i * SAMPLE_STRUCT_SIZE
#     #     chunk = data[offset:offset + SAMPLE_STRUCT_SIZE]
#     #     sample = struct.unpack(SAMPLE_STRUCT_FORMAT, chunk)
#     #     print(sample)

#     # REPLACE THIS LOGIC WITH PROPPER STATE MACHINE, ONLY A HACK TO ENSURE THAT HEADER IS READ PROPPERLY
#     print(f"DATA BYTES: {len(data)}")
#     if (len(data) == HEADER_STRUCT_SIZE):
#         print("-----------------------------------------")
#         print("-------------    Header    --------------")
#         print("-----------------------------------------")    

#         device_name, time_started, sample_rate_hz, duration_hours, duration_minutes, duration_seconds, padding= struct.unpack(HEADER_STRUCT_FORMAT, data)
#         print(f"Device name: {c_str_to_string(device_name)}")
#         print(f"Time Started {time_started}")
#         print(f"Sample Rate: {sample_rate_hz}")
#     else:

#         print("-----------------------------------------")
#         print("------------- Sample Packet -------------")
#         print("-----------------------------------------")    

#         for sample in struct.iter_unpack(SAMPLE_STRUCT_FORMAT, data):

#             # new_rows.append(sample)


#             timestamp, flow, volume = sample
#             new_rows.append({
#                 "date": datetime.fromtimestamp(timestamp / 1e6),  # adjust divisor as needed
#                 "avg_flow_rate_ml_s": flow,
#                 "daily_voided_volume_ml": volume,
#             })


#             print(sample)
#             writer.writerow(sample)
            
#             # time_data.append(sample[0])
#             # flow_data.append(sample[1])
#             # volume_data.append(sample[2])

#             # if len(flow_data) > MAX_POINTS:
#             #     time_data.pop(0)
#             #     flow_data.pop(0)
#             #     volume_data.pop(0)

#             log_file.flush() 
        


    
#     # Send ACK
#     # await client.write_gatt_char(RX_UUID, b"ACK", response=True)
#     asyncio.create_task(client.write_gatt_char(RX_UUID, b"ACK", response=False))

    
#     # print(f"Telemetry from {sender}: {decode_sample(data)}")
#     # print(decode_sample(data))
#     # print(vdd_decode_from_bytes(data))




def c_str_to_string(c_str):
    return c_str.split(b'\x00', 1)[0].decode('utf-8')

# async def run_state_machine():
#     global state
#     global device
#     global client

#     global writer
#     global log_file
    
#     i = 0
#     while True:
#         if state == State.SCAN:
#             print("\rScanning for BLE devices" + "."*i + " "*(4-i), end="")
#             i = (i + 1)%4
#             devices_original = await BleakScanner.discover()

#             devices = [device for device in devices_original if device.name]

#             # # Print discovered devices
#             # for i, d in enumerate(devices):
#             #     print(f"{i}: {d.name} [{d.address}]")


#             for d in devices:
#                 if d.address.lower() == device_mac.lower():
#                 # if d.name == device_name:
#                     device = d
#                     print()
#                     state = State.CONNECT
#                     break
            
#             if (device == None):
#                 pass
#                 # print("Scan failed...trying again")


#         elif state == State.CONNECT:
        
#             print("Connecting...")
#             client = BleakClient(device.address)

#             await client.connect()

#             if client.is_connected:
#                 print("Services...")
#                 for service in client.services:
#                     for char in service.characteristics:
#                         print(char.uuid, char.properties)

#                 log_file = open("python_out_logs/log.csv", "w", newline="")
#                 writer = csv.writer(log_file)

#                 state = State.SUBSCRIBE
#             else:
#                 print("Connection failed, scanning...")
#                 state = State.SCAN


#         elif state == State.SUBSCRIBE:

#             print("Subscribing...")
#             await client.start_notify(CHAR_UUID, notification_handler)
#             state = State.RECEIVE
#             print("Receiving...")



#         elif state == State.RECEIVE:
#             if (client.is_connected == False):
#                 print("Client disconnected, going back to scanning")
#                 state = State.SCAN
#                 if log_file:
#                     log_file.close()  # ensures all buffered data is written
#             await asyncio.sleep(1)


#         elif state == State.DISCONNECT:
#             await client.disconnect()
#             state = State.DONE

#         elif state == State.DONE:
#             print("Transmission done")

#             if (client.is_connected):
#                 print("Still connected, waiting for more data")
#                 state = State.RECEIVE


# Press Shift+F10 to execute it or replace it with your code.
# Press Double Shift to search everywhere for classes, files, tool windows, actions, and settings.


def print_hi(name):
    # Use a breakpoint in the code line below to debug your script.
    print(f'Hi, {name}')  # Press Ctrl+F8 to toggle the breakpoint.


# Press the green button in the gutter to run the script.
# if __name__ == '__main__':
#     print_hi('PyCharm')

# See PyCharm help at https://www.jetbrains.com/help/pycharm/



if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")

    # forces SelectorEventLoop which is MTA-compatible on Windows
    if sys.platform == "win32":
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())


    loop = QEventLoop(app)
    asyncio.set_event_loop(loop)

    window = UroSmartWindow()
    window.show()

    with loop:
        loop.create_task(window.run_ble())  # BLE is now a method on the window
        loop.run_forever()

    # sys.exit(app.exec())


    

    # asyncio.run(run_state_machine())

