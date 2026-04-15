🎵 MP3 Player Library

A lightweight Python-based MP3 player library system that scans local storage, extracts metadata, and organizes music into a structured, user-friendly interface.

⸻

🚀 Overview

This project focuses on building the core library system of an MP3 player, handling how music is discovered, organized, and displayed.

The system scans a /Music directory, processes .mp3 files, extracts metadata (title, artist, album), and dynamically builds a library of songs, artists, albums, and playlists.

Designed with simplicity and performance in mind, this project simulates how real-world music applications manage local media.

⸻

✨ Features
	•	🎧 Automatic scanning of local music directory
	•	🏷️ Metadata extraction (Title, Artist, Album)
	•	🖼️ Album art handling with fallback placeholders
	•	🎵 Song library generation
	•	👤 Artist and album organization
	•	📂 Playlist detection via folder structure
	•	⚡ Lightweight and fast processing
	•	🖥️ GUI interface built with PySide6

⸻

🧠 Library Rules (Core Logic)

To keep the system consistent and predictable, the following rules are enforced:
	•	Only .mp3 files are supported
	•	A single root directory: /Music
	•	All .mp3 files inside /Music and its first-level subfolders are treated as songs
	•	Metadata (not folder names) determines:
	•	Artist
	•	Album
	•	Each first-level folder inside /Music is treated as a playlist
	•	Songs directly inside /Music:
	•	Are included in the library
	•	Are not assigned to any playlist
	•	Nested folders beyond one level are ignored
	•	Unsupported file types are skipped
