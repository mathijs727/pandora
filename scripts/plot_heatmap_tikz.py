# usage: python png_pgfplots.py [name] [input-file]
# without the square brackets...
# Written by Jost Henkel, 19.11.2014
#
# === Modified by Mathijs Molenaar ===

import re
import sys
import subprocess
import shared_image
import matplotlib.pyplot as plt
import numpy as np

# Plot variables
xr0 = "*"
xr1 = "*"
yr0 = "*"
yr1 = "*"
zr0 = "-25"
zr1 = "-3"

def write_tex(name, xr0, xr1, yr0, yr1, zr0, zr1):
    texstring = f"""

\\documentclass{{standalone}}
\\usepackage{{pgfplots}}
\\pgfplotsset{{compat=newest}}

\\begin{{document}}

\\begin{{tikzpicture}}

\\begin{{axis}}[
  enlargelimits=false,
  axis on top,
  colorbar,
  point meta max={zr1},
  point meta min={zr0},
  colormap name={{viridis}}
]
\\addplot graphics [
xmin={xr0},
xmax={xr1},
ymin={yr0},
ymax={yr1}
] {{{name}.png}};
\\end{{axis}} 

\\end{{tikzpicture}}

\\end{{document}}

"""

    # write gnuplot script
    with open(f"{name}.tex", "w") as f:
        f.write(texstring)


def write_color_mapped_image(name, image):
    fig, ax = plt.subplots()
    ax.imshow(image, cmap="viridis")
    ax.axis("off")
    fig.savefig(f"{name}.png", bbox_inches="tight")

if __name__ == "__main__":
    ###########################################################
    # check input
    if(len(sys.argv) > 2):
        name = str(sys.argv[1]).split('.')[0]
        inp = str(sys.argv[2])
    else:
        print("Please enter a filename!")
        sys.exit()

    image = shared_image.read_exr(inp)
    write_color_mapped_image(name, image)
    
    write_tex(name, 0, image.shape[1], 0, image.shape[0], np.min(image), np.max(image))

    #subprocess.call(["pdflatex", "-shell-escape", f"{name}.tex"])
