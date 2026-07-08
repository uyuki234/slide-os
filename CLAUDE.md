# slide-os — Claude向けガイド

スライド表示専用の自作OS学習プロジェクト。i686 (32bit x86) フリースタンディング環境。
`kernel_v1/` → `kernel_v2/` → `kernel_v3/` の順に発展していて、**現在の開発対象は `kernel_v3/`**。

## 使っていいコマンド

### ビルド

```sh
make -C kernel_v3            # kernel.elf をビルド
make -C kernel_v3 iso        # GRUB入り起動ISO (slideos.iso) を作成
make -C kernel_v3 clean      # 生成物を削除
```

v1/v2 も同様に `make -C kernel_v1` / `make -C kernel_v2`(ターゲットは all / run / clean のみ)。

### 実行 (QEMU)

```sh
make -C kernel_v3 run-iso    # ISO経由で起動。GUI表示はこれが正規ルート
make -C kernel_v3 run        # -kernel直接ロード。防御チェック確認用(グラフィックス無しで止まるのが正常)
```

- **v3のフレームバッファ(GUI)はISO経由が必須。** QEMUの `-kernel` 直接ロードはMultibootのビデオ要求を無視する。
- シリアルログは `-serial stdio` でQEMUの標準出力に出る。動作確認はまずシリアルログを見る。
- ヘッドレスで確認したいときは `qemu-system-i386 -cdrom kernel_v3/slideos.iso -vga std -serial stdio -display none` のようにオプションを足してよい。スクリーンショットは `-display none` + QEMUモニタの `screendump` などを使う。

### ツールチェーン(直接呼んでもよい)

```sh
i686-elf-gcc ...             # クロスコンパイラ(ホストのgcc/clangは使わない)
nasm -f elf32 ...            # アセンブル
i686-elf-grub-mkrescue ...   # ISO作成(grub-mkrescueではなくこの名前)
qemu-system-i386 ...         # エミュレータ
python3 tools/extract_font.py  # フォントヘッダ生成
```

インストールは `brew install qemu nasm i686-elf-gcc i686-elf-grub`(macOS)。

## 注意事項

- カーネルはフリースタンディング: libc・標準ヘッダ(`stdio.h`等)は使えない。`stdint.h` / `stddef.h` / `stdbool.h` はOK。
- ビルドフラグ: `-ffreestanding -O2 -Wall -Wextra -std=c11`。警告ゼロを維持する。
- Makefileの依存関係追跡は簡易版(ヘッダを変えると全`.o`再ビルド)。挙動が怪しいときは `make clean` してからビルド。
- `.o` / `.elf` / `.iso` / `isodir/` は生成物。手で編集しない。
- QEMUをフォアグラウンドで起動すると戻ってこないので、Bashではバックグラウンド実行かタイムアウト付きで起動する。

## ディレクトリ

| パス | 内容 |
| --- | --- |
| `kernel_v3/` | 現行。GRUB+Multibootでフレームバッファ、ウィンドウ、PS/2マウス |
| `kernel_v2/` | 割り込み・タイマー・キーボード・テキストウィンドウ |
| `kernel_v1/` | 最初の版。VGAテキストでスライド表示 |
| `experiments/bootsector/` | 512バイトのブートセクタ実験 |
| `docs/` | 要件メモ・スクリーンショット |
| `tools/` | フォント抽出スクリプト |
