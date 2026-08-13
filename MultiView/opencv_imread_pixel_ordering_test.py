# cv.imreadの画像のロードのテスト。
# 左上から横方向に明るくなっていく以下のような3x2のグレースケール画像を読んで
# 作られる配列の構造をテスト。
# [ [ 0   56  79 ]
#   [ 121 156 234 ] ]
#
# ■ 結果 ■
# cv.imreadの戻す2次元配列imgのピクセル値は、
# img[y, x]のようにアクセスする必要があり、画像の幅高さは以下のように取得する：
# 幅: img.shape[1]
# 高さ: img.shape[0]
# 配列は、x+、y↓の順でピクセル値が入る。


import cv2 as cv

img = cv.imread("opencv_imread_pixel_ordering_test_img.png", cv.IMREAD_GRAYSCALE)
print(f"img={img}")
print(f"img shape[0] = {img.shape[0]}, shape[1] = {img.shape[1]}")
print(f"img[0,1]={img[0,1]}")
