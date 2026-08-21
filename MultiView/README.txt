# 金谷健一他, 3次元コンピュータービジョン計算ハンドブック, 森北出版
# のいくつかのアルゴリズムのPython実装。
# バグっています。

# インストール

miniforge prompt起動
conda deactivate
conda env remove -y -n multiview
conda create -y -n multiview python=3.12
conda activate multiview
conda install -y pip opencv scipy pytest
pip install jupyter numpy matplotlib pandas opencv-python

# 実行

## miniforge prompt起動

conda activate multiview
cd \work\projects\MultiView

00run.bat

# テストデータ説明

Synthetic_OctPrism : 8角柱を、円状に取り囲む24台のカメラで撮影した写真24枚。Blenderで生成した画像。

# プログラム説明

Run_FeatureMatch3.py : 3つの画像からfeature point の座標の組のCSVを作る。
Run_MultiCamSelfCalib.py : 3つの画像のfeature pointの組のCSVから、3つのカメラの姿勢を推定しCSV出力する。
Run_MergeMultiCam.py : 複数のカメラの姿勢CSVを集めて、全カメラの姿勢を推定しCSV出力する。




