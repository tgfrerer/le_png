# `le_png`

Island Module for reading and writing png files.

Compared to `le_pixels`, this module allows you to read more complex `.png` files and allows to convert `.png` files to target image formats on load. 

It also allows you to **save** `.png` images - when feeding the encoder interface to an image swapchain for example. 

If you want to encode pngs faster, you can switch on the CMAKE Option `LE_PNG_USE_FPNGE`. This will make the encoder use [fpnge](https://github.com/veluca93/fpnge) -- image size might be slightly larger, but encoding speed will be much better.

## Credits

* `le_png` uses [lodepng](https://github.com/lvandeve/lodepng) to do png encoding and decoding. 
* `le_png` uses [fpnge](https://github.com/veluca93/fpnge), a very fast png encoder. 

The license files for both dependencies can be found in their respective subfolders.

## Troubleshooting

Note: This module contains its dependency, lodepng as a git submodule. It should get automatically checked out when you configure an Island app that uses `le_png` as a dependency for the first time.

If this does not happen automatically you can update these git submodules manually:

From this current directory, issue the following commands:

```bash 
git submodule init 
git submodule update 
```
