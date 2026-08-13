# インストール

miniforge prompt起動
conda deactivate
conda env remove -y -n multiview
conda create -y -n multiview python=3.12
conda activate multiview
conda install -y pip opencv scipy pytest
pip install jupyter numpy matplotlib pandas

# jupyter notebook 実行

## miniforge prompt起動

cd \work\projects\MultiView
jupyter notebook 
# a.ipynbをセーブします

# a.ipynbを指定して実行。
# miniforge prompt起動

cd \work\projects\MultiView
jupyter notebook a.ipynb

Run_TwoCam_Ransac.py : 2つの画像に共通に写っているfeature point の座標の組から generates 3d point list

Run_FeatureMatch.py : 2つの画像からfeature point の座標の組のCSVを作る。


