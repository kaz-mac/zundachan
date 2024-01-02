chcp 65001
python psdlayers2png.py "四国めたん立ち絵素材2.1.psd" image_metan_png
python pngmerger_rgb565.py image_metan.ini image_metan_png image_metan.h
pause
