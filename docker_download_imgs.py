import os
import urllib.request


website = "https://monstermash.zone/imgs/"
files = [
    # Images.
    "example-antelope.jpg",
    "example-bird.jpg",
    "example-box.jpg",
    "example-dino.jpg",
    "example-dog.jpg",
    "example-heart.jpg",
    "example-chihuahua.jpg",
    "example-tree.jpg",
    "mouse_Left.jpg",
    "mouse_Wheel.jpg",
    "mouse_Right.jpg",
    "loading.jpg",

    # Movies.
    "tutorial-mode1.mov",
    "tutorial-mode2.mov",
    "tutorial-mode3.mov",
    "help-drawmode.mov",
    "help-inflatemode.mov",
    "help-animatemode.mov",
    "help-redrawmode.mov",

    # Icons.
    "icon_pause.svg",
    "icon_play.svg",
    "icon_new_ss2_white.svg",
    "icon_save_ss2_white.svg",
    "icon_open_ss2_white.svg",
    "rotate-gizmo.svg",
    "icon_trash_ss2_white.svg",
    "tutorial-play.svg",
    "key-box.svg",
    "key-rect.svg",
    "key-rectBig.svg"
]

os.makedirs("imgs")

for f in files:
    url = website + f
    urllib.request.urlretrieve(url, os.path.join("imgs", f))
