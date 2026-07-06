# slide-os

スライドを表示することだけを目的にした、自作OS(風)の学習プロジェクト。
「main() の前には何があるのか」「printf はなぜ動くのか」を、
OSのない世界で実際にコードを書いて確かめた記録です。

## リポジトリ構成

| ディレクトリ | 内容 |
| --- | --- |
| [`kernel_v1/`](kernel_v1/) | 本体。QEMU上でMultibootカーネルとして起動し、VGAテキスト画面にスライドを表示する。`N`/`P`キーでページ送り。 |
| [`experiments/bootsector/`](experiments/bootsector/) | 最初の実験。BIOSが直接実行する512バイトのブートセクタで文字列を表示する。 |

仕組みの詳しい解説は [kernel_v1/README.md](kernel_v1/README.md) を参照。

## クイックスタート (macOS)

```sh
brew install qemu nasm i686-elf-gcc

cd kernel_v1
make run
```

QEMUが起動し、スライドが表示されます。`N` で次、`P` で前のスライドへ。

## ブートセクタ実験を動かす

```sh
cd experiments/bootsector
nasm -f bin boot.asm -o boot.bin
qemu-system-i386 -drive format=raw,file=boot.bin
```

## ライセンス

[MIT](LICENSE)
