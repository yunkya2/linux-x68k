# Linux for X68000

## 概要

これはX68000でLinuxを動かすプロジェクトです。

68000にはMMUが搭載されていませんが、本プロジェクトではMMUを使用しない組み込み向けLinux(uClinux)を使用します。

![Linux for X68000](x68klinux.png)

## 起動方法

- [リリースアーカイブ](https://github.com/yunkya2/linux-x68k/releases)内の `loader.x` と `vmlinux.bin` をHuman68k上で同じディレクトリに置き、 `loader.x` を実行してください
- しばらく待つとLinuxカーネルが起動し、シェルプロンプトが表示されます
  - シェルプロンプトが出るまで、16MHz機で1分半ほどかかります。気長に待ってください
  - キーボードはUS配列になっています
- 起動すると本体の電源スイッチが効かなくなります。電源を切る際は一度リセットしてください

## 制約事項

- X68000専用です。X68030など68000以外のCPUを搭載した機種では動作しません
- 起動には最低6MBのメモリが必要です
- 10MHz機では動作しません。16MHz機(XVI/Compact)やPhantomX等のアクセラレータ環境、エミュレータなどを使用してください
  - 10MHzだとカーネル初期化中に固まる現象が分かっています(未調査)

## 色々

- 以下のデバイスのみサポートしています
  - MFP Timer-C (tick timer)
  - MFP USART (キー入力)
  - カーネルデバッグコンソール出力
  - グラフィックVRAMによるフレームバッファコンソール
    - Linux kernelのsimple framebufferはX68000のテキストVRAMのようなプレーン形式をサポートしていないため、グラフィック画面をコンソール出力に使用しています
    - simple framebufferが対応しているピクセルフォーマットは(MSB側から見て)RGBの並びですがX68000はGRBの順に並んでいるため、カラーパレットを用いてRとGを入れ替えています
- Linuxカーネルはアドレス 0x004000- に配置されます
  - Human68k上のローダーはmalloc()で確保した領域に一旦ロードした後、0x004000-にコピーしてからジャンプします
- ユーザランドのCライブラリにはuClibcを使用しています
  - gccを`m68k-uclinux-uclibc`というターゲットでビルドしていますが、このターゲットでgccを作るとCPU種別に関係なくunaligned accessが可能な設定でコードが出力されてしまうため、68000で実行するとアドレスエラーが発生してしまいます。これを修正するための[パッチ](https://github.com/yunkya2/buildroot/commit/3d4d43fa887fcd5f42927d5c2869d8c9df79d8d2)をbuildrootに追加しています

## 謝辞

X68000用Linuxは、[Atari Jaguar用Linux](https://cakehonolulu.github.io/linux-for-jaguar/)を参考に開発しました。
開発者のcakehonolulu氏に感謝します。
