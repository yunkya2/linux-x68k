# Linux for X68000

## 概要

これはX68000でLinuxを動かすプロジェクトです。

68000にはMMUが搭載されていませんが、本プロジェクトではMMUを使用しない組み込み向けLinux(uClinux)を使用します。

**(NEW)** Human68k上のプロセスとして動作するようになりました。Linux側からHuman68kのファイルシステムにアクセスできるようになりました。LinuxからHuman68kに戻ることも可能です。

![Linux for X68000](x68klinux.png)

## 起動方法

- [リリースアーカイブ](https://github.com/yunkya2/linux-x68k/releases)内の `linux.x` と `linux.sys` をHuman68k上の同じディレクトリに置き、`linux.x` を実行してください
- `linux.x`の引数はLinuxカーネルの起動パラメータとして渡されます。引数を省略した場合は`init=/bin/sh`が使用されます
  - 例: `linux.x init=/bin/sh mem=8M`
  - `mem=`はLinuxが使用するメモリの終端を指定します。省略時はX68000のSRAMに記録されたメモリ終端値が使用されます
  - initramfs内の`/init`は`init=`より先に実行されます。デフォルトの`/init`以外を起動する場合は`rdinit=`を使用してください
- `HUMAN.SYS`、`COMMAND.X`、`xdftool.py`を用意して`make hdf`を実行すると、エミュレータから直接起動できる`linux-x68k.hdf`を作成できます。起動後は`AUTOEXEC.BAT`から自動的にLinuxを起動します
- しばらく待つとLinuxカーネルが起動し、シェルプロンプトが表示されます
  - 10MHz機では起動に時間がかかります
- キーボードはX68000のJIS配列に対応しています
- デフォルトのinitramfsは`humanfs`を`/mnt`にマウントします。メディアが挿入されているHuman68kのドライブを`/mnt/c`、`/mnt/d`などからアクセスできます
- 起動したシェルをCTRL-Dで終了させる、Interruptスイッチを押す、バスエラー等のエラーの発生により、Linuxを終了してHuman68kに戻ります

## 制約事項

- X68000専用です。X68030など68000以外のCPUを搭載した機種では動作しません
- 起動には最低6MBのメモリが必要です

## ビルド方法

ビルドはUbuntu-24.04でのみ確認しています。

ソースコードリポジトリを`--recursive`オプション付きでclone後、
```
make everything
```
でtoolchainやユーザランドを含めた全てのビルドが行えます。

一度ビルドした後は、以下のコマンドでビルド構成を変更できます。

- `make linux` : Linuxカーネルの再ビルド (ユーザランドを変更した場合の取り込み)
- `make linux-menuconfig` : Linuxカーネルの設定変更
- `make linux-savedefconfig` : 変更したconfigの保存 (`linux/arch/m68k/configs/x68k_defconfig`に保存されます)
- `make buildroot` : ユーザランドの再ビルド
- `make buildroot-menuconfig` : ユーザランドの設定変更
- `make buildroot-savedefconfig` : ユーザランドの設定変更の保存 (`buildroot/configs/x68k_defconfig`に保存されます)
- `make busybox` : busyboxの再ビルド
- `make busybox-menuconfig` : busyboxの設定変更
- `make busybox-update-config` : busyboxの設定変更の保存 (`buildroot/package/busybox/busybox-minimal.config`に保存されます)

## 色々

- 以下のデバイスのみサポートしています
  - MFP Timer-C (tick timer)
  - MFP USART (JISキーボード入力)
  - IOCSテキストコンソール
  - 起動初期のシリアルおよびIOCSコンソール出力 (`CONFIG_X68K_EARLY_CONSOLE`有効時のみ)
  - Human68k DOSコールでファイルシステムへアクセスする`humanfs`
- Human68k上の`linux.x`は自身のメモリブロックを最大まで拡張し、自身と同じディレクトリの`linux.sys`を8192バイト境界にロードして実行します
- ユーザランドのCライブラリにはuClibcを使用しています
  - gccを`m68k-uclinux-uclibc`というターゲットでビルドしていますが、このターゲットでgccを作るとCPU種別に関係なくunaligned accessが可能な設定でコードが出力されてしまうため、68000で実行するとアドレスエラーが発生してしまいます。これを修正するための[パッチ](https://github.com/yunkya2/buildroot/commit/3d4d43fa887fcd5f42927d5c2869d8c9df79d8d2)をbuildrootに追加しています
- X68k版では100HzのTimer-C割り込みでタスクスケジューリングを行っていますが、10MHz機だとその処理が終わる前に次の割り込み周期が来てしまい、起動中に初期化処理が先に進まなくなってしまいます。このため、tick処理の呼び出し周期を下げるためのオプション (`CONFIG_X68K_LEGACY_TICK_DIVISOR`) を追加し、割り込み10回に1回だけtick処理を呼び出すようにしています。

## 謝辞

X68000用Linuxは、[Atari Jaguar用Linux](https://cakehonolulu.github.io/linux-for-jaguar/)を参考に開発しました。
開発者のcakehonolulu氏に感謝します。
