# ---------------------------------------------------------------------------- #
#                                                                              #
# 	Module:       vex_python_player.py                                         #
# 	Description:  VEXcode V5 Python Video Player template                      #
#                                                                              #
# ---------------------------------------------------------------------------- #

from vex import *

# Initialize brain
brain = Brain()

def main():
    brain.screen.clear_screen()
    brain.screen.set_cursor(1, 1)
    brain.screen.print("VEX Video Player")
    
    # Note: If using VEXcode Python with BMP sequences:
    # brain.screen.draw_image_from_file("/usd/frame0.png", 0, 0)
    
if __name__ == "__main__":
    main()
