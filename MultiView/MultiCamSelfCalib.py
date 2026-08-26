class MultiCamSelfCalib:
    """
    13章 透視投影カメラの自己校正。
    総数M台のカメラ。複数のカメラから見えている特徴点がN個。
    k番カメラから見えるa番特徴点の点座標が(x_ak, y_ak)。

    カメラ番号k、カメラ総数M
    特徴点番号a、特徴点総数N

    射影的奥行きz_akを求め、観測行列Wを作る。
    """

    def __init__(self, f0):
        """
        f0: x_ak/f0, y_ak/f0 を大体1にするスケール係数f0。f0==600などの適当な値をセット。
        """
        self.f0 = f0

    def PrimaryMethod(
        self,
    ):
        """
        手順13.2 基本法。
        """
