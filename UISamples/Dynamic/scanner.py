from pathlib import Path
from hashlib import md5
from mutagen.mp3 import MP3
from mutagen.easyid3 import EasyID3
from mutagen.id3 import ID3


def extract_album_art(file_path, cache_dir="cache/album_art"):
    """
    Extract embedded album art from an MP3 file and save it to a cache folder.
    Returns the saved image path as a string, or None if no art is found.
    """
    file_path = Path(file_path)
    cache_path = Path(cache_dir)
    cache_path.mkdir(parents=True, exist_ok=True)

    try:
        tags = ID3(file_path)

        for tag in tags.values():
            if tag.FrameID == "APIC":
                image_data = tag.data
                mime_type = getattr(tag, "mime", "image/jpeg")

                if mime_type == "image/png":
                    extension = ".png"
                else:
                    extension = ".jpg"

                # Make a stable filename based on the file path
                image_name = md5(str(file_path.resolve()).encode()).hexdigest() + extension
                image_file = cache_path / image_name

                # Avoid rewriting if already cached
                if not image_file.exists():
                    with open(image_file, "wb") as img:
                        img.write(image_data)

                return str(image_file.resolve())

    except Exception:
        pass

    return None


def read_mp3_metadata(file_path):
    file_path = Path(file_path)

    metadata = {
        "title": file_path.stem,
        "artist": "Unknown Artist",
        "album": "Unknown Album",
        "duration": 0.0,
        "album_art": None
    }

    try:
        audio = MP3(file_path)
        metadata["duration"] = round(audio.info.length, 2)
    except Exception:
        pass

    try:
        tags = EasyID3(file_path)
        metadata["title"] = tags.get("title", [file_path.stem])[0]
        metadata["artist"] = tags.get("artist", ["Unknown Artist"])[0]
        metadata["album"] = tags.get("album", ["Unknown Album"])[0]
    except Exception:
        pass

    metadata["album_art"] = extract_album_art(file_path)

    return metadata


def scan_music_library(sd_root):
    sd_root = Path(sd_root)
    music_root = sd_root / "Music"

    songs = []
    playlists = {}

    if not music_root.exists() or not music_root.is_dir():
        print("Music folder not found.")
        return songs, playlists

    for item in music_root.iterdir():
        if item.is_file() and item.suffix.lower() == ".mp3":
            metadata = read_mp3_metadata(item)

            song_data = {
                "file_path": str(item.resolve()),
                "filename": item.name,
                "playlist": None,
                "title": metadata["title"],
                "artist": metadata["artist"],
                "album": metadata["album"],
                "duration": metadata["duration"],
                "album_art": metadata["album_art"]
            }
            songs.append(song_data)

    for item in music_root.iterdir():
        if item.is_dir():
            playlist_name = item.name
            playlist_songs = []

            for subitem in item.iterdir():
                if subitem.is_file() and subitem.suffix.lower() == ".mp3":
                    metadata = read_mp3_metadata(subitem)

                    song_data = {
                        "file_path": str(subitem.resolve()),
                        "filename": subitem.name,
                        "playlist": playlist_name,
                        "title": metadata["title"],
                        "artist": metadata["artist"],
                        "album": metadata["album"],
                        "duration": metadata["duration"],
                        "album_art": metadata["album_art"]
                    }

                    songs.append(song_data)
                    playlist_songs.append(song_data)

            if playlist_songs:
                playlists[playlist_name] = playlist_songs

    songs = sorted(songs, key=lambda s: s["title"].lower())
    return songs, playlists


def build_artists_library(songs):
    artists = {}

    for song in songs:
        artist_name = song["artist"]

        if artist_name not in artists:
            artists[artist_name] = {
                "name": artist_name,
                "songs": [],
                "albums": set()
            }

        artists[artist_name]["songs"].append(song)
        artists[artist_name]["albums"].add(song["album"])

    for artist_name in artists:
        artists[artist_name]["albums"] = sorted(list(artists[artist_name]["albums"]))
        artists[artist_name]["songs"] = sorted(
            artists[artist_name]["songs"],
            key=lambda s: s["title"].lower()
        )

    return dict(sorted(artists.items(), key=lambda item: item[0].lower()))


def build_albums_library(songs):
    albums = {}

    for song in songs:
        album_key = (song["album"], song["artist"])

        if album_key not in albums:
            albums[album_key] = {
                "title": song["album"],
                "artist": song["artist"],
                "songs": [],
                "album_art": song["album_art"]
            }

        albums[album_key]["songs"].append(song)

        if albums[album_key]["album_art"] is None and song["album_art"] is not None:
            albums[album_key]["album_art"] = song["album_art"]

    for album_key in albums:
        albums[album_key]["songs"] = sorted(
            albums[album_key]["songs"],
            key=lambda s: s["title"].lower()
        )

    return dict(
        sorted(
            albums.items(),
            key=lambda item: (item[1]["artist"].lower(), item[1]["title"].lower())
        )
    )


def build_library(sd_root):
    songs, playlists = scan_music_library(sd_root)
    artists = build_artists_library(songs)
    albums = build_albums_library(songs)

    return {
        "songs": songs,
        "artists": artists,
        "albums": albums,
        "playlists": playlists
    }


from library_manager import LibraryManager

if __name__ == "__main__":
    sd_path = "test_sd"
    library = build_library(sd_path)

    manager = LibraryManager(library)

    print("\nArtists:")
    print(manager.get_all_artists())

    print("\nSongs by Rosalia:")
    for song in manager.get_songs_by_artist("Rosalia "):
        print(song["title"])

    print("\nPlaylists:")
    print(manager.get_all_playlists())