import pygame
import sys
import datetime

# Screen
WIDTH = 240
HEIGHT = 320

pygame.init()
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("UI Prototype")

# Load icons
gear_icon = pygame.image.load("settingsgearicon.png")
battery_icon = pygame.image.load("batteryicon.png")
playlist1 = pygame.image.load("takecare.jpeg")
playlist2 = pygame.image.load("inrainbows.jpeg")

# Resize icons
gear_icon = pygame.transform.scale(gear_icon, (16, 16))
battery_icon = pygame.transform.scale(battery_icon, (20, 12))
playlist1 = pygame.transform.scale(playlist1, (50, 50))
playlist2 = pygame.transform.scale(playlist2, (50, 50))

playlist_covers = [playlist1, playlist2]

# 🎨 Colors (gray + purple theme)
BG_COLOR = (18, 18, 18)
CARD_COLOR = (30, 30, 30)
TEXT_COLOR = (200, 200, 200)

PURPLE = (140, 82, 255)
PURPLE_LIGHT = (180, 140, 255)

# Fonts
font = pygame.font.SysFont("Arial", 20)
small_font = pygame.font.SysFont("Arial", 14)

# Menu items
low_menu_items = ["Queue"]
selected_index = 0

queue_songs = [
    "Shot For Me - Drake",
    "Jigsaw Falling Into Place - Radiohead",
    "butterflies. - Brent Faiyaz"
]

queue_index = 0

# Fake playlist covers (placeholders for now)
NUM_PLAYLISTS = 2
library_index = 0

def draw_menu():
    screen.fill(BG_COLOR)

    # 🕒 Time
    now = datetime.datetime.now()
    current_time = now.strftime("%H:%M")

    # 🔝 Top bar
    top_items = ["time", "gear", "battery"]

    for s, item in enumerate(top_items):
        x = 15 + s * 90

        if item == "time":
            text = small_font.render(current_time, True, TEXT_COLOR)
            screen.blit(text, (x, 5))

        elif item == "gear":
            screen.blit(gear_icon, (x, 5))

        elif item == "battery":
            screen.blit(battery_icon, (x, 7))

    # Divider
    pygame.draw.line(screen, (50, 50, 50), (0, 30), (240, 30))

    # 📚 Library title
    lib_title = font.render("Library", True, TEXT_COLOR)
    screen.blit(lib_title, (10, 40))

    # 📚 Playlist covers (horizontal)
    for i in range(len(playlist_covers)):
        # shift based on selected index (scroll effect)
        x = 10 + (i - library_index) * 60
        y = 70

        # only draw if visible on screen
        if -50 < x < WIDTH:
            screen.blit(playlist_covers[i], (x, y))

            # highlight selected playlist
            if i == library_index:
                pygame.draw.rect(screen, PURPLE, (x, y, 50, 50), 3, border_radius=6)
    # Divider
    pygame.draw.line(screen, (50, 50, 50), (0, 140), (240, 140))

    queue = font.render("Queue", True, TEXT_COLOR)
    screen.blit(queue, (10, 150))

    for i, song in enumerate(queue_songs):
        y = 180 + i * 25  # start below "Queue"

        if i == queue_index:
            pygame.draw.rect(screen, PURPLE, (10, y, 220, 22), border_radius=6)
            text = small_font.render(song, True, (255, 255, 255))
        else:
            text = small_font.render(song, True, TEXT_COLOR)

        screen.blit(text, (15, y))

    # 🎵 Bottom "Now Playing" bar
    pygame.draw.rect(screen, CARD_COLOR, (0, 260, 240, 60))

    # Album cover (placeholder)
    pygame.draw.rect(screen, PURPLE_LIGHT, (10, 270, 40, 40), border_radius=6)

    # Song + artist
    song_text = font.render("Blinding Lights", True, TEXT_COLOR)
    artist_text = small_font.render("The Weeknd", True, (150, 150, 150))

    screen.blit(song_text, (60, 270))
    screen.blit(artist_text, (60, 290))

    pygame.display.flip()


# Main loop
while True:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            sys.exit()

        if event.type == pygame.KEYDOWN:
            if event.key == pygame.K_DOWN:
                queue_index = (queue_index + 1) % len(queue_songs)

            if event.key == pygame.K_UP:
                queue_index = (queue_index - 1) % len(queue_songs)

                # 📚 Library navigation
                if event.key == pygame.K_RIGHT:
                    library_index = (library_index + 1) % len(playlist_covers)

                if event.key == pygame.K_LEFT:
                    library_index = (library_index - 1) % len(playlist_covers)

    draw_menu()