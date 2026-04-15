class LibraryManager:
    def __init__(self, library):
        self.library = library

    # --- Songs ---
    def get_all_songs(self):
        return self.library["songs"]

    # --- Artists ---
    def get_all_artists(self):
        return list(self.library["artists"].keys())

    def get_artist(self, artist_name):
        return self.library["artists"].get(artist_name)

    def get_songs_by_artist(self, artist_name):
        artist = self.get_artist(artist_name)
        return artist["songs"] if artist else []

    # --- Albums ---
    def get_all_albums(self):
        return list(self.library["albums"].values())

    def get_album(self, album_title, artist_name):
        return self.library["albums"].get((album_title, artist_name))

    def get_songs_in_album(self, album_title, artist_name):
        album = self.get_album(album_title, artist_name)
        return album["songs"] if album else []

    # --- Playlists ---
    def get_all_playlists(self):
        return list(self.library["playlists"].keys())

    def get_playlist(self, playlist_name):
        return self.library["playlists"].get(playlist_name, [])

    # --- Utility ---
    def search_songs(self, query):
        query = query.lower()
        return [
            song for song in self.library["songs"]
            if query in song["title"].lower()
            or query in song["artist"].lower()
            or query in song["album"].lower()
        ]