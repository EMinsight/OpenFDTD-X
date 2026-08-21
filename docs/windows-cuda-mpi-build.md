# Windows で CUDA + MPI 版カーネルを使う (管理者権限なしの手順)

OpenFDTD-X の実行エンジン「CPU+MPI」「GPU (CUDA)」「GPU+MPI」は、それぞれ
カーネルの `<kernel>_mpi` / `<kernel>_cuda` / `<kernel>_cuda_mpi` を起動する。
本書は **Windows PC で CUDA Toolkit も MS-MPI もインストールせず
(管理者権限なしで)** これらをビルドして GUI から使うまでの手順と、
2026-08-21 に実機 (RTX 3060 / CUDA 13.1 / MS-MPI 10.1 / MSVC 14.36) で
検証した結果をまとめる。

## 結果の要約

| カーネル | CPU | CPU+MPI | GPU (CUDA) | GPU+MPI | 備考 |
|---|---|---|---|---|---|
| OpenFDTD `ofd` | ✓ | ✓ (CPU とビット一致) | ✓ | ✓ | CUDA 版は float の和の順序で収束履歴の 6 桁目が異なるのみ。インピーダンス表は表示精度で一致 |
| OpenRCWA `orcwa` | ✓ | ✓ | ✓ | ✓ (*) | **MPI / CUDA 版は FDTD 専用** — RCWA / FMM (`rcwalayer`) は `orcwa` (CPU) だけが計算できる (他は理由を出して終了) |
| OpenBPM `obpm` | ✓ | ✓ (*) | ✓ | ✓ (*) | **MPI 版 (`obpm_mpi` / `obpm_cuda_mpi`) は FDTD 専用** — BPM は `obpm` (CPU) か `obpm_cuda` |
| bellhopcuda `bellhopcxx` | ✓ | — | ✓ (`bellhopcuda`) | — | 水中音響。**MPI 版は存在しない** (GUI もその旨を表示する)。GPU 版は CPU 版と別名の実行ファイル。CUDA の対象アーキテクチャは同リポジトリの CMake が `native` (実機自動判定) |

(*) `orcwa_cuda_mpi` / `obpm_mpi` / `obpm_cuda_mpi` は **2 ランク以上では
`time_series_data.h5` を書かない** (理由を出して出力だけ止める)。並列 HDF5 の
集団操作を rank 0 だけが呼ぶ構造で、2 ランク以上では H5Fclose で待ち合って
デッドロックしていた (実測: 1 ランクは通り 2 ランクでハング)。`.log` / `.out`
(→ `*_post`) は従来どおり出る。`orcwa_mpi` だけは集団書き込みに直されており
HDF5 も出る。

GUI は上の「仕様上できない組合せ」を `Runner::engineUnsupportedReason` で
判定し、エンジンの選択肢を理由つきで無効化する (実行直前にも再確認する)。
バイナリや mpiexec が見つからない場合は従来どおり `Runner::checkAvailability`
が無効化する。

## 1. ツールチェーンの導入 (すべてユーザー空間)

| 部品 | 入手方法 | 置き場所の例 |
|---|---|---|
| MSVC + Ninja + CMake | Visual Studio Build Tools 2022 (既存) — `vcvars64.bat` で有効化 | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` |
| CUDA 13.1 (portable) | NVIDIA の redist アーカイブ (zip) を展開して合成する: `cuda_nvcc`, `libnvvm`, `cuda_cudart`, `cuda_cccl`, `cuda_crt` (+ `cuda_nvml_dev`, `cuda_nvtx`, `cuda_profiler_api`)。<https://developer.download.nvidia.com/compute/cuda/redist/> の `redistrib_<ver>.json` に URL がある。インストーラ不要・管理者権限不要 | `C:\Users\<you>\tools\cuda-13.1` (`CUDA_PATH` に設定) |
| MS-MPI 10.1 | conda-forge の `msmpi` パッケージ (tar.bz2) を展開するだけ。`Library\bin` に `mpiexec.exe` / `smpd.exe` / `msmpi.dll`、`Library\include` に `mpi.h`、`Library\lib` に `msmpi.lib`。**SMPD サービスを登録しなくても単一ノードなら `mpiexec -n N` が動く** | `C:\Users\<you>\tools\msmpi` (`MSMPI_BIN` / `MSMPI_INC` / `MSMPI_LIB64` に設定) |
| HDF5 (直列) + zlib + Eigen3 | vcpkg (`hdf5[core,zlib]:x64-windows-static-md eigen3:x64-windows-static-md` — カーネルの CI と同じ) | `C:\Users\<you>\tools\vcpkg` |
| HDF5 (並列, MS-MPI) | OpenRCWA / OpenBPM の MPI 版は `H5Pset_fapl_mpio` を使うので並列 HDF5 が要る。HDF5 1.14.6 のソースを `-DHDF5_ENABLE_PARALLEL=ON -DBUILD_SHARED_LIBS=OFF` で自前ビルドする (MS-MPI の環境変数で FindMPI が見つける)。OpenFDTD の MPI 版は rank 0 の直列ドライバで書くので不要 | `C:\Users\<you>\tools\hdf5-1.14.6-parallel-msmpi` |
| Qt 6.8.3 (GUI 用) | `pip install aqtinstall` → `aqt install-qt windows desktop 6.8.3 win64_msvc2022_64` | `C:\Users\<you>\Qt\6.8.3\msvc2022_64` |

ドライバは CUDA 13.1 対応 (`nvidia-smi` の CUDA Version) であること。
GPU は sm_75 以降 (CUDA 13 は Pascal / Volta のサポートを打ち切った)。

### 環境設定スクリプトの例 (`devenv.cmd`)

```bat
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set TOOLS=C:\Users\%USERNAME%\tools
set CUDA_PATH=%TOOLS%\cuda-13.1
set MSMPI_BIN=%TOOLS%\msmpi\Library\bin
set MSMPI_INC=%TOOLS%\msmpi\Library\include
set MSMPI_LIB64=%TOOLS%\msmpi\Library\lib
set VCPKG_ROOT=%TOOLS%\vcpkg
set QT_ROOT=C:\Users\%USERNAME%\Qt\6.8.3\msvc2022_64
set PATH=%CUDA_PATH%\bin;%MSMPI_BIN%;%QT_ROOT%\bin;%VCPKG_ROOT%;%PATH%
```

バッチファイルは ASCII で書く (cmd.exe は OEM コードページで読むため、
日本語コメントや括弧を含む `rem` が `( )` ブロック内にあると壊れる)。

## 2. カーネルのビルド

各リポジトリ (`OpenFDTD` / `OpenRCWA` / `OpenBPM`) を clone し、構成ごとに
別のビルドディレクトリで configure する。出力は各リポジトリの `bin\` に
集まる (CMake の `EXECUTABLE_OUTPUT_PATH`)。

```bat
rem 共通: Ninja + Release。RTX 30xx は sm_86 (40xx は 89)
set OPTS=-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=86
set VCPKG=-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static-md

rem CPU / CUDA / MPI (OpenFDTD) — 直列 HDF5 (vcpkg)
cmake -B build-cpu      %OPTS% %VCPKG% -DWITH_CUDA=OFF -DWITH_MPI=OFF && cmake --build build-cpu      -j
cmake -B build-cuda     %OPTS% %VCPKG% -DWITH_CUDA=ON  -DWITH_MPI=OFF && cmake --build build-cuda     -j
cmake -B build-mpi      %OPTS% %VCPKG% -DWITH_CUDA=OFF -DWITH_MPI=ON  && cmake --build build-mpi      -j
cmake -B build-cuda-mpi %OPTS% %VCPKG% -DWITH_CUDA=ON  -DWITH_MPI=ON  && cmake --build build-cuda-mpi -j

rem OpenRCWA / OpenBPM の MPI 構成 — 並列 HDF5 (vcpkg のツールチェーンは使わず、
rem HDF5 は HDF5_DIR で、Eigen3 は vcpkg のインストール先から直接指す)
set PAR=-DHDF5_DIR=%TOOLS%\hdf5-1.14.6-parallel-msmpi\lib\cmake\hdf5 -DHDF5_USE_STATIC_LIBRARIES=ON -DHDF5_PREFER_PARALLEL=ON -DEigen3_DIR=%VCPKG_ROOT%\installed\x64-windows-static-md\share\eigen3
cmake -B build-mpi      %OPTS% %PAR% -DWITH_CUDA=OFF -DWITH_MPI=ON && cmake --build build-mpi      -j
cmake -B build-cuda-mpi %OPTS% %PAR% -DWITH_CUDA=ON  -DWITH_MPI=ON && cmake --build build-cuda-mpi -j
```

### カーネル側で直したこと (2026-08-21、3 リポジトリ共通)

- `CMAKE_CUDA_ARCHITECTURES` が 60 に固定され利用者指定を無視していた。
  CUDA 13 では `compute_60` がビルドできないため、利用者指定を尊重し、
  未指定なら nvcc < 13 → 60 / nvcc ≥ 13 → 75 とした。
- MSVC に存在しない `pthread` / `m` の直リンクを `Threads::Threads` /
  `${MATH_LIB}` へ。`/utf-8` は C/C++ にだけ渡し、CUDA には
  `-Xcompiler=/utf-8` で中継 (nvcc は `/` オプションを受けない)。
  MPI / CUDA 実行ファイルにも `/STACK:16777216` (CPU 版と同じ)。
- OpenFDTD: **HDM (既定の host/device 分離メモリ) で `ofd_cuda` /
  `ofd_cuda_mpi` が最初の出力ステップで 0xC0000005 で落ちる**バグを修正。
  HDF5 スナップショットが device メモリの Ex..Hz を host から読んでいた
  (UM と `-cpu` では出ないため、GPU の無い CI では検出できなかった)。
- OpenRCWA: `hdf5::hdf5` → `HDF5::HDF5` に統一 (FindHDF5 が常に作る名前)。
  GPU 版 RCWA のテストは gdstk / cuSOLVER / MAGMA に依存するので
  `WITH_RCWA_LEGACY_TESTS` (MSVC 既定 OFF) で切り離した。MPI / CUDA 版の
  main は RCWA 入力 (`rcwalayer`) を受けると理由を出して終了する
  (以前はメッシュ 0 のまま走って落ちていた)。
- **3 リポジトリ共通: `cuda/solve.cu` だけ収束履歴の最後の 1 点が落ちていた**。
  `Niter` の加算が収束判定の `break` より後にあり、収束して抜けた点が
  記録されない (CPU / MPI / CUDA+MPI は格納直後に加算しており、この実装
  だけがずれていた)。実測では `convergence/iter` が CPU の 0..550 (12 点)
  に対し CUDA は 0..500 (11 点)。**解析解との比較にもログの回帰基準値にも
  現れないため、HDF5 の中身を構成間で突き合わせて初めて分かる種類の不具合。**

### bellhopcuda (水中音響) のビルド

CUDA の対象アーキテクチャは同リポジトリの CMake が `native` (実機から自動
判定) にしているので、指定は要らない。glm サブモジュールの取得が必須。

```bat
git clone https://github.com/Sirokujira/bellhopcuda.git
cd bellhopcuda && git submodule update --init --recursive
cmake -B build-cuda -G Ninja -DCMAKE_BUILD_TYPE=Release -DBHC_ENABLE_CUDA=ON -DBHC_BUILD_EXAMPLES=OFF
cmake --build build-cuda -j --target bellhopcxx bellhopcuda   rem → bin\
```

MPI 版は存在しない (GUI もエンジン選択でその旨を出す)。CUDA 版は
テンプレート実体化が多く、ビルドに数分〜十数分かかる。

## 2.5 ビルドした実行ファイルを単体で起動できるようにする

ビルド直後の実行ファイルは、**ビルドに使ったシェル (Qt や MS-MPI を PATH に
持つシェル) からしか起動できない**。Explorer からのダブルクリックや素の
コマンドプロンプトでは `0xC0000135` (DLL not found) で**何も表示されずに落ちる**。
依存を実行ファイルの隣へ置いて解決する。

| 実行ファイル | 素の環境で起動できるか | 必要な措置 |
|---|---|---|
| `openfdtd_x.exe` / `openuwa.exe` | ✗ Qt6Core/Gui/Widgets/Network.dll が要る | `windeployqt` (下記) |
| `ofd.exe` / `ofd_cuda.exe` / `bellhopcxx.exe` / `bellhopcuda.exe` | ✓ そのまま起動できる | **不要** — CUDA ランタイムも静的リンクされている |
| `*_mpi.exe` / `*_cuda_mpi.exe` | ✗ `msmpi.dll` が要る | `msmpi.dll` を隣へコピー (または `%MSMPI_BIN%` を PATH へ) |

```bat
rem GUI: Qt の DLL とプラグインを実行ファイルの隣へ配置する
"%QT_ROOT%\bin\windeployqt.exe" --release --no-translations ^
    --no-system-d3d-compiler --no-opengl-sw build\openfdtd_x.exe
"%QT_ROOT%\bin\windeployqt.exe" --release --no-translations ^
    --no-system-d3d-compiler --no-opengl-sw build\openuwa.exe

rem ヘッドレス実行 (--screenshot / CI) をするなら offscreen プラグインも要る。
rem windeployqt は qwindows.dll しか置かないので手でコピーする。
rem これが無いと QT_QPA_PLATFORM=offscreen で「プラグインが見つからない」の
rem モーダルダイアログが出たまま止まる (バッチからだと無応答に見える)。
copy "%QT_ROOT%\plugins\platforms\qoffscreen.dll" build\platforms\
copy "%QT_ROOT%\plugins\platforms\qminimal.dll"   build\platforms\

rem MPI 版カーネル: msmpi.dll を隣へ
copy "%MSMPI_BIN%\msmpi.dll" C:\Users\<you>\Downloads\OpenFDTD\bin\
```

GUI から起動する分には `Runner` が mpiexec のフォルダを子プロセスの PATH に
足すので `msmpi.dll` のコピーは不要だが、`*_mpi.exe` を手で叩くときに要る。

**カーネルの場所は GUI の設定に入れる。** Explorer から起動したアプリには
`OPENFDTD_HOME` 等の環境変数が届かないため、**ツール → カーネルパスの設定…**
で指定する (QSettings に永続化され、次回以降どの起動経路でも有効)。設定前は
ステータスバーに「⚠ カーネル未検出: ofd.exe」が出る。

## 3. GUI 側の設定

1. **ツール → カーネルパスの設定…** で各リポジトリのルートを指定する
   (`bin\` も探索される)。環境変数 `OPENFDTD_HOME` 等でもよい。
2. 同じダイアログの **並列実行 (MPI) → MPI ランチャ (mpiexec)** に
   `mpiexec.exe` を指定する。空欄なら PATH → `$MSMPI_BIN` →
   `C:\Program Files\Microsoft MPI\Bin` → 各カーネルディレクトリ (直下と
   `bin\`) の順に自動探索する。Runner は見つけた mpiexec のフォルダを
   子プロセスの PATH の先頭に足すので、`msmpi.dll` がシステムに無くても
   `<kernel>_mpi.exe` が起動する。
3. ツールバーの **リソース** で MPI プロセス数 (`mpiexec -n`)、
   **スレッド数** で OpenMP スレッド数 (`-n <thread>`) を設定する。
   GPU 系エンジンでは **デバイス番号** が `CUDA_VISIBLE_DEVICES` になる
   (CUDA 版は `-n` を受け付けないので渡さない)。

検出結果は画面を出さずに確認できる:

```bash
openfdtd_x --check-kernels
```

各カーネルの CPU / CPU+MPI / GPU / GPU+MPI / post の解決パス、mpiexec の
所在、MPI / CUDA エンジンが使えない理由が UTF-8 で出力される。

## 4. 検証 (2026-08-21, RTX 3060, dipole サンプル)

| 実行 | 結果 |
|---|---|
| `ofd -n 4 dipole.ofd` | converged, normal end |
| `ofd_cuda dipole.ofd` | converged (HDM 修正後)。UM (`-um`) / `-cpu` も可 |
| `mpiexec -n 2 ofd_mpi -n 2 dipole.ofd` | converged、収束履歴は CPU と**ビット一致** |
| `mpiexec -n 2 ofd_cuda_mpi dipole.ofd` | converged (2 ランクが同じ GPU 0 を共有) |
| `orcwa` / `orcwa_cuda` (FDTD 入力) | normal end |
| `orcwa -n 4 grating.ofd` (RCWA) | normal end (`rcwa_efficiency.csv`) |
| `orcwa_cuda grating.ofd` | 理由を出して終了 (RCWA は CPU 版のみ) |
| `mpiexec -n 2 orcwa_mpi -n 2 dipole.ofd` | converged, normal end (HDF5 あり) |
| `mpiexec -n 2 orcwa_cuda_mpi dipole.ofd` | converged, normal end (HDF5 は 2 ランクでは書かない旨を表示) |
| `obpm` / `obpm_cuda` (fiber.ofd) | normal end |
| `mpiexec -n 2 obpm_mpi` / `obpm_cuda_mpi` (FDTD 入力 dipole.ofd) | converged, normal end (同上。修正前は 2 ランクで H5Fclose 待ちのままハング) |
| `bellhopcxx MunkB_Coh` / `bellhopcuda MunkB_Coh` | 両方 normal end。`.shd` (4 MB) を同リポジトリ同梱の `compare_shdfil.py` (ULP 単位の比較) で突き合わせ、**差分なし** |

インピーダンス表 (21 周波数 × Rin/Xin/Ref/VSWR) は 4 構成とも表示精度で
一致。CUDA 版の収束履歴 `<E>` `<H>` は 6 桁目で ±1 の差 (float の総和順序)。

### HDF5 出力の中身の突き合わせ (h5py)

ログや解析解では見えない差を潰すため、`time_series_data.h5` の**全データ
セット**を構成間で比較した (dipole、CPU を基準、複合型は成分ごとに数値比較):

| 構成 | 最大相対差 | 判定 |
|---|---|---|
| `mpiexec -n 2 ofd_mpi` | 3.4e-16 (`convergence/H`) | 実質ビット一致 |
| `mpiexec -n 4 ofd_mpi` | 1.0e-15 (`convergence/H`) | 同上 (分割数を変えても不変) |
| `ofd_cuda` (HDM 既定) | 1.1e-6 (`freqdomain/H`) | 単精度 DFT の差、想定内 |
| `ofd_cuda -um` | 1.1e-6 (同上) | HDM と同値 |
| `mpiexec -n 2 ofd_cuda_mpi` | 1.1e-6 (同上) | 同上 |

- `metadata/input_impedance` (複合型) も全 7 成分を数値比較して一致。
- **CPU 版にしかない `loss/P_loss` を除き、データセットの構成も一致**する
  (熱解析レイヤは `ofd` のみ — README「実装ごとの対応状況」のとおり)。
- この比較で上記の `Niter` 欠落 (CUDA の `convergence/*` が 12 点 → 11 点)
  を検出した。修正後は 5 構成すべてが `Niter = 12` / `iter` 末尾 550 で一致。

### GUI の統合テスト (実カーネル + HDF5) を全部走らせる

`ofdx_selftest` には**環境変数で有効になる統合テスト**があり、既定では
skip される。実カーネルを揃えたこの PC では全部走らせられる:

```bat
cmake -B build-h5 -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=%QT_ROOT% ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md -DUSE_HDF5=ON
cmake --build build-h5 -j
set QT_QPA_PLATFORM=offscreen
set OFDX_OFD_BIN=C:\Users\<you>\Downloads\OpenFDTD\bin\ofd.exe
set OFDX_BELLHOP_BIN=C:\Users\<you>\Downloads\bellhopcuda\bin\bellhopcxx.exe
build-h5\ofdx_selftest.exe
```

| ビルド / 環境 | 結果 |
|---|---|
| 既定 (`USE_HDF5=OFF`、カーネル無し) | 24 files, **11,250 checks**, 0 failures |
| `USE_HDF5=ON` + `OFDX_OFD_BIN` + `OFDX_BELLHOP_BIN` | 24 files, **11,447 checks**, 0 failures |

差の +197 checks が、これまでこの PC で一度も動いていなかった分:
H5 リーダ (`io/H5Reader` — カーネルの `time_series_data.h5` を GUI が読む経路)、
`ofd` 統合 (実行 → `ofd.log` の給電点表を GUI のパーサで読む → GUI の
Courant 推定がカーネルの `Dt` と一致することの確認)、bellhop 統合
(GUI の `BellhopIO` が書いた `.env` を実カーネルで走らせ `.shd` 生成まで)。
`OFDX_PEEC_BIN` / `OFDX_OFE_BIN` (回路抽出) だけは対応リポジトリを未ビルドの
ため skip のまま。

GUI の画面でも、カーネルを設定するとステータスバーの
「⚠ カーネル未検出: bellhopcxx.exe」が消えて「● 準備完了」になることを
4 ドメインのスクリーンショットで確認した (`--screenshot`。Windows では
`QT_QPA_FONTDIR=C:\Windows\Fonts` が要る)。

## 5. 既知の制約

- CUDA 版は単精度 (`real_t = float`) のため CPU 版とビット一致しない。
- 単一 GPU の PC で GPU+MPI を使ってもランクは同じ GPU を共有するだけで
  速くはならない (マルチ GPU / 複数ノード向けのエンジン)。
- OpenRCWA の GPU 版 RCWA ソルバ (`gpu/`, cuSOLVER + MAGMA) は Windows では
  ビルド対象外 (テスト専用経路)。GUI が起動する `orcwa_cuda` は FDTD の
  CUDA 版である。
- Windows の `QT_QPA_PLATFORM=offscreen` でスクリーンショットを撮るときは
  `QT_QPA_FONTDIR=C:\Windows\Fonts` を設定しないと文字が描画されない。
- `orcwa_cuda_mpi` / `obpm_mpi` / `obpm_cuda_mpi` の並列 HDF5 は**出力を
  止めているだけで直っていない** (上記 (*))。根本修正は `orcwa_mpi` と同じ
  「グローバル添字での集団書き込み」への作り替えで、`OpenRCWA/mpi/solve.c`
  が手本になる (同ファイルの `g_Ni` / `w_i0` 群と `H5FD_MPIO_COLLECTIVE` の
  書き込み、および metadata 節を全 rank で作成し書き込みだけ rank 0 に
  限定する形)。`OpenBPM/mpi/solve.c` は `OpenRCWA/mpi/solve.c` の修正前版と
  include 3 行を除いてバイト一致なので、そのまま移植できる。
