import imageio
import numpy as np

#exr_input = "C:/Users/Mathijs/Documents/GitHub/pandora/build/MSVC RelWithDebInfo/output.exr"
exr_input = "C:/Users/Mathijs/Desktop/output.exr"
jpg_output = "C:/Users/Mathijs/Desktop/output.jpg"
scale = 1 / 15

def tonemapping_reinhard(image):
    return image / (image + 1.0)

def tonemapping_aces(image):
    a = 2.51
    b = 0.03
    c = 2.43
    d = 0.59
    e = 0.14
    return (image * (a * image + b)) / (image * (c * image + d) + e)

def gamma_correct(image):
    #return np.log(1 / 2.2) / np.log(image)
    return np.power(image, 1 / 2.2)

def float_to_uint8(image):
    return np.clip(255 * image, 0, 255).astype(np.uint8)

if __name__ == "__main__":
    image = imageio.imread(exr_input)
    print(image.shape)
    print(image[650, 330])


    """image *= scale

    #image = tonemapping_reinhard(image)
    image = tonemapping_aces(image)

    image = gamma_correct(image)
    image = float_to_uint8(image)
    imageio.imwrite(jpg_output, image, ignoregamma=True)"""
