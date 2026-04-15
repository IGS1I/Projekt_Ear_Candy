import sys
from pathlib import Path

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget,
    QVBoxLayout, QHBoxLayout, QListWidget, QListWidgetItem,
    QLabel, QStackedWidget, QPushButton
)
from PySide6.QtGui import QPixmap
from PySide6.QtCore import Qt

from scanner import build_library
from library_manager import LibraryManager


ASSETS_DIR = Path("assets")
DEFAULT_COVER = ASSETS_DIR / "default_cover.png"

SCREEN_WIDTH = 240
SCREEN_HEIGHT = 320


def format_duration(seconds_float):
    seconds_float = seconds_float or 0.0
    minutes = int(seconds_float // 60)
    seconds = int(seconds_float % 60)
    return f"{minutes}:{seconds:02d}"


def shorten(text, max_len=28):
    if text is None:
        return ""
    text = str(text)
    if len(text) <= max_len:
        return text
    return text[: max_len - 1] + "…"


class AlbumArtWidget(QLabel):
    def __init__(self, image_path=None):
        super().__init__()
        self.setAlignment(Qt.AlignCenter)
        self.setFixedSize(120, 120)
        self.setStyleSheet("""
            QLabel {
                border-radius: 12px;
                background-color: #ece8ff;
                border: 1px solid #d9d0ff;
                color: #7a6fcf;
                font-size: 11px;
                font-weight: 600;
            }
        """)
        self.set_cover(image_path)

    def set_cover(self, image_path=None):
        final_path = image_path if image_path and Path(image_path).exists() else str(DEFAULT_COVER)

        pixmap = QPixmap(final_path)
        if pixmap.isNull():
            self.clear()
            self.setText("No Cover")
            return

        scaled = pixmap.scaled(
            120, 120,
            Qt.KeepAspectRatioByExpanding,
            Qt.SmoothTransformation
        )
        self.setPixmap(scaled)
        self.setText("")


class MenuPage(QWidget):
    def __init__(self):
        super().__init__()

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(8)

        title = QLabel("Library")
        title.setAlignment(Qt.AlignCenter)
        title.setStyleSheet("""
            font-size: 18px;
            font-weight: 700;
            color: #6c4df6;
            padding: 4px;
        """)

        subtitle = QLabel("Choose a section")
        subtitle.setAlignment(Qt.AlignCenter)
        subtitle.setStyleSheet("""
            font-size: 11px;
            color: #666666;
            padding-bottom: 2px;
        """)

        self.menu_list = QListWidget()
        self.menu_list.addItems(["Songs", "Artists", "Albums", "Playlists"])

        layout.addWidget(title)
        layout.addWidget(subtitle)
        layout.addWidget(self.menu_list)


class ListPage(QWidget):
    def __init__(self, title_text):
        super().__init__()

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(6)

        top_bar = QHBoxLayout()
        top_bar.setSpacing(6)

        self.back_button = QPushButton("←")
        self.back_button.setFixedSize(32, 28)

        self.title = QLabel(title_text)
        self.title.setStyleSheet("""
            font-size: 15px;
            font-weight: 700;
            color: #6c4df6;
            padding-left: 4px;
        """)

        top_bar.addWidget(self.back_button)
        top_bar.addWidget(self.title, 1)

        self.list_widget = QListWidget()

        layout.addLayout(top_bar)
        layout.addWidget(self.list_widget)

    def set_title(self, title_text):
        self.title.setText(shorten(title_text, 22))

    def clear_items(self):
        self.list_widget.clear()

    def add_item(self, text, data):
        item = QListWidgetItem(shorten(text, 34))
        item.setData(Qt.UserRole, data)
        self.list_widget.addItem(item)


class SongDetailPage(QWidget):
    def __init__(self):
        super().__init__()

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(6)

        top_bar = QHBoxLayout()
        self.back_button = QPushButton("←")
        self.back_button.setFixedSize(32, 28)

        self.page_title = QLabel("Now Viewing")
        self.page_title.setStyleSheet("""
            font-size: 15px;
            font-weight: 700;
            color: #6c4df6;
            padding-left: 4px;
        """)

        top_bar.addWidget(self.back_button)
        top_bar.addWidget(self.page_title, 1)

        self.cover_widget = AlbumArtWidget()

        self.song_title = QLabel("No song selected")
        self.song_title.setAlignment(Qt.AlignCenter)
        self.song_title.setWordWrap(True)
        self.song_title.setStyleSheet("""
            font-size: 14px;
            font-weight: 700;
            color: #222222;
            padding-top: 2px;
        """)

        self.song_info = QLabel("")
        self.song_info.setAlignment(Qt.AlignTop)
        self.song_info.setWordWrap(True)
        self.song_info.setStyleSheet("""
            font-size: 11px;
            color: #4a4a4a;
            line-height: 1.3;
            padding-top: 4px;
        """)

        layout.addLayout(top_bar)
        layout.addWidget(self.cover_widget, alignment=Qt.AlignCenter)
        layout.addWidget(self.song_title)
        layout.addWidget(self.song_info)
        layout.addStretch()

    def set_song(self, song):
        self.cover_widget.set_cover(song.get("album_art"))
        self.song_title.setText(shorten(song.get("title", "Unknown"), 30))

        playlist = song.get("playlist") or "None"

        info_text = (
            f'Artist: {song.get("artist", "Unknown Artist")}\n'
            f'Album: {song.get("album", "Unknown Album")}\n'
            f'Duration: {format_duration(song.get("duration", 0.0))}\n'
            f'Playlist: {playlist}'
        )
        self.song_info.setText(info_text)


class MainWindow(QMainWindow):
    def __init__(self, manager):
        super().__init__()
        self.manager = manager

        self.setWindowTitle("MP3 Player")
        self.setFixedSize(SCREEN_WIDTH, SCREEN_HEIGHT)

        self.history = []

        self.stack = QStackedWidget()
        self.setCentralWidget(self.stack)

        self.menu_page = MenuPage()
        self.list_page = ListPage("Items")
        self.detail_page = SongDetailPage()

        self.stack.addWidget(self.menu_page)
        self.stack.addWidget(self.list_page)
        self.stack.addWidget(self.detail_page)

        self.current_song_list = []
        self.current_context = "menu"

        self.menu_page.menu_list.itemClicked.connect(self.handle_menu_clicked)
        self.list_page.back_button.clicked.connect(self.go_back)
        self.detail_page.back_button.clicked.connect(self.go_back)
        self.list_page.list_widget.itemClicked.connect(self.handle_list_item_clicked)

        self.show_menu()

    def push_page(self, widget):
        current_index = self.stack.currentIndex()
        self.history.append(current_index)
        self.stack.setCurrentWidget(widget)

    def go_back(self):
        if self.history:
            previous_index = self.history.pop()
            self.stack.setCurrentIndex(previous_index)

    def show_menu(self):
        self.stack.setCurrentWidget(self.menu_page)
        self.current_context = "menu"

    def handle_menu_clicked(self, item):
        section = item.text()

        if section == "Songs":
            self.show_songs()
        elif section == "Artists":
            self.show_artists()
        elif section == "Albums":
            self.show_albums()
        elif section == "Playlists":
            self.show_playlists()

    def show_songs(self):
        self.current_context = "songs"
        self.current_song_list = []

        self.list_page.set_title("Songs")
        self.list_page.clear_items()

        songs = self.manager.get_all_songs()
        for song in songs:
            text = f'{song["title"]} — {song["artist"]}'
            self.list_page.add_item(text, {"type": "song", "value": song})

        self.push_page(self.list_page)

    def show_artists(self):
        self.current_context = "artists"
        self.list_page.set_title("Artists")
        self.list_page.clear_items()

        artists = self.manager.get_all_artists()
        for artist in artists:
            self.list_page.add_item(artist, {"type": "artist", "value": artist})

        self.push_page(self.list_page)

    def show_albums(self):
        self.current_context = "albums"
        self.list_page.set_title("Albums")
        self.list_page.clear_items()

        albums = self.manager.get_all_albums()
        for album in albums:
            text = f'{album["title"]} — {album["artist"]}'
            self.list_page.add_item(
                text,
                {
                    "type": "album",
                    "value": {
                        "title": album["title"],
                        "artist": album["artist"]
                    }
                }
            )

        self.push_page(self.list_page)

    def show_playlists(self):
        self.current_context = "playlists"
        self.list_page.set_title("Playlists")
        self.list_page.clear_items()

        playlists = self.manager.get_all_playlists()
        for playlist in playlists:
            self.list_page.add_item(playlist, {"type": "playlist", "value": playlist})

        self.push_page(self.list_page)

    def show_artist_songs(self, artist_name):
        self.current_context = "artist_songs"
        self.list_page.set_title(artist_name)
        self.list_page.clear_items()

        songs = self.manager.get_songs_by_artist(artist_name)
        for song in songs:
            text = f'{song["title"]} — {song["album"]}'
            self.list_page.add_item(text, {"type": "song", "value": song})

        self.push_page(self.list_page)

    def show_album_songs(self, album_title, artist_name):
        self.current_context = "album_songs"
        self.list_page.set_title(album_title)
        self.list_page.clear_items()

        songs = self.manager.get_songs_in_album(album_title, artist_name)
        for song in songs:
            self.list_page.add_item(song["title"], {"type": "song", "value": song})

        self.push_page(self.list_page)

    def show_playlist_songs(self, playlist_name):
        self.current_context = "playlist_songs"
        self.list_page.set_title(playlist_name)
        self.list_page.clear_items()

        songs = self.manager.get_playlist(playlist_name)
        for song in songs:
            text = f'{song["title"]} — {song["artist"]}'
            self.list_page.add_item(text, {"type": "song", "value": song})

        self.push_page(self.list_page)

    def show_song_detail(self, song):
        self.detail_page.set_song(song)
        self.push_page(self.detail_page)

    def handle_list_item_clicked(self, item):
        payload = item.data(Qt.UserRole)
        if not payload:
            return

        item_type = payload.get("type")
        value = payload.get("value")

        if item_type == "song":
            self.show_song_detail(value)

        elif item_type == "artist":
            self.show_artist_songs(value)

        elif item_type == "album":
            self.show_album_songs(value["title"], value["artist"])

        elif item_type == "playlist":
            self.show_playlist_songs(value)


def main():
    sd_path = "test_sd"
    library = build_library(sd_path)
    manager = LibraryManager(library)

    app = QApplication(sys.argv)

    app.setStyleSheet("""
        QMainWindow, QWidget {
            background-color: #f6f4ff;
            color: #1f1f1f;
            font-family: Arial;
            font-size: 11px;
        }

        QListWidget {
            background-color: #ffffff;
            border: 1px solid #ddd7ff;
            border-radius: 10px;
            outline: none;
            padding: 4px;
        }

        QListWidget::item {
            padding: 8px 6px;
            margin: 2px 0px;
            border-radius: 8px;
            min-height: 20px;
        }

        QListWidget::item:hover {
            background-color: #eee9ff;
        }

        QListWidget::item:selected {
            background-color: #6c4df6;
            color: white;
        }

        QLabel {
            color: #1f1f1f;
            background: transparent;
        }

        QPushButton {
            background-color: #6c4df6;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 700;
            padding: 4px;
        }

        QPushButton:pressed {
            background-color: #5a3fe0;
        }

        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 2px;
        }

        QScrollBar::handle:vertical {
            background: #cabfff;
            border-radius: 3px;
            min-height: 20px;
        }

        QScrollBar::handle:vertical:hover {
            background: #6c4df6;
        }

        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
        }
    """)

    window = MainWindow(manager)
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()