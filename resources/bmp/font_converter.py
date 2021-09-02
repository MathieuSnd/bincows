from PIL import Image
import numpy as np

img = np.array(Image.open('charmap.bmp'))


i: int # col
j: int # line
ascii: int = 0
chars = [] # chars tab

for i in range(0,256,16):
    for j in range(0,192,12):
        chars.append(img[i:i+16,j:j+12,:])

chars_arr = np.concatenate(chars, axis=0)

Image.fromarray(chars_arr).save("new.bmp")
