# wmf_yv12
demo project for unexpected yv12 layout returned by Windows Media Foundation video decoder.

sample video is generated with:

ffmpeg -f lavfi -i smptebars=size=800x600:duration=1 -pix_fmt yuv420p -c:v libx264 ./test_smpte_420_800x600.mp4
