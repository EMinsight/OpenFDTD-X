#!/usr/bin/env bash
# update-and-build.sh — 関連リポジトリを git pull して一括ビルドする (macOS / Linux)
#
# OpenFDTD-X (GUI) と、GUI が subprocess で起動するソルバーカーネル群は別々の
# リポジトリにある。全部を手で pull してビルドし直すのは手間なので、まとめる。
#
#   GUI      OpenFDTD-X       openfdtd_x / openuwa
#   FDTD     OpenFDTD         ofd / ofd_mpi / ofd_cuda / ofd_cuda_mpi / ofd_post
#   RCWA     OpenRCWA         orcwa / orcwa_mpi / orcwa_cuda / orcwa_cuda_mpi / orcwa_post
#   BPM      OpenBPM          obpm  / obpm_mpi  / obpm_cuda  / obpm_cuda_mpi  / obpm_post
#   水中音響  bellhopcuda      bellhopcxx / bellhopcuda
#   室内音響  OpenAcoustics    ofdx_acoustic_fdtd ほか (OpenMP のみ)
#
# ビルドする構成は**実行環境から自動判定**する (nvcc があれば CUDA、mpiexec が
# あれば MPI)。無いものは黙って飛ばさず、最後の一覧に理由を出す。
#
# Windows は同じディレクトリの update-and-build.ps1 を使う。
#
# 使い方:
#   tools/update-and-build.sh                    # 既定 (pull + 自動判定でビルド)
#   tools/update-and-build.sh --repos gui,ofd    # 対象を絞る
#   tools/update-and-build.sh --configs cpu      # CPU 版だけ
#   tools/update-and-build.sh --no-pull --tests  # pull せずビルドしてテストまで
#   tools/update-and-build.sh --clone            # 無いリポジトリは clone してくる
set -u

# ── 既定値 ────────────────────────────────────────────────────────────────
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
GUI_DIR=$(dirname "$SCRIPT_DIR")          # tools/ の 1 つ上 = OpenFDTD-X
ROOT=$(dirname "$GUI_DIR")                # その 1 つ上 = リポジトリを並べる親
REPOS="gui,ofd,orcwa,obpm,bellhop,acoustics"
CONFIGS="auto"
DO_PULL=1
DO_CLONE=0
DO_CLEAN=0
DO_TESTS=0
JOBS=""
GIT_BASE="https://github.com/Sirokujira"

usage() {
    sed -n '2,26p' "$0" | sed 's/^# \{0,1\}//'
    cat <<'EOS'

オプション:
  --root DIR        リポジトリを並べる親ディレクトリ (既定: OpenFDTD-X の 1 つ上)
  --repos LIST      対象: gui,ofd,orcwa,obpm,bellhop,acoustics (既定: 全部)
  --configs LIST    カーネルの構成: auto | cpu,cuda,mpi,cuda-mpi (既定: auto)
  --no-pull         git pull を行わない (ビルドのみ)
  --clone           無いリポジトリを clone する
  --clean           ビルドディレクトリを消してから構成する
  --tests           ビルド後にテストを実行する
  --jobs N          並列ビルド数 (既定: CPU 数)
  -h, --help        この使い方
EOS
}

while [ $# -gt 0 ]; do
    case "$1" in
        --root)    ROOT=$2; shift 2 ;;
        --repos)   REPOS=$2; shift 2 ;;
        --configs) CONFIGS=$2; shift 2 ;;
        --jobs)    JOBS=$2; shift 2 ;;
        --no-pull) DO_PULL=0; shift ;;
        --clone)   DO_CLONE=1; shift ;;
        --clean)   DO_CLEAN=1; shift ;;
        --tests)   DO_TESTS=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "不明なオプション: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [ -z "$JOBS" ]; then
    JOBS=$( (command -v nproc >/dev/null && nproc) \
         || (command -v sysctl >/dev/null && sysctl -n hw.ncpu) || echo 4 )
fi

# ── 表示 ──────────────────────────────────────────────────────────────────
if [ -t 1 ]; then B=$(printf '\033[1m'); R=$(printf '\033[0m')
                  G=$(printf '\033[32m'); Y=$(printf '\033[33m'); E=$(printf '\033[31m')
else B=; R=; G=; Y=; E=; fi
say()  { printf '%s\n' "$*"; }
head2() { printf '\n%s== %s%s\n' "$B" "$*" "$R"; }
warn() { printf '%s! %s%s\n' "$Y" "$*" "$R"; }
err()  { printf '%s x %s%s\n' "$E" "$*" "$R"; }

RESULTS=""                                 # "状態|対象|備考" を 1 行ずつ貯める
record() { RESULTS="${RESULTS}$1|$2|$3
"; }

# ── リポジトリ定義 (キー:ディレクトリ名) ──────────────────────────────────
dir_of() {
    case "$1" in
        gui)       echo "OpenFDTD-X" ;;
        ofd)       echo "OpenFDTD" ;;
        orcwa)     echo "OpenRCWA" ;;
        obpm)      echo "OpenBPM" ;;
        bellhop)   echo "bellhopcuda" ;;
        acoustics) echo "OpenAcoustics" ;;
        *)         echo "" ;;
    esac
}
has_repo() { case ",$REPOS," in *",$1,"*) return 0 ;; *) return 1 ;; esac; }

# ── 環境の検出 ────────────────────────────────────────────────────────────
head2 "環境の検出"
UNAME=$(uname -s)
HAVE_CUDA=0; HAVE_MPI=0
CUDA_REASON="nvcc が PATH にありません"
MPI_REASON="mpiexec / mpirun が PATH にありません"
command -v nvcc >/dev/null 2>&1 && { HAVE_CUDA=1; CUDA_REASON=$(nvcc --version | tail -1); }
if command -v mpiexec >/dev/null 2>&1 || command -v mpirun >/dev/null 2>&1; then
    HAVE_MPI=1; MPI_REASON=$( (command -v mpiexec || command -v mpirun) 2>/dev/null )
fi
say "  OS      : $UNAME"
say "  CMake   : $( (cmake --version 2>/dev/null | head -1) || echo '見つかりません')"
say "  CUDA    : $( [ $HAVE_CUDA = 1 ] && echo "$CUDA_REASON" || echo "無効 ($CUDA_REASON)")"
say "  MPI     : $( [ $HAVE_MPI = 1 ] && echo "$MPI_REASON" || echo "無効 ($MPI_REASON)")"
say "  並列数  : $JOBS"
say "  親ディレクトリ: $ROOT"

command -v cmake >/dev/null 2>&1 || { err "cmake が見つかりません"; exit 1; }

# Ninja があれば使う (Makefiles より速い)。無ければ CMake の既定に任せる。
GEN=""
command -v ninja >/dev/null 2>&1 && GEN="-G Ninja"
say "  生成器  : $( [ -n "$GEN" ] && echo Ninja || echo '既定 (Unix Makefiles)')"

# macOS は Homebrew の Qt / HDF5 / libomp を CMake に教える
PREFIX_PATHS=""
if [ "$UNAME" = "Darwin" ] && command -v brew >/dev/null 2>&1; then
    for p in qt hdf5 libomp eigen lapack; do
        pfx=$(brew --prefix "$p" 2>/dev/null) || continue
        [ -d "$pfx" ] && PREFIX_PATHS="${PREFIX_PATHS:+$PREFIX_PATHS;}$pfx"
    done
    [ -n "$PREFIX_PATHS" ] && say "  brew    : $PREFIX_PATHS"
fi

# 構成の決定
AUTO_CONFIGS=0
if [ "$CONFIGS" = "auto" ]; then
    AUTO_CONFIGS=1
    CONFIGS="cpu"
    [ $HAVE_CUDA = 1 ] && CONFIGS="$CONFIGS,cuda"
    [ $HAVE_MPI  = 1 ] && CONFIGS="$CONFIGS,mpi"
    [ $HAVE_CUDA = 1 ] && [ $HAVE_MPI = 1 ] && CONFIGS="$CONFIGS,cuda-mpi"
fi
say "  ビルド構成: $CONFIGS"

# ── git pull ──────────────────────────────────────────────────────────────
update_repo() {   # $1 = ディレクトリ名
    d="$ROOT/$1"
    if [ ! -d "$d/.git" ]; then
        if [ $DO_CLONE = 1 ]; then
            say "  clone $1 ..."
            git clone -q "$GIT_BASE/$1.git" "$d" || { record "NG" "$1" "clone 失敗"; return 1; }
        else
            warn "$1 がありません (--clone で取得できます)"
            record "SKIP" "$1" "リポジトリ無し"
            return 1
        fi
    fi
    [ $DO_PULL = 0 ] && return 0
    br=$(git -C "$d" rev-parse --abbrev-ref HEAD 2>/dev/null)
    if [ -n "$(git -C "$d" status --porcelain --untracked-files=no)" ]; then
        warn "$1 [$br] に未コミットの変更があるので pull しません"
        return 0
    fi
    if git -C "$d" pull --ff-only -q 2>/dev/null; then
        say "  $1 [$br] $(git -C "$d" log --oneline -1)"
    else
        warn "$1 [$br] は fast-forward できませんでした (手で解決してください)"
    fi
}

# ── ビルド ────────────────────────────────────────────────────────────────
# カーネル 1 構成をビルドする。$1 = ディレクトリ名, $2 = 構成
build_kernel() {
    d="$ROOT/$1"; cfg="$2"; bdir="$d/build-$cfg"
    # 前提が無い構成は作らない。ビルドディレクトリが残っていると古いキャッシュで
    # 通ってしまい「作れた」と誤解するので、明示指定でもここで止める。
    case "$cfg" in
        *cuda*) if [ $HAVE_CUDA = 0 ]; then
                    warn "[$1/$cfg] nvcc が無いので飛ばします"
                    record "SKIP" "$1/$cfg" "nvcc 未検出"; return 0
                fi ;;
    esac
    case "$cfg" in
        *mpi*) if [ $HAVE_MPI = 0 ]; then
                   warn "[$1/$cfg] MPI が無いので飛ばします"
                   record "SKIP" "$1/$cfg" "MPI 未検出"; return 0
               fi ;;
    esac
    case "$cfg" in
        cpu)      opts="-DWITH_CUDA=OFF -DWITH_MPI=OFF" ;;
        cuda)     opts="-DWITH_CUDA=ON  -DWITH_MPI=OFF" ;;
        mpi)      opts="-DWITH_CUDA=OFF -DWITH_MPI=ON" ;;
        cuda-mpi) opts="-DWITH_CUDA=ON  -DWITH_MPI=ON" ;;
        *) err "不明な構成: $cfg"; return 1 ;;
    esac
    # MPI 構成は並列 HDF5 を優先する (OpenRCWA / OpenBPM の mpi/solve.c が
    # H5Pset_fapl_mpio を使う。Linux は libhdf5-openmpi-dev、macOS は
    # brew install hdf5-mpi で入る)
    case "$cfg" in *mpi*) opts="$opts -DHDF5_PREFER_PARALLEL=ON" ;; esac
    [ -n "$PREFIX_PATHS" ] && opts="$opts -DCMAKE_PREFIX_PATH=$PREFIX_PATHS"
    [ -n "${CUDA_ARCH:-}" ] && opts="$opts -DCMAKE_CUDA_ARCHITECTURES=$CUDA_ARCH"
    [ $DO_CLEAN = 1 ] && rm -rf "$bdir"
    say "  [$1/$cfg] configure"
    if ! cmake -S "$d" -B "$bdir" $GEN -DCMAKE_BUILD_TYPE=Release $opts >"$bdir.log" 2>&1; then
        err "[$1/$cfg] configure 失敗 — $bdir.log"
        record "NG" "$1/$cfg" "configure"
        return 1
    fi
    say "  [$1/$cfg] build"
    if ! cmake --build "$bdir" -j "$JOBS" >>"$bdir.log" 2>&1; then
        err "[$1/$cfg] ビルド失敗 — $bdir.log"
        record "NG" "$1/$cfg" "build"
        return 1
    fi
    record "OK" "$1/$cfg" "$d/bin"
}

build_gui() {
    d="$ROOT/$(dir_of gui)"; bdir="$d/build"
    opts=""
    [ -n "$PREFIX_PATHS" ] && opts="-DCMAKE_PREFIX_PATH=$PREFIX_PATHS"
    # HDF5 があれば結果表示 (2D 断面 / H5 アニメ) を有効にする
    if [ "$UNAME" = "Darwin" ] || [ -f /usr/include/hdf5.h ] \
       || [ -f /usr/include/hdf5/serial/hdf5.h ]; then
        opts="$opts -DUSE_HDF5=ON"
    fi
    [ $DO_CLEAN = 1 ] && rm -rf "$bdir"
    say "  [gui] configure"
    if ! cmake -S "$d" -B "$bdir" $GEN -DCMAKE_BUILD_TYPE=Release $opts >"$bdir.log" 2>&1; then
        err "[gui] configure 失敗 — $bdir.log"; record "NG" "gui" "configure"; return 1
    fi
    say "  [gui] build"
    if ! cmake --build "$bdir" -j "$JOBS" >>"$bdir.log" 2>&1; then
        err "[gui] ビルド失敗 — $bdir.log"; record "NG" "gui" "build"; return 1
    fi
    record "OK" "gui" "$bdir"
    if [ $DO_TESTS = 1 ]; then
        say "  [gui] test"
        # ヘッドレス環境でも動くように offscreen を指定する。実カーネルが
        # 揃っていれば統合テストも走らせる (無ければテスト側が skip する)。
        ( cd "$d" && env QT_QPA_PLATFORM=offscreen \
            OFDX_OFD_BIN="$ROOT/$(dir_of ofd)/bin/ofd" \
            OFDX_BELLHOP_BIN="$ROOT/$(dir_of bellhop)/bin/bellhopcxx" \
            "$bdir/ofdx_selftest" ) >"$bdir-test.log" 2>&1 \
            && record "OK" "gui/selftest" "$(tail -2 "$bdir-test.log" | head -1)" \
            || { err "[gui] selftest 失敗 — $bdir-test.log"; record "NG" "gui/selftest" ""; }
        env QT_QPA_PLATFORM=offscreen ctest --test-dir "$bdir" --output-on-failure \
            >>"$bdir-test.log" 2>&1 \
            && record "OK" "gui/ctest" "$(grep -o '[0-9]* tests passed[^)]*' "$bdir-test.log" | tail -1)" \
            || { err "[gui] ctest 失敗 — $bdir-test.log"; record "NG" "gui/ctest" ""; }
    fi
}

build_bellhop() {
    d="$ROOT/$(dir_of bellhop)"; bdir="$d/build"
    # glm サブモジュールが無いと configure で落ちる
    [ -f "$d/glm/CMakeLists.txt" ] || git -C "$d" submodule update --init --recursive -q
    cuda_opt="-DBHC_ENABLE_CUDA=OFF"
    [ $HAVE_CUDA = 1 ] && cuda_opt="-DBHC_ENABLE_CUDA=ON"
    [ $DO_CLEAN = 1 ] && rm -rf "$bdir"
    say "  [bellhop] configure"
    if ! cmake -S "$d" -B "$bdir" $GEN -DCMAKE_BUILD_TYPE=Release $cuda_opt \
              -DBHC_BUILD_EXAMPLES=OFF >"$bdir.log" 2>&1; then
        err "[bellhop] configure 失敗 — $bdir.log"; record "NG" "bellhop" "configure"; return 1
    fi
    say "  [bellhop] build (CUDA 版はテンプレート実体化が多く時間がかかります)"
    tgt="--target bellhopcxx"
    [ $HAVE_CUDA = 1 ] && tgt="$tgt bellhopcuda"
    if ! cmake --build "$bdir" -j "$JOBS" $tgt >>"$bdir.log" 2>&1; then
        err "[bellhop] ビルド失敗 — $bdir.log"; record "NG" "bellhop" "build"; return 1
    fi
    record "OK" "bellhop" "$d/bin"
}

build_acoustics() {
    d="$ROOT/$(dir_of acoustics)"; bdir="$d/build"
    [ $DO_CLEAN = 1 ] && rm -rf "$bdir"
    say "  [acoustics] configure / build (OpenMP のみ。CUDA / MPI 版はありません)"
    if ! cmake -S "$d" -B "$bdir" $GEN -DCMAKE_BUILD_TYPE=Release >"$bdir.log" 2>&1 \
       || ! cmake --build "$bdir" -j "$JOBS" >>"$bdir.log" 2>&1; then
        err "[acoustics] 失敗 — $bdir.log"; record "NG" "acoustics" "build"; return 1
    fi
    record "OK" "acoustics" "$bdir"
}

# ── 実行 ──────────────────────────────────────────────────────────────────
head2 "リポジトリの更新"
[ $DO_PULL = 0 ] && say "  (--no-pull のため省略)"
for k in gui ofd orcwa obpm bellhop acoustics; do
    has_repo "$k" || continue
    update_repo "$(dir_of "$k")" || true
done

head2 "ビルド"
for k in ofd orcwa obpm; do
    has_repo "$k" || continue
    d="$ROOT/$(dir_of "$k")"; [ -d "$d/.git" ] || continue
    old_ifs=$IFS; IFS=,
    for cfg in $CONFIGS; do IFS=$old_ifs; build_kernel "$(dir_of "$k")" "$cfg" || true; IFS=,; done
    IFS=$old_ifs
done
if has_repo bellhop && [ -d "$ROOT/$(dir_of bellhop)/.git" ]; then build_bellhop || true; fi
if has_repo acoustics && [ -d "$ROOT/$(dir_of acoustics)/.git" ]; then build_acoustics || true; fi
# GUI は最後 (カーネルが揃った状態で統合テストを走らせられる)
if has_repo gui; then build_gui || true; fi

# ── 結果 ──────────────────────────────────────────────────────────────────
head2 "結果"
printf '%s\n' "$RESULTS" | while IFS='|' read -r st tgt note; do
    [ -z "$st" ] && continue
    case "$st" in
        OK)   printf '  %s%-4s%s %-24s %s\n' "$G" "$st" "$R" "$tgt" "$note" ;;
        SKIP) printf '  %s%-4s%s %-24s %s\n' "$Y" "$st" "$R" "$tgt" "$note" ;;
        *)    printf '  %s%-4s%s %-24s %s\n' "$E" "$st" "$R" "$tgt" "$note" ;;
    esac
done
[ $AUTO_CONFIGS = 1 ] && [ $HAVE_CUDA = 0 ] && warn "CUDA 版は作っていません ($CUDA_REASON)"
[ $AUTO_CONFIGS = 1 ] && [ $HAVE_MPI  = 0 ] && warn "MPI 版は作っていません ($MPI_REASON)"

say ""
say "GUI からカーネルを使うには、ツール > カーネルパスの設定… で各リポジトリを"
say "指定するか、環境変数を設定してください:"
say "  export OPENFDTD_HOME=$ROOT/$(dir_of ofd)"
say "  export OPENRCWA_HOME=$ROOT/$(dir_of orcwa)"
say "  export OPENBPM_HOME=$ROOT/$(dir_of obpm)"
say "  export BELLHOPCUDA_HOME=$ROOT/$(dir_of bellhop)"
say "検出結果は次で確認できます: $ROOT/$(dir_of gui)/build/openfdtd_x --check-kernels"

printf '%s\n' "$RESULTS" | grep -q '^NG|' && exit 1
exit 0
