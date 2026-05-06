import sys
import csv
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget,
    QVBoxLayout, QHBoxLayout, QPushButton, QFileDialog,
    QTableWidget, QTableWidgetItem, QLabel, QMessageBox
)
from PyQt5.QtGui import QIcon
from PyQt5.QtGui import QKeySequence
from PyQt5.QtWidgets import QLineEdit
from PyQt5.QtCore import Qt
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

import struct
import ctypes

HEADER_STRUCT_FORMAT = "<50sQBBBBH"
HEADER_STRUCT_SIZE = struct.calcsize(HEADER_STRUCT_FORMAT)

SAMPLE_STRUCT_FORMAT = "<Qff"
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

        # --- State ---
        self.current_filepath = None   # path of the currently open file
        self.header_raw = None         # raw bytes of the header (preserved on save)
        self.is_dirty = False          # unsaved changes flag

        layout = QVBoxLayout()

        # --- Button row ---
        btn_layout = QHBoxLayout()

        self.open_button = QPushButton("Open File")
        self.open_button.clicked.connect(self.open_file)

        self.copy_button = QPushButton("Copy All")
        self.copy_button.clicked.connect(self.copy_all)

        self.save_button = QPushButton("Save")
        self.save_button.clicked.connect(self.save_file)
        self.save_button.setEnabled(False)

        self.save_as_button = QPushButton("Save As…")
        self.save_as_button.clicked.connect(self.save_file_as)
        self.save_as_button.setEnabled(False)

        self.add_row_button = QPushButton("Add Row")
        self.add_row_button.clicked.connect(self.add_row)
        self.add_row_button.setEnabled(False)

        self.delete_row_button = QPushButton("Delete Row(s)")
        self.delete_row_button.clicked.connect(self.delete_rows)
        self.delete_row_button.setEnabled(False)

        btn_layout.addWidget(self.open_button)
        btn_layout.addWidget(self.copy_button)
        btn_layout.addWidget(self.save_button)
        btn_layout.addWidget(self.save_as_button)

        btn_layout.addWidget(self.add_row_button)        # <-- new
        btn_layout.addWidget(self.delete_row_button)     # <-- new

        self.info_label = QLabel("Drop a .bin file here or click Open File")
        self.info_label.setTextInteractionFlags(
            Qt.TextSelectableByMouse | Qt.TextSelectableByKeyboard
        )

        self.dataTable = QTableWidget()
        self.dataTable.keyPressEvent = self.handle_keypress

        # Track edits so we can mark the file dirty and enable Save
        self.dataTable.itemChanged.connect(self.on_cell_changed)

        layout.addLayout(btn_layout)
        layout.addWidget(self.info_label)
        layout.addWidget(self.dataTable)

        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)

    # ------------------------------------------------------------------ #
    #  Drag-and-drop                                                       #
    # ------------------------------------------------------------------ #

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
            self.load_file(url.toLocalFile())
            event.acceptProposedAction()

    # ------------------------------------------------------------------ #
    #  Open                                                                #
    # ------------------------------------------------------------------ #

    def open_file(self):
        filename, _ = QFileDialog.getOpenFileName(
            self, "Open File", "",
            "Binary Files (*.bin);;All Files (*)"
        )
        if filename:
            self.load_file(filename)

    # ------------------------------------------------------------------ #
    #  Load                                                                #
    # ------------------------------------------------------------------ #

    def load_file(self, filepath):
        # Warn about unsaved changes before replacing
        if self.is_dirty:
            reply = QMessageBox.question(
                self, "Unsaved Changes",
                "You have unsaved changes. Open a new file anyway?",
                QMessageBox.Yes | QMessageBox.No
            )
            if reply == QMessageBox.No:
                return

        with open(filepath, "rb") as f:
            data = f.read()

        self.header_raw = data[:HEADER_STRUCT_SIZE]
        samples = data[HEADER_STRUCT_SIZE:]

        device_name, time_started, sample_rate_hz, \
            duration_hours, duration_minutes, duration_seconds, padding = \
            struct.unpack(HEADER_STRUCT_FORMAT, self.header_raw)

        self.info_label.setText(
            f"Device name: {self.c_str_to_string(device_name)}\n"
            f"Time Started: {time_started}\n"
            f"Sample Rate: {sample_rate_hz}\n"
            f"Duration: {duration_hours:02}:{duration_minutes:02}:{duration_seconds:02}"
        )

        # Populate table — block signals so itemChanged doesn't fire during load
        self.dataTable.blockSignals(True)
        self.dataTable.setRowCount(len(samples) // SAMPLE_STRUCT_SIZE)
        self.dataTable.setColumnCount(3)
        self.dataTable.setHorizontalHeaderLabels(["Timestamp", "Volume", "Flow"])

        for row_index, (timestamp, volume, flow) in enumerate(
                struct.iter_unpack(SAMPLE_STRUCT_FORMAT, samples)):
            self.dataTable.setItem(row_index, 0, QTableWidgetItem(str(timestamp)))
            self.dataTable.setItem(row_index, 1, QTableWidgetItem(f"{volume:.6f}"))
            self.dataTable.setItem(row_index, 2, QTableWidgetItem(f"{flow:.6f}"))

        self.dataTable.blockSignals(False)

        self.current_filepath = filepath
        self._set_dirty(False)
        self.save_as_button.setEnabled(True)

        self.add_row_button.setEnabled(True)
        self.delete_row_button.setEnabled(True)

    # ------------------------------------------------------------------ #
    #  Dirty-state helpers                                                 #
    # ------------------------------------------------------------------ #

    def on_cell_changed(self, item):
        """Called whenever the user edits a cell."""
        self._set_dirty(True)

    def _set_dirty(self, dirty: bool):
        self.is_dirty = dirty
        self.save_button.setEnabled(dirty and self.current_filepath is not None)
        # Reflect unsaved state in the title bar
        title = "Custom File Viewer"
        if self.current_filepath:
            title += f" — {self.current_filepath}"
        if dirty:
            title += " *"
        self.setWindowTitle(title)

    # ------------------------------------------------------------------ #
    #  Save                                                                #
    # ------------------------------------------------------------------ #

    def save_file(self):
        """Overwrite the file that was originally opened."""
        if self.current_filepath:
            self._write_binary(self.current_filepath)

    def save_file_as(self):
        """Write to a new path chosen by the user."""
        filename, _ = QFileDialog.getSaveFileName(
            self, "Save As", self.current_filepath or "",
            "Binary Files (*.bin);;All Files (*)"
        )
        if filename:
            self._write_binary(filename)
            self.current_filepath = filename   # future "Save" targets new path
            self._set_dirty(False)

    def _write_binary(self, filepath):
        """Serialize header + table rows back into the binary format."""
        try:
            samples_bytes = self._build_samples_bytes()
        except ValueError as exc:
            QMessageBox.critical(self, "Save Error",
                                 f"Could not convert cell value:\n{exc}")
            return

        with open(filepath, "wb") as f:
            f.write(self.header_raw)      # header is preserved as-is
            f.write(samples_bytes)

        self._set_dirty(False)

    def _build_samples_bytes(self) -> bytes:
        """Read every row from the table and pack into binary structs."""
        out = bytearray()
        for row in range(self.dataTable.rowCount()):
            ts_item  = self.dataTable.item(row, 0)
            vol_item = self.dataTable.item(row, 1)
            flow_item = self.dataTable.item(row, 2)

            # Raise ValueError with a helpful message if conversion fails
            try:
                timestamp = int(ts_item.text())
            except (ValueError, AttributeError):
                raise ValueError(f"Row {row+1}, Timestamp: '{ts_item.text() if ts_item else ''}' is not an integer.")
            try:
                volume = float(vol_item.text())
            except (ValueError, AttributeError):
                raise ValueError(f"Row {row+1}, Volume: '{vol_item.text() if vol_item else ''}' is not a float.")
            try:
                flow = float(flow_item.text())
            except (ValueError, AttributeError):
                raise ValueError(f"Row {row+1}, Flow: '{flow_item.text() if flow_item else ''}' is not a float.")

            out += struct.pack(SAMPLE_STRUCT_FORMAT, timestamp, volume, flow)
        return bytes(out)

    # ------------------------------------------------------------------ #
    #  Clipboard                                                           #
    # ------------------------------------------------------------------ #

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
                row_text.append(item.text() if item else "")
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

    # ------------------------------------------------------------------ #
    #  Utilities                                                           #
    # ------------------------------------------------------------------ #

    def c_str_to_string(self, c_str):
        return c_str.split(b'\x00', 1)[0].decode('utf-8')

    def closeEvent(self, event):
        """Warn before quitting with unsaved changes."""
        if self.is_dirty:
            reply = QMessageBox.question(
                self, "Unsaved Changes",
                "You have unsaved changes. Quit anyway?",
                QMessageBox.Yes | QMessageBox.No
            )
            if reply == QMessageBox.No:
                event.ignore()
                return
        event.accept()
    
    def add_row(self):
        """Insert a blank row directly below the current selection, or at the end."""
        self.dataTable.blockSignals(True)

        selected = self.dataTable.selectedItems()
        if selected:
            # Find the lowest selected row and insert just below it
            insert_at = max(item.row() for item in selected) + 1
        else:
            # Nothing selected — append to the end
            insert_at = self.dataTable.rowCount()

        self.dataTable.insertRow(insert_at)

        # Pre-fill with neutral default values so the binary pack won't fail
        self.dataTable.setItem(insert_at, 0, QTableWidgetItem("0"))    # timestamp
        self.dataTable.setItem(insert_at, 1, QTableWidgetItem("0.0"))  # volume
        self.dataTable.setItem(insert_at, 2, QTableWidgetItem("0.0"))  # flow

        self.dataTable.blockSignals(False)

        # Scroll to and select the new row so the user can edit immediately
        self.dataTable.scrollToItem(self.dataTable.item(insert_at, 0))
        self.dataTable.selectRow(insert_at)

        self._set_dirty(True)

    def delete_rows(self):
        """Delete all currently selected rows, or the last row if nothing is selected."""
        selected_rows = sorted(
            set(item.row() for item in self.dataTable.selectedItems()),
            reverse=True   # delete bottom-up so indices don't shift mid-loop
        )

        if not selected_rows:
            # Nothing selected — remove the last row as a fallback
            last = self.dataTable.rowCount() - 1
            if last < 0:
                return
            selected_rows = [last]

        confirm = QMessageBox.question(
            self,
            "Delete Rows",
            f"Delete {len(selected_rows)} row(s)?",
            QMessageBox.Yes | QMessageBox.No
        )
        if confirm == QMessageBox.No:
            return

        self.dataTable.blockSignals(True)
        for row in selected_rows:
            self.dataTable.removeRow(row)
        self.dataTable.blockSignals(False)

        self._set_dirty(True)


window = FileViewer()
window.show()
sys.exit(app.exec())