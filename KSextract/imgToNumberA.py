# -*- coding: utf-8 -*-
"""imgToNumberA.py - 数字画像(0-9, A の11クラス)を5層CNNで分類する

データ: Sticker/<CODE>_<IDX>.png  ラベル = CODE[IDX]
        例) 130A_1.png -> "130A"の1要素目 = '3'

使い方:
  python imgToNumberA.py train   [--data Sticker] [--epochs 80] [--test-ratio 0.2]
  python imgToNumberA.py test    [--data Sticker] [--model number_cnn.pt]
  python imgToNumberA.py predict <画像.png> [more.png ...] [--model number_cnn.pt]
"""
import argparse
import os
import random

import numpy as np
import torch
import torch.nn as nn
from PIL import Image
from torch.utils.data import DataLoader, Dataset
from torchvision import transforms as T

CLASSES = "0123456789A"
IMG_SIZE = 8
DEFAULT_MODEL = "number_cnn.pt"


# ---------------------------------------------------------------- データ
def scan_dataset(data_dir):
    """ファイル名から (path, label_id) のリストを作る。不正名はスキップ。"""
    items = []
    for f in sorted(os.listdir(data_dir)):
        if not f.lower().endswith(".png"):
            continue
        stem = os.path.splitext(f)[0]
        try:
            code, idx = stem.rsplit("_", 1)
            label = CLASSES.index(code[int(idx)])
        except (ValueError, IndexError):
            continue
        items.append((os.path.join(data_dir, f), label))
    return items


def stratified_split(items, test_ratio, seed):
    """ラベルごとに test_ratio だけテストへ回す層化分割。"""
    rng = random.Random(seed)
    by_cls = {}
    for i, (_, lab) in enumerate(items):
        by_cls.setdefault(lab, []).append(i)
    test_idx = set()
    for idxs in by_cls.values():
        idxs = idxs[:]
        rng.shuffle(idxs)
        test_idx.update(idxs[: max(1, round(len(idxs) * test_ratio))])
    train = [it for i, it in enumerate(items) if i not in test_idx]
    test = [it for i, it in enumerate(items) if i in test_idx]
    return train, test


def compute_stats(items):
    """学習画像全体の画素平均・標準偏差（0-1スケール）。"""
    vals = []
    for path, _ in items:
        img = Image.open(path).convert("L").resize((IMG_SIZE, IMG_SIZE), Image.BILINEAR)
        vals.append(np.asarray(img, dtype=np.float32))
    x = np.stack(vals) / 255.0
    return float(x.mean()), float(x.std())


def make_transform(mean, std, train):
    ops = []
    if train:  # 学習時のみ軽めの拡張（微小回転・平行移動）
        ops.append(T.RandomAffine(degrees=6, translate=(0.06, 0.06)))
    ops += [T.Resize((IMG_SIZE, IMG_SIZE)), T.ToTensor(), T.Normalize(mean, std)]
    return T.Compose(ops)


class NumberDataset(Dataset):
    def __init__(self, items, transform):
        self.items = items
        self.transform = transform

    def __len__(self):
        return len(self.items)

    def __getitem__(self, i):
        path, label = self.items[i]
        img = Image.open(path).convert("L")
        return self.transform(img), label


# ---------------------------------------------------------------- モデル
class NumberCNN(nn.Module):
    """8x8灰画像 -> 11クラス。conv3層 + fc2層 = 5層。"""

    def __init__(self, n_classes=len(CLASSES)):
        super().__init__()
        self.features = nn.Sequential(
            # 1x8x8 -> 16x8x8 -> 16x4x4
            nn.Conv2d(1, 16, 3, padding=1), nn.BatchNorm2d(16), nn.ReLU(), nn.MaxPool2d(2),
            # 16x4x4 -> 32x4x4 -> 32x2x2
            nn.Conv2d(16, 32, 3, padding=1), nn.BatchNorm2d(32), nn.ReLU(), nn.MaxPool2d(2),
            # 32x2x2 -> 64x2x2
            nn.Conv2d(32, 64, 3, padding=1), nn.BatchNorm2d(64), nn.ReLU(),
        )
        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Linear(64 * 2 * 2, 64), nn.ReLU(), nn.Dropout(0.5),
            nn.Linear(64, n_classes),
        )

    def forward(self, x):
        return self.classifier(self.features(x))


# ---------------------------------------------------------------- 学習・評価
@torch.no_grad()
def evaluate(model, loader, device, criterion=None):
    model.eval()
    total = correct = 0
    loss = 0.0
    n_cls = len(CLASSES)
    conf = np.zeros((n_cls, n_cls), dtype=int)
    for images, labels in loader:
        images, labels = images.to(device), labels.to(device)
        logits = model(images)
        if criterion is not None:
            loss += criterion(logits, labels).item() * labels.size(0)
        pred = logits.argmax(1)
        correct += (pred == labels).sum().item()
        total += labels.size(0)
        for t, p in zip(labels.tolist(), pred.tolist()):
            conf[t, p] += 1
    acc = correct / total if total else 0.0
    return acc, loss / max(total, 1), conf


def log_confusion(conf):
    print("混同行列 (行=正解, 列=予測):")
    print("      " + " ".join(f"{c:>4}" for c in CLASSES))
    for i, c in enumerate(CLASSES):
        print(f"  {c} : " + " ".join(f"{v:>4}" for v in conf[i]))


def do_train(args):
    torch.manual_seed(args.seed)
    random.seed(args.seed)
    np.random.seed(args.seed)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"device: {device}")

    items = scan_dataset(args.data)
    if not items:
        raise SystemExit(f"{args.data} に画像がありません")
    train_items, test_items = stratified_split(items, args.test_ratio, args.seed)
    print(f"画像数: {len(items)}  学習: {len(train_items)}  テスト: {len(test_items)}")

    mean, std = compute_stats(train_items)
    print(f"正規化: mean={mean:.4f} std={std:.4f}")

    train_loader = DataLoader(
        NumberDataset(train_items, make_transform(mean, std, train=True)),
        batch_size=args.batch_size, shuffle=True, num_workers=0)
    test_loader = DataLoader(
        NumberDataset(test_items, make_transform(mean, std, train=False)),
        batch_size=256, shuffle=False, num_workers=0)

    # クラス不均衡対策: 逆頻度重み
    counts = np.bincount([l for _, l in train_items], minlength=len(CLASSES)).astype(np.float64)
    weights = counts.sum() / (len(CLASSES) * np.maximum(counts, 1.0))
    criterion = nn.CrossEntropyLoss(weight=torch.tensor(weights, dtype=torch.float32, device=device))

    model = NumberCNN().to(device)
    n_params = sum(p.numel() for p in model.parameters())
    print(f"モデルパラメータ数: {n_params:,}")
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs)

    best_acc = 0.0
    for epoch in range(1, args.epochs + 1):
        model.train()
        running = 0
        for images, labels in train_loader:
            images, labels = images.to(device), labels.to(device)
            optimizer.zero_grad()
            loss = criterion(model(images), labels)
            loss.backward()
            optimizer.step()
            running += loss.item() * labels.size(0)
        scheduler.step()
        train_loss = running / len(train_loader.dataset)
        acc, test_loss, _ = evaluate(model, test_loader, device)
        mark = ""
        if acc > best_acc:
            best_acc = acc
            mark = "  <-- best"
            torch.save({"model": model.state_dict(), "classes": CLASSES,
                        "mean": mean, "std": std, "img_size": IMG_SIZE,
                        "test_acc": acc, "epoch": epoch}, args.model)
        if epoch % args.log_every == 0 or epoch == 1 or mark:
            print(f"epoch {epoch:3d}/{args.epochs}  train_loss={train_loss:.4f}  "
                  f"test_acc={acc:.4f}{mark}")

    print(f"\n最良テスト精度: {best_acc:.4f}  ->  {args.model} に保存")


def load_model(model_path, device):
    ckpt = torch.load(model_path, map_location=device, weights_only=False)
    model = NumberCNN()
    model.load_state_dict(ckpt["model"])
    model.to(device).eval()
    return model, ckpt


def do_test(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model, ckpt = load_model(args.model, device)
    items = scan_dataset(args.data)
    _, test_items = stratified_split(items, args.test_ratio, args.seed)
    print(f"テスト画像数: {len(test_items)}  (モデル: {args.model}, "
          f"学習epoch={ckpt.get('epoch')}, 学習時test_acc={ckpt.get('test_acc'):.4f})")
    loader = DataLoader(
        NumberDataset(test_items, make_transform(ckpt["mean"], ckpt["std"], train=False)),
        batch_size=256, shuffle=False, num_workers=0)
    acc, loss, conf = evaluate(model, loader, device)
    print(f"テスト精度: {acc:.4f}  loss={loss:.4f}")
    print("\nクラス別再現率:")
    for i, c in enumerate(CLASSES):
        total = conf[i].sum()
        print(f"  {c}: {conf[i, i]}/{total} = {conf[i, i] / total:.4f}" if total else f"  {c}: -")
    log_confusion(conf)


def do_predict(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model, ckpt = load_model(args.model, device)
    classes = ckpt["classes"]
    tf = make_transform(ckpt["mean"], ckpt["std"], train=False)
    for path in args.images:
        img = tf(Image.open(path).convert("L")).unsqueeze(0).to(device)
        with torch.no_grad():
            prob = model(img).softmax(1)[0]
        p = prob.argmax().item()
        top = " ".join(f"{classes[i]}:{prob[i]:.2f}" for i in prob.argsort(descending=True)[:3])
        print(f"{path} -> {classes[p]}  (確率 {prob[p]:.3f} | {top})")


# ---------------------------------------------------------------- 入口
def main():
    ap = argparse.ArgumentParser(description="8x8数字画像を5層CNNで11クラス分類")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("train", help="学習")
    p.add_argument("--data", default="Sticker")
    p.add_argument("--model", default=DEFAULT_MODEL)
    p.add_argument("--epochs", type=int, default=80)
    p.add_argument("--batch-size", type=int, default=64)
    p.add_argument("--lr", type=float, default=1e-3)
    p.add_argument("--test-ratio", type=float, default=0.2)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--log-every", type=int, default=5)

    p = sub.add_parser("test", help="テスト")
    p.add_argument("--data", default="Sticker")
    p.add_argument("--model", default=DEFAULT_MODEL)
    p.add_argument("--test-ratio", type=float, default=0.2)
    p.add_argument("--seed", type=int, default=42)

    p = sub.add_parser("predict", help="画像ファイルから分類")
    p.add_argument("images", nargs="+")
    p.add_argument("--model", default=DEFAULT_MODEL)

    args = ap.parse_args()
    {"train": do_train, "test": do_test, "predict": do_predict}[args.cmd](args)


if __name__ == "__main__":
    main()
