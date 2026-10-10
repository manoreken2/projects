# KSextract - 数字認識（imgToNumberA.py）

数値画像（切り出し済み1文字画像）を **8x8ピクセルに縮小** し、**5層CNN** で
`0,1,2,3,4,5,6,7,8,9,A` の **11クラス** に分類するプログラムです。

- モデル: 約4万パラメータ、GPU（CUDA）で学習時間 約10秒

## 動作環境 / セットアップ

- Windows + [Miniforge3](https://github.com/conda-forge/miniforge)（`C:\miniforge3`にインストール）
- GPUは必須ではありません（CPUでも動作しますが、学習はGPU推奨）

### conda環境の作成

```bat
:: Miniforge3 promptで実施
conda create -n ImgToTxt python=3.12 -y
conda activate ImgToTxt
conda install numpy pillow -y
pip install torch torchvision
:: NVIDIA GPUがある場合（CUDA 12.8ビルドの例）
:: pip install torch torchvision --index-url https://download.pytorch.org/whl/cu128
```

conda環境を有効化せずに実行する場合は `python` の代わりに
`C:\miniforge3\envs\ImgToTxt\python.exe` を使用してください。

## 学習データ（Sticker/）

`Sticker/<CODE>_<IDX>.png` 形式の画像で、**ラベルはファイル名から自動決定**されます。
`CODE`（4文字）の `IDX` 文字目が正解クラスです。

| ファイル | 意味 | ラベル |
|---|---|---|
| `130A_0.png` | "130A" の 0要素目 | `1` |
| `130A_1.png` | "130A" の 1要素目 | `3` |
| `130A_3.png` | "130A" の 3要素目 | `A` |

同梱データ: 720コード × 4文字 = 2,880枚。
学習・テストの比率は既定80/20、seed指定で再現可能。

## 使い方

すべて本フォルダで実行します。

### 1. 学習

Miniforge3 promptを開き
```bat
conda activate ImgToTxt
python imgToNumberA.py train
```

学習済みモデル `number_cnn.pt`（正規化統計・クラス情報込み）が保存されます。

主なオプション:

| オプション | 既定値 | 説明 |
|---|---|---|
| `--data` | `Sticker` | 学習データフォルダ |
| `--model` | `number_cnn.pt` | モデル保存先 |
| `--epochs` | `80` | エポック数 |
| `--batch-size` | `64` | バッチサイズ |
| `--lr` | `0.001` | 学習率（Adam + CosineAnnealing） |
| `--test-ratio` | `0.2` | テスト分割比率 |
| `--seed` | `42` | 乱数シード（分割の再現に使用） |

### 2. テスト

```bat
python imgToNumberA.py test
```

テスト精度・クラス別再現率・混同行列を表示します。
`train` と同じ `--test-ratio` / `--seed` を使用する場合のみ、学習時に未見のテストセットの評価になります。

### 3. 分類（推論）

```bat
python imgToNumberA.py predict 画像.png [画像2.png ...]
```

1画像について分類結果とトップ3の確率を表示します。

```
Sticker/130A_3.png -> A  (確率 1.000 | A:1.00 8:0.00 0:0.00)
```

## モデル構成

| 層 | 構成 | 出力サイズ |
|---|---|---|
| 1 | Conv3x3(1→16) + BN + ReLU + MaxPool2 | 16x4x4 |
| 2 | Conv3x3(16→32) + BN + ReLU + MaxPool2 | 32x2x2 |
| 3 | Conv3x3(32→64) + BN + ReLU | 64x2x2 |
| 4 | FC(256→64) + ReLU + Dropout(0.5) | 64 |
| 5 | FC(64→11) | 11 |

- 入力画像はグレースケールに変換後、8x8へ縮小されます。
- 入力: グレースケール8x8、学習画像の平均・標準偏差で正規化
- 損失: クラス逆頻度重み付きCrossEntropy（クラス不均衡対策）
- 拡張: 学習時のみ微小回転（±6°）・平行移動（±6%）

## 注意

- 入力画像は、1文字が上下左右ほぼパンパンに切り出された画像を想定しています。しかも、1種類のフォント活字だけを学習しているため汎化性能はありません。しかしながら、この条件下で認識は100％成功します。
