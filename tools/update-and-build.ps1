<#
update-and-build.ps1 — 関連リポジトリを git pull して一括ビルドする (Windows)

OpenFDTD-X (GUI) と、GUI が subprocess で起動するソルバーカーネル群は別々の
リポジトリにある。全部を手で pull してビルドし直すのは手間なので、まとめる。

  GUI      OpenFDTD-X       openfdtd_x / openuwa
  FDTD     OpenFDTD         ofd / ofd_mpi / ofd_cuda / ofd_cuda_mpi / ofd_post
  RCWA     OpenRCWA         orcwa / orcwa_mpi / orcwa_cuda / orcwa_cuda_mpi / orcwa_post
  BPM      OpenBPM          obpm  / obpm_mpi  / obpm_cuda  / obpm_cuda_mpi  / obpm_post
  水中音響  bellhopcuda      bellhopcxx / bellhopcuda
  室内音響  OpenAcoustics    ofdx_acoustic_fdtd ほか (OpenMP のみ)

ビルドする構成は実行環境から自動判定する (nvcc があれば CUDA、mpiexec があれば
MPI)。無いものは黙って飛ばさず、最後の一覧に理由を出す。

Windows 固有の後始末までやる:
  - GUI に windeployqt で Qt の DLL とプラグインを配置する (これをやらないと
    Explorer からのダブルクリックが 0xC0000135 で無言のまま落ちる)
  - ヘッドレス用に qoffscreen.dll / qminimal.dll も置く (windeployqt は置かない)
  - MPI 版カーネルの隣に msmpi.dll を置く (手で叩いたときに動くように)

macOS / Linux は同じディレクトリの update-and-build.sh を使う。

使い方:
  powershell -ExecutionPolicy Bypass -File tools\update-and-build.ps1
  ... -Repos gui,ofd        対象を絞る
  ... -Configs cpu          CPU 版だけ
  ... -NoPull -Tests        pull せずビルドしてテストまで
  ... -Clone                無いリポジトリは clone してくる

事前に必要なもの (無い構成は自動で飛ばす):
  Visual Studio 2022 Build Tools / CMake / Ninja
  vcpkg      : hdf5[core,zlib] と eigen3 を x64-windows-static-md で入れておく
  Qt         : $env:QT_ROOT か C:\Qt\<版>\msvc*_64
  CUDA       : $env:CUDA_PATH か PATH の nvcc
  MS-MPI     : $env:MSMPI_BIN か "C:\Program Files\Microsoft MPI\Bin"
  並列 HDF5  : $env:OFDX_HDF5_PARALLEL_DIR (OpenRCWA / OpenBPM の MPI 構成で必要)
#>
[CmdletBinding()]
param(
    [string] $Root,
    [string] $Repos   = "gui,ofd,orcwa,obpm,bellhop,acoustics",
    [string] $Configs = "auto",
    [int]    $Jobs    = 0,
    [string] $CudaArch,
    [switch] $NoPull,
    [switch] $Clone,
    [switch] $Clean,
    [switch] $Tests
)

$ErrorActionPreference = "Continue"
$GitBase = "https://github.com/Sirokujira"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$guiDir    = Split-Path -Parent $scriptDir
if (-not $Root) { $Root = Split-Path -Parent $guiDir }
if ($Jobs -le 0) { $Jobs = [int]$env:NUMBER_OF_PROCESSORS; if ($Jobs -le 0) { $Jobs = 4 } }

$dirOf = @{ gui = "OpenFDTD-X"; ofd = "OpenFDTD"; orcwa = "OpenRCWA";
            obpm = "OpenBPM"; bellhop = "bellhopcuda"; acoustics = "OpenAcoustics" }
$want  = $Repos.Split(",") | ForEach-Object { $_.Trim() }

$results = New-Object System.Collections.ArrayList
function Record($state, $target, $note) { [void]$results.Add([pscustomobject]@{ 状態=$state; 対象=$target; 備考=$note }) }
function Head($text) { Write-Host ""; Write-Host "== $text" -ForegroundColor Cyan }
# 外部コマンドを実行してログへ追記する。PowerShell 5.1 の *> は UTF-16 で書くため
# 後から読めない (1 文字ごとに NUL が挟まる)。文字列化して UTF-8 で残す。
function Invoke-Logged($log, $append, [string[]] $cmdline) {
    $exe = $cmdline[0]
    # 注: $cmdline[1..0] は要素 1 個のとき逆順スライスになり引数が 1 個できて
    #     しまう (selftest に自分自身のパスを渡してしまった)。明示的に分ける。
    if ($cmdline.Length -gt 1) { $rest = @($cmdline[1..($cmdline.Length - 1)]) } else { $rest = @() }
    $out = & $exe @rest 2>&1 | ForEach-Object { $_.ToString() }
    $code = $LASTEXITCODE
    if ($append) { $out | Out-File -FilePath $log -Encoding utf8 -Append }
    else         { $out | Out-File -FilePath $log -Encoding utf8 }
    return $code
}
function Warn($text) { Write-Host "! $text" -ForegroundColor Yellow }
function Fail($text) { Write-Host "x $text" -ForegroundColor Red }

# ── MSVC の環境を取り込む ────────────────────────────────────────────────
# cl.exe / cmake / ninja に PATH を通すため vcvars64.bat の結果を移植する。
function Import-VcVars {
    if ($env:VSCMD_ARG_TGT_ARCH -eq "x64") { return $true }   # 既に開発者シェル
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $bat = $null
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -property installationPath 2>$null
        if ($vs) { $bat = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat" }
    }
    if (-not $bat -or -not (Test-Path $bat)) {
        foreach ($e in @("BuildTools", "Community", "Professional", "Enterprise")) {
            $c = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\$e\VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $c) { $bat = $c; break }
        }
    }
    if (-not $bat -or -not (Test-Path $bat)) { return $false }
    cmd /c "`"$bat`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
    }
    return $true
}

# ── 環境の検出 ────────────────────────────────────────────────────────────
Head "環境の検出"
if (-not (Import-VcVars)) { Fail "Visual Studio 2022 の vcvars64.bat が見つかりません"; exit 1 }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Fail "cmake が見つかりません"; exit 1 }

# CUDA。インストーラ版は CUDA_PATH が立つ。管理者権限なしで redist を展開した
# 構成 (tools\cuda-13.1 など) は環境変数が無いので、よくある置き場所も見る。
if (-not $env:CUDA_PATH) {
    $c = Get-ChildItem "$env:USERPROFILE\tools\cuda-*" -Directory -ErrorAction SilentlyContinue |
         Where-Object { Test-Path (Join-Path $_.FullName "bin\nvcc.exe") } |
         Sort-Object Name -Descending | Select-Object -First 1
    if ($c) { $env:CUDA_PATH = $c.FullName }
}
if ($env:CUDA_PATH -and (Test-Path "$env:CUDA_PATH\bin\nvcc.exe")) {
    $env:PATH = "$env:CUDA_PATH\bin;$env:PATH"
}
$nvcc     = Get-Command nvcc -ErrorAction SilentlyContinue
$haveCuda = [bool]$nvcc

# MS-MPI (mpiexec.exe の隣に msmpi.dll がある構成を想定)。インストーラ版と、
# conda-forge のパッケージを展開しただけの構成の両方を見る。
if (-not $env:MSMPI_BIN) {
    foreach ($c in @("$env:ProgramFiles\Microsoft MPI\Bin",
                     "$env:USERPROFILE\tools\msmpi\Library\bin")) {
        if (Test-Path "$c\mpiexec.exe") { $env:MSMPI_BIN = $c; break }
    }
}
if ($env:MSMPI_BIN -and (Test-Path "$env:MSMPI_BIN\mpiexec.exe")) {
    $env:PATH = "$env:MSMPI_BIN;$env:PATH"
    # FindMPI はこの 2 つを見る。展開しただけの構成では立っていないので補う。
    $mpiRoot = Split-Path -Parent $env:MSMPI_BIN
    if (-not $env:MSMPI_INC   -and (Test-Path "$mpiRoot\include\mpi.h"))     { $env:MSMPI_INC   = "$mpiRoot\include" }
    if (-not $env:MSMPI_LIB64 -and (Test-Path "$mpiRoot\lib\msmpi.lib"))     { $env:MSMPI_LIB64 = "$mpiRoot\lib" }
}
$mpiexec = Get-Command mpiexec -ErrorAction SilentlyContinue
$haveMpi = [bool]$mpiexec

# Qt (GUI 用)
if (-not $env:QT_ROOT) {
    $qt = Get-ChildItem "C:\Qt\*\msvc*_64" -Directory -ErrorAction SilentlyContinue |
          Sort-Object Name -Descending | Select-Object -First 1
    if (-not $qt) {
        $qt = Get-ChildItem "$env:USERPROFILE\Qt\*\msvc*_64" -Directory -ErrorAction SilentlyContinue |
              Sort-Object Name -Descending | Select-Object -First 1
    }
    if ($qt) { $env:QT_ROOT = $qt.FullName }
}
# vcpkg (直列 HDF5 / Eigen3)
if (-not $env:VCPKG_ROOT) {
    foreach ($c in @("$env:USERPROFILE\tools\vcpkg", "C:\vcpkg", "C:\src\vcpkg")) {
        if (Test-Path "$c\vcpkg.exe") { $env:VCPKG_ROOT = $c; break }
    }
}
$vcpkgToolchain = $null
if ($env:VCPKG_ROOT) { $vcpkgToolchain = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" }

Write-Host "  MSVC    : $env:VCToolsVersion"
Write-Host "  CMake   : $((cmake --version | Select-Object -First 1))"
if ($haveCuda) { Write-Host "  CUDA    : $($nvcc.Source)" } else { Write-Host "  CUDA    : 無効 (nvcc が見つかりません)" }
if ($haveMpi)  { Write-Host "  MPI     : $($mpiexec.Source)" } else { Write-Host "  MPI     : 無効 (mpiexec が見つかりません)" }
Write-Host "  Qt      : $(if ($env:QT_ROOT) { $env:QT_ROOT } else { '見つかりません (GUI はビルドできません)' })"
Write-Host "  vcpkg   : $(if ($vcpkgToolchain -and (Test-Path $vcpkgToolchain)) { $env:VCPKG_ROOT } else { '見つかりません' })"
Write-Host "  並列数  : $Jobs"
Write-Host "  親ディレクトリ: $Root"

# 並列 HDF5 (OpenRCWA / OpenBPM の MPI 構成でのみ必要)
$hdf5Parallel = $env:OFDX_HDF5_PARALLEL_DIR
if (-not $hdf5Parallel) {
    $c = "$env:USERPROFILE\tools\hdf5-1.14.6-parallel-msmpi"
    if (Test-Path "$c\lib\cmake\hdf5") { $hdf5Parallel = $c }
}
Write-Host "  並列HDF5: $(if ($hdf5Parallel) { $hdf5Parallel } else { '未設定 ($env:OFDX_HDF5_PARALLEL_DIR)' })"

# 構成の決定
$autoConfigs = ($Configs -eq "auto")
if ($autoConfigs) {
    $list = @("cpu")
    if ($haveCuda) { $list += "cuda" }
    if ($haveMpi)  { $list += "mpi" }
    if ($haveCuda -and $haveMpi) { $list += "cuda-mpi" }
    $Configs = ($list -join ",")
}
$cfgList = $Configs.Split(",") | ForEach-Object { $_.Trim() }
Write-Host "  ビルド構成: $Configs"

# ── git pull ──────────────────────────────────────────────────────────────
function Update-Repo($name) {
    $d = Join-Path $Root $name
    if (-not (Test-Path (Join-Path $d ".git"))) {
        if ($Clone) {
            Write-Host "  clone $name ..."
            git clone -q "$GitBase/$name.git" $d
            if ($LASTEXITCODE -ne 0) { Record "NG" $name "clone 失敗"; return $false }
        } else {
            Warn "$name がありません (-Clone で取得できます)"
            Record "SKIP" $name "リポジトリ無し"
            return $false
        }
    }
    if ($NoPull) { return $true }
    $br = (git -C $d rev-parse --abbrev-ref HEAD 2>$null)
    if (git -C $d status --porcelain --untracked-files=no) {
        Warn "$name [$br] に未コミットの変更があるので pull しません"
        return $true
    }
    git -C $d pull --ff-only -q 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  $name [$br] $(git -C $d log --oneline -1)"
    } else {
        Warn "$name [$br] は fast-forward できませんでした (手で解決してください)"
    }
    return $true
}

# ── ビルド ────────────────────────────────────────────────────────────────
function Build-Kernel($name, $cfg) {
    $d = Join-Path $Root $name
    $bdir = Join-Path $d "build-$cfg"
    $log  = "$bdir.log"
    switch ($cfg) {
        "cpu"      { $opts = @("-DWITH_CUDA=OFF", "-DWITH_MPI=OFF") }
        "cuda"     { $opts = @("-DWITH_CUDA=ON",  "-DWITH_MPI=OFF") }
        "mpi"      { $opts = @("-DWITH_CUDA=OFF", "-DWITH_MPI=ON") }
        "cuda-mpi" { $opts = @("-DWITH_CUDA=ON",  "-DWITH_MPI=ON") }
        default    { Fail "不明な構成: $cfg"; return }
    }
    if ($CudaArch) { $opts += "-DCMAKE_CUDA_ARCHITECTURES=$CudaArch" }

    # 前提が無い構成は作らない。ビルドディレクトリが残っていると古いキャッシュで
    # 通ってしまい「作れた」と誤解するので、明示指定でもここで止める。
    if (($cfg -like "*cuda*") -and -not $haveCuda) {
        Warn "[$name/$cfg] nvcc が無いので飛ばします"
        Record "SKIP" "$name/$cfg" "nvcc 未検出"
        return
    }
    if (($cfg -like "*mpi*") -and -not $haveMpi) {
        Warn "[$name/$cfg] MPI が無いので飛ばします"
        Record "SKIP" "$name/$cfg" "MPI 未検出"
        return
    }

    # OpenRCWA / OpenBPM の MPI 構成は並列 HDF5 が要る (mpi/solve.c が
    # H5Pset_fapl_mpio を使う)。OpenFDTD の MPI 版は rank 0 の直列ドライバで
    # 書くので直列 HDF5 (vcpkg) のままでよい。
    $needParallel = ($cfg -like "*mpi*") -and ($name -ne "OpenFDTD")
    if ($needParallel) {
        if (-not $hdf5Parallel) {
            Warn "[$name/$cfg] 並列 HDF5 が未設定のため飛ばします (OFDX_HDF5_PARALLEL_DIR)"
            Record "SKIP" "$name/$cfg" "並列 HDF5 未設定"
            return
        }
        # vcpkg のツールチェーンは find_package(HDF5) を vcpkg 版に固定するので使わない。
        # HDF5 は HDF5_DIR で、Eigen3 は vcpkg のインストール先から直接指す。
        $opts += "-DHDF5_DIR=$hdf5Parallel\lib\cmake\hdf5"
        $opts += "-DHDF5_USE_STATIC_LIBRARIES=ON"
        $opts += "-DHDF5_PREFER_PARALLEL=ON"
        if ($env:VCPKG_ROOT) {
            $opts += "-DEigen3_DIR=$env:VCPKG_ROOT\installed\x64-windows-static-md\share\eigen3"
        }
    } elseif ($vcpkgToolchain -and (Test-Path $vcpkgToolchain)) {
        $opts += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain"
        $opts += "-DVCPKG_TARGET_TRIPLET=x64-windows-static-md"
    }

    if ($Clean -and (Test-Path $bdir)) { Remove-Item -Recurse -Force $bdir }
    Write-Host "  [$name/$cfg] configure"
    $rc = Invoke-Logged $log $false (@("cmake", "-S", $d, "-B", $bdir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release") + $opts)
    if ($rc -ne 0) { Fail "[$name/$cfg] configure 失敗 — $log"; Record "NG" "$name/$cfg" "configure"; return }
    Write-Host "  [$name/$cfg] build"
    $rc = Invoke-Logged $log $true @("cmake", "--build", $bdir, "-j", "$Jobs")
    if ($rc -ne 0) { Fail "[$name/$cfg] ビルド失敗 — $log"; Record "NG" "$name/$cfg" "build"; return }

    # MPI 版は msmpi.dll が隣に無いと単体起動が 0xC0000135 で落ちる
    if (($cfg -like "*mpi*") -and $env:MSMPI_BIN -and (Test-Path "$env:MSMPI_BIN\msmpi.dll")) {
        $bin = Join-Path $d "bin"
        if ((Test-Path $bin) -and -not (Test-Path "$bin\msmpi.dll")) {
            Copy-Item "$env:MSMPI_BIN\msmpi.dll" $bin
        }
    }
    Record "OK" "$name/$cfg" (Join-Path $d "bin")
}

function Build-Gui {
    $d = Join-Path $Root $dirOf["gui"]
    $bdir = Join-Path $d "build"
    $log  = "$bdir.log"
    if (-not $env:QT_ROOT) { Warn "Qt が無いので GUI は飛ばします"; Record "SKIP" "gui" "Qt 未検出"; return }
    $opts = @("-DCMAKE_PREFIX_PATH=$env:QT_ROOT")
    # HDF5 があれば結果表示 (2D 断面 / H5 アニメ) を有効にする
    if ($vcpkgToolchain -and (Test-Path $vcpkgToolchain)) {
        $opts += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain"
        $opts += "-DVCPKG_TARGET_TRIPLET=x64-windows-static-md"
        $opts += "-DUSE_HDF5=ON"
    }
    # ツールチェーンは初回 configure でしか効かない。既存の build が別設定で
    # 作られていると、-DCMAKE_TOOLCHAIN_FILE を足しても読み込まれないまま
    # USE_HDF5=ON だけ通り「HDF5 が見つからない」で落ちる。キャッシュに値が
    # あるかではなく、**実際に適用されたか** (_VCPKG_INSTALLED_DIR が入るのは
    # ツールチェーンが走ったときだけ) で判定して、違えば作り直す。
    $cache = Join-Path $bdir "CMakeCache.txt"
    $wantVcpkg = [bool]($opts | Where-Object { $_ -like "-DCMAKE_TOOLCHAIN_FILE=*" })
    if ((Test-Path $cache) -and -not $Clean) {
        $applied = [bool](Select-String -Path $cache -Pattern "^_VCPKG_INSTALLED_DIR" -ErrorAction SilentlyContinue)
        if ($wantVcpkg -ne $applied) {
            Warn "[gui] ツールチェーンの設定が変わったのでビルドディレクトリを作り直します"
            Remove-Item -Recurse -Force $bdir
        }
    }
    if ($Clean -and (Test-Path $bdir)) { Remove-Item -Recurse -Force $bdir }
    Write-Host "  [gui] configure"
    $rc = Invoke-Logged $log $false (@("cmake", "-S", $d, "-B", $bdir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release") + $opts)
    if ($rc -ne 0) { Fail "[gui] configure 失敗 — $log"; Record "NG" "gui" "configure"; return }
    Write-Host "  [gui] build"
    $rc = Invoke-Logged $log $true @("cmake", "--build", $bdir, "-j", "$Jobs")
    if ($rc -ne 0) { Fail "[gui] ビルド失敗 — $log"; Record "NG" "gui" "build"; return }

    # Qt のランタイムを実行ファイルの隣へ。これをやらないとビルドに使った
    # シェル以外 (Explorer からのダブルクリック等) で 0xC0000135 になる。
    $wdq = Join-Path $env:QT_ROOT "bin\windeployqt.exe"
    if (Test-Path $wdq) {
        foreach ($exe in @("openfdtd_x.exe", "openuwa.exe")) {
            $p = Join-Path $bdir $exe
            if (Test-Path $p) { [void](Invoke-Logged $log $true @($wdq, "--release", "--no-translations", "--no-system-d3d-compiler", "--no-opengl-sw", $p)) }
        }
        # windeployqt は qwindows.dll しか置かない。ヘッドレス (--screenshot /
        # CI) では offscreen が要るので手で入れる。無いと Qt がプラグイン
        # 不在のダイアログを出したまま止まる。
        $plat = Join-Path $bdir "platforms"
        if (Test-Path $plat) {
            foreach ($p in @("qoffscreen.dll", "qminimal.dll")) {
                $src = Join-Path $env:QT_ROOT "plugins\platforms\$p"
                if ((Test-Path $src) -and -not (Test-Path (Join-Path $plat $p))) { Copy-Item $src $plat }
            }
        }
    }
    Record "OK" "gui" $bdir

    if ($Tests) {
        Write-Host "  [gui] test"
        $env:QT_QPA_PLATFORM = "offscreen"
        $ofdBin = Join-Path $Root "$($dirOf['ofd'])\bin\ofd.exe"
        $bhBin  = Join-Path $Root "$($dirOf['bellhop'])\bin\bellhopcxx.exe"
        if (Test-Path $ofdBin) { $env:OFDX_OFD_BIN = $ofdBin }
        if (Test-Path $bhBin)  { $env:OFDX_BELLHOP_BIN = $bhBin }
        $tlog = "$bdir-test.log"
        Push-Location $d      # selftest は tests/data を相対で探す
        $rc = Invoke-Logged $tlog $false @((Join-Path $bdir "ofdx_selftest.exe"))
        Pop-Location
        if ($rc -eq 0) {
            Record "OK" "gui/selftest" ((Select-String -Path $tlog -Pattern "checks" | Select-Object -Last 1).Line)
        } else { Fail "[gui] selftest 失敗 — $tlog"; Record "NG" "gui/selftest" "" }
        $rc = Invoke-Logged $tlog $true @("ctest", "--test-dir", $bdir, "--output-on-failure")
        if ($rc -eq 0) {
            Record "OK" "gui/ctest" ((Select-String -Path $tlog -Pattern "tests passed" | Select-Object -Last 1).Line)
        } else { Fail "[gui] ctest 失敗 — $tlog"; Record "NG" "gui/ctest" "" }
    }
}

function Build-Bellhop {
    $d = Join-Path $Root $dirOf["bellhop"]
    $bdir = Join-Path $d "build"
    $log  = "$bdir.log"
    # glm サブモジュールが無いと configure で落ちる
    if (-not (Test-Path (Join-Path $d "glm\CMakeLists.txt"))) {
        git -C $d submodule update --init --recursive -q
    }
    $cudaOpt = if ($haveCuda) { "-DBHC_ENABLE_CUDA=ON" } else { "-DBHC_ENABLE_CUDA=OFF" }
    if ($Clean -and (Test-Path $bdir)) { Remove-Item -Recurse -Force $bdir }
    Write-Host "  [bellhop] configure"
    $rc = Invoke-Logged $log $false @("cmake", "-S", $d, "-B", $bdir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", $cudaOpt, "-DBHC_BUILD_EXAMPLES=OFF")
    if ($rc -ne 0) { Fail "[bellhop] configure 失敗 — $log"; Record "NG" "bellhop" "configure"; return }
    Write-Host "  [bellhop] build (CUDA 版はテンプレート実体化が多く時間がかかります)"
    $targets = @("--target", "bellhopcxx")
    if ($haveCuda) { $targets += "bellhopcuda" }
    $rc = Invoke-Logged $log $true (@("cmake", "--build", $bdir, "-j", "$Jobs") + $targets)
    if ($rc -ne 0) { Fail "[bellhop] ビルド失敗 — $log"; Record "NG" "bellhop" "build"; return }
    Record "OK" "bellhop" (Join-Path $d "bin")
}

function Build-Acoustics {
    $d = Join-Path $Root $dirOf["acoustics"]
    $bdir = Join-Path $d "build"
    $log  = "$bdir.log"
    if ($Clean -and (Test-Path $bdir)) { Remove-Item -Recurse -Force $bdir }
    Write-Host "  [acoustics] configure / build (OpenMP のみ。CUDA / MPI 版はありません)"
    $rc = Invoke-Logged $log $false @("cmake", "-S", $d, "-B", $bdir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release")
    if ($rc -eq 0) { $rc = Invoke-Logged $log $true @("cmake", "--build", $bdir, "-j", "$Jobs") }
    if ($rc -ne 0) { Fail "[acoustics] 失敗 — $log"; Record "NG" "acoustics" "build"; return }
    Record "OK" "acoustics" $bdir
}

# ── 実行 ──────────────────────────────────────────────────────────────────
Head "リポジトリの更新"
if ($NoPull) { Write-Host "  (-NoPull のため省略)" }
foreach ($k in @("gui", "ofd", "orcwa", "obpm", "bellhop", "acoustics")) {
    if ($want -notcontains $k) { continue }
    [void](Update-Repo $dirOf[$k])
}

Head "ビルド"
foreach ($k in @("ofd", "orcwa", "obpm")) {
    if ($want -notcontains $k) { continue }
    $d = Join-Path $Root $dirOf[$k]
    if (-not (Test-Path (Join-Path $d ".git"))) { continue }
    foreach ($cfg in $cfgList) { Build-Kernel $dirOf[$k] $cfg }
}
if (($want -contains "bellhop") -and (Test-Path (Join-Path $Root "$($dirOf['bellhop'])\.git"))) { Build-Bellhop }
if (($want -contains "acoustics") -and (Test-Path (Join-Path $Root "$($dirOf['acoustics'])\.git"))) { Build-Acoustics }
# GUI は最後 (カーネルが揃った状態で統合テストを走らせられる)
if ($want -contains "gui") { Build-Gui }

# ── 結果 ──────────────────────────────────────────────────────────────────
Head "結果"
foreach ($r in $results) {
    $color = switch ($r.状態) { "OK" { "Green" } "SKIP" { "Yellow" } default { "Red" } }
    Write-Host ("  {0,-4} {1,-24} {2}" -f $r.状態, $r.対象, $r.備考) -ForegroundColor $color
}
if ($autoConfigs -and -not $haveCuda) { Warn "CUDA 版は作っていません (nvcc が見つかりません。CUDA_PATH を設定すると使えます)" }
if ($autoConfigs -and -not $haveMpi)  { Warn "MPI 版は作っていません (mpiexec が見つかりません。MSMPI_BIN を設定すると使えます)" }

Write-Host ""
Write-Host "GUI からカーネルを使うには、ツール > カーネルパスの設定… で各リポジトリを"
Write-Host "指定するか、環境変数を設定してください (Explorer 起動には環境変数が届かないので"
Write-Host "設定ダイアログの方が確実です):"
Write-Host "  `$env:OPENFDTD_HOME  = `"$Root\$($dirOf['ofd'])`""
Write-Host "  `$env:OPENRCWA_HOME  = `"$Root\$($dirOf['orcwa'])`""
Write-Host "  `$env:OPENBPM_HOME   = `"$Root\$($dirOf['obpm'])`""
Write-Host "  `$env:BELLHOPCUDA_HOME = `"$Root\$($dirOf['bellhop'])`""
Write-Host "検出結果は次で確認できます: $Root\$($dirOf['gui'])\build\openfdtd_x.exe --check-kernels"

if ($results | Where-Object { $_.状態 -eq "NG" }) { exit 1 }
exit 0
