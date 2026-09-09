"""wallclock - a lightweight dynamic clock wallpaper.

Layout:
    core       time -> angle maths, schedule rules, sun times, configuration
    renderer   composes background + layers into one static image (Pillow)
    themes     theme loading and the pluggable Layer system
    platforms  tiny adapters that apply an image as the OS wallpaper
    scheduler  computes the next instant something changes and sleeps until then
"""

__version__ = "0.1.0"
