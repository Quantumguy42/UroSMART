import sys
import csv
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget,
    QVBoxLayout, QPushButton, QFileDialog,
    QTableWidget, QTableWidgetItem, QLabel
)
from PyQt5.QtGui import QIcon
from PyQt5.QtGui import QKeySequence
from PyQt5.QtWidgets import QLineEdit
from PyQt5.QtCore import Qt
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

import struct
import ctypes


# C:\Users\ual-laptop\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0\LocalCache\local-packages\Python313\Scripts\pyinstaller.exe --onefile --noconsole --icon=icons/urosmart_logo_2.ico --add-data "icons/urosmart_logo_2.ico;." bin_plotting_test.py

HEADER_STRUCT_FORMAT = "<50sQBBBBH"   # must match C struct
HEADER_STRUCT_SIZE = struct.calcsize(HEADER_STRUCT_FORMAT)

SAMPLE_STRUCT_FORMAT = "<Qff"   # must match C struct
SAMPLE_STRUCT_SIZE = struct.calcsize(SAMPLE_STRUCT_FORMAT)

ICON_PATH = "icons/urosmart_logo_2.ico"

ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID("urosmart.datareader")


app = QApplication(sys.argv)
app.setWindowIcon(QIcon(ICON_PATH))


class FileViewer(QMainWindow):

    def __init__(self):
        super().__init__()

        self.setWindowTitle("Custom File Viewer")
        self.setWindowIcon(QIcon(ICON_PATH))
        self.resize(800, 600)

        self.setAcceptDrops(True)

        layout = QVBoxLayout()


        self.open_button = QPushButton("Open File")
        self.open_button.clicked.connect(self.open_file)

        self.copy_button = QPushButton("Copy All")
        self.copy_button.clicked.connect(self.copy_all)

        self.info_label = QLabel("Drop a .bin file here or click Open File") 

        self.info_label.setTextInteractionFlags(
            Qt.TextSelectableByMouse | Qt.TextSelectableByKeyboard
        )
        # self.info_label = QLineEdit()
        # self.info_label.setReadOnly(True)

        self.dataTable = QTableWidget()

        self.figure = Figure()
        self.canvas = FigureCanvas(self.figure)

        self.dataTable.keyPressEvent = self.handle_keypress

        layout.addWidget(self.open_button)
        layout.addWidget(self.copy_button)
        layout.addWidget(self.info_label)
        layout.addWidget(self.dataTable)
        # layout.addWidget(self.canvas)

        container = QWidget()
        container.setLayout(layout)

        self.setCentralWidget(container)

    def dragEnterEvent(self, event):

        if event.mimeData().hasUrls():
            event.acceptProposedAction()
        else:
            event.ignore()

    def dragMoveEvent(self, event):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
        else:
            event.ignore()

    def dropEvent(self, event):

        if event.mimeData().hasUrls():

            url = event.mimeData().urls()[0]
            filepath = url.toLocalFile()

            self.load_file(filepath)

            event.acceptProposedAction()

    def open_file(self):
        filename, _ = QFileDialog.getOpenFileName(
            self,
            "Open File",
            "",
            "Binary Files (*.bin);;Sample Files (*.txt);;All Files (*)"
        )

        if filename:
            self.load_file(filename)


    def copy_selection(self):

        selected = self.dataTable.selectedRanges()

        if not selected:
            return

        text = ""

        r = selected[0]

        for row in range(r.topRow(), r.bottomRow() + 1):

            row_text = []

            for col in range(r.leftColumn(), r.rightColumn() + 1):

                item = self.dataTable.item(row, col)

                if item:
                    row_text.append(item.text())
                else:
                    row_text.append("")

            text += "\t".join(row_text) + "\n"

        QApplication.clipboard().setText(text)

    def handle_keypress(self, event):

        if event.matches(QKeySequence.StandardKey.Copy):
            self.copy_selection()
        else:
            super(QTableWidget, self.dataTable).keyPressEvent(event)
    
    def copy_all(self):
        self.dataTable.selectAll()
        self.copy_selection()

    def c_str_to_string(self, c_str):
        return c_str.split(b'\x00', 1)[0].decode('utf-8')

    def load_file(self, filepath):

        timestamp_values = []
        volume_values = []
        flow_values = []



        # with open(filepath) as f:
        #     reader = csv.reader(f)
        #     next(reader)

        #     data = list(reader)

        with open(filepath, "rb") as f:
            data = f.read()
        
        header = data[:HEADER_STRUCT_SIZE]
        samples = data[HEADER_STRUCT_SIZE:]

        # Update Header 
        
        device_name, time_started, sample_rate_hz, duration_hours, duration_minutes, duration_seconds, padding= struct.unpack(HEADER_STRUCT_FORMAT, header)
   
        self.info_label.setText(
            f"Device name: {self.c_str_to_string(device_name)}\n"
            f"Time Started: {time_started}\n"
            f"Sample Rate: {sample_rate_hz}\n"
            f"Duration: {duration_hours:02}:{duration_minutes:02}:{duration_seconds:02}"
        )

        self.dataTable.setRowCount(len(samples) // SAMPLE_STRUCT_SIZE)
        self.dataTable.setColumnCount(3)
        self.dataTable.setHorizontalHeaderLabels(["Timestamp", "Volume", "Flow"])

        row_index = 0
        for timestamp, volume, flow in struct.iter_unpack(SAMPLE_STRUCT_FORMAT, samples):
            timestamp_values.append(int(timestamp))
            volume_values.append(float(volume))
            flow_values.append(float(flow))

            self.dataTable.setItem(row_index, 0, QTableWidgetItem(f"{timestamp}"))
            self.dataTable.setItem(row_index, 1, QTableWidgetItem(f"{volume:.2f}"))
            self.dataTable.setItem(row_index, 2, QTableWidgetItem(f"{flow:.2f}"))
            row_index += 1
        
        # Plot volume over time
        # self.plot_data(timestamp_values, volume_values)


        # for row_index, row in enumerate(data):

        #     timestamp_values.append(int(row[0]))
        #     volume.append(int(row[1]))

        #     self.dataTable.setItem(row_index, 0, QTableWidgetItem(row[0]))
        #     self.dataTable.setItem(row_index, 1, QTableWidgetItem(row[1]))

        # self.plot_data(timestamp_values, values)

    def plot_data(self, x, y):

        self.figure.clear()

        ax = self.figure.add_subplot(111)
        ax.plot(x, y)
        ax.set_title("Sample Data")

        self.canvas.draw()



window = FileViewer()
window.show()

sys.exit(app.exec())