import shared_image
import imageio
import os
import matplotlib.pyplot as plt
import numpy as np
import cv2
from mpl_toolkits.axes_grid1 import make_axes_locatable, ImageGrid
import matplotlib as mpl

# https://tex.stackexchange.com/questions/77968/how-do-i-avoid-type3-fonts-when-submitting-to-manuscriptcentral
mpl.rcParams['pdf.fonttype'] = 42
mpl.rcParams['ps.fonttype'] = 42

font_size = 20

def image_cutout(image, cutout_aspect):
    height, width = image.shape
    image_aspect = width / height
    if image_aspect > cutout_aspect:
        cutout_width = width * (cutout_aspect / image_aspect)
        border = int((width - cutout_width) / 2)
        return image[:,border:-border]
    else:
        cutout_height = height * (image_aspect / cutout_aspect)
        border = int((height - cutout_height) / 2)
        return image[border:-border,:]

def add_image_border(image, thickness):
    out = image.copy()
    out[:thickness,:] = 0
    out[-thickness:,:] = 0
    out[:,:thickness] = 0
    out[:,-thickness:] = 0
    return out

def black_border_image(resolution, thickness):
    out = np.zeros((resolution[1], resolution[0], 4))
    out[:thickness,:,3] = 1
    out[-thickness:,:,3] = 1
    out[:,:thickness,3] = 1
    out[:,-thickness:,3] = 1
    return out

def plot_culling_on_vs_off(results_folder, scenes, spp):
    image_border = 6
    desired_image_res = (1200, 900)
    desired_aspect = desired_image_res[0] / desired_image_res[1]

    image_pairs = []
    #vmax = 0
    for scene in scenes:
        image_file1 = os.path.join(results_folder, "mem_limit", "100", "no-culling", scene, "run-0", "num_top_level_intersections.exr")
        image_file2 = os.path.join(results_folder, "mem_limit", "100", "culling", scene, "run-0", "num_top_level_intersections.exr")
        images = (shared_image.read_exr(image_file1), shared_image.read_exr(image_file2))
        images = [image / spp for image in images]
        images = [image_cutout(im, desired_aspect) for im in images]
        images = [cv2.resize(im, desired_image_res) for im in images]

        #vmax = max(vmax, max(np.max(images[0]), np.max(images[1])))
        image_pairs.append(images)

    fig = plt.figure(figsize=(24,10))
    for col, image_pair in enumerate(image_pairs):
        grid = ImageGrid(fig, int(f"1{len(image_pairs)}{col+1}"),
            nrows_ncols=(2,1),
            axes_pad=0.1,
            label_mode="L",
            share_all=True,
            cbar_mode="single",
            cbar_location="right",
            cbar_size="3%",
            cbar_set_cax=True)
        
        vmax = np.max(image_pair)
        for row in range(2):
            ax = grid.axes_all[row]
            im = ax.imshow(image_pair[row], vmin=0, vmax=vmax, cmap="rainbow")
            ax.imshow(black_border_image(desired_image_res, image_border))
            ax.axis("off")
        cbar = grid.cbar_axes[0].colorbar(im)
        cbar.ax.tick_params(labelsize=font_size)

    fig.savefig("num_intersections.png", bbox_inches="tight")
    plt.show()

if __name__ == "__main__":
    results_folder = "C:/Users/Mathijs/OneDrive/TU Delft/Projects/Batched Ray Traversal/Results/"
    #results_folder = "C:/Users/mathi/Desktop/EG2020-final/updated_code"
    plot_culling_on_vs_off(results_folder, ["crown", "landscape", "islandX"], 256)