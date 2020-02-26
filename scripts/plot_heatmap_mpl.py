import shared_image
import imageio
import os
import matplotlib.pyplot as plt
import numpy as np

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

def plot_culling_on_vs_off(results_folder, scenes, spp):
    fig, axis = plt.subplots(nrows=2, ncols=len(scenes))
    for j, scene in enumerate(scenes):
        image_file1 = os.path.join(results_folder, "mem_limit", "100", "culling", scene, "run-0", "num_top_level_intersections.exr")
        image_file2 = os.path.join(results_folder, "mem_limit", "100", "no-culling", scene, "run-0", "num_top_level_intersections.exr")
        images = (shared_image.read_exr(image_file1), shared_image.read_exr(image_file2))
        images = [image / spp for image in images]
        images = [image_cutout(im, 1.0) for im in images]

        vmax = max(np.max(images[0]), np.max(images[1]))

        for i in range(2):
            ax = axis[i][j]
            im = ax.imshow(images[i], vmin=0, vmax=vmax, cmap="coolwarm")
            ax.axis("off")

    plt.colorbar(im, ax=axis.ravel().tolist())
    plt.show()

if __name__ == "__main__":
    results_folder = "C:/Users/mathi/OneDrive/TU Delft/Batched Ray Traversal/Results/"
    plot_culling_on_vs_off(results_folder, ["crown", "landscape"], 256)