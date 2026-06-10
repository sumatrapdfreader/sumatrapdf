#!/usr/bin/env bash

DAV1D="tools/dav1d"
ARGON_DIR='.'
FILMGRAIN=1
CPUMASK=-1
THREADS=1
JOBS=0
WRAP=""
FAIL_FAST=0


usage() {
    NAME=$(basename "$0")
    {
        printf "Usage:   %s [-d dav1d] [-a argondir] [-g \$filmgrain] [-c \$cpumask] [-t threads] [-j jobs] [DIRECTORY]...\n" "$NAME"
        printf "Example: %s -d /path/to/dav1d -a /path/to/argon/ -g 0 -c avx2 profile0_core\n" "$NAME"
        printf "Used to verify that dav1d can decode the Argon AV1 test vectors correctly.\n\n"
        printf " DIRECTORY one or more dirs in the argon folder to check against\n"
        printf "             (default: everything except large scale tiles and stress files)\n"
        printf " -f        fail fast\n"
        printf " -d dav1d  path to dav1d executable (default: tools/dav1d)\n"
        printf " -a dir    path to argon dir (default: 'tests/argon' if found; '.' otherwise)\n"
        printf " -g \$num   enable filmgrain (default: 1)\n"
        printf " -c \$mask  use restricted cpumask (default: -1)\n"
        printf " -t \$num   number of threads per dav1d (default: 1)\n"
        printf " -j \$num   number of parallel dav1d processes (default: 0)\n"
        printf " -w tool   execute dav1d with a wrapper tool\n\n"
    } >&2
    exit 1
}

error() {
    printf "\033[1;91m%s\033[0m\n" "$*" >&2
    exit 1
}

fail() {
    printf "\033[1K\rMismatch in %s\n" "$1"
    [[ $FAIL_FAST = 1 ]] && exit 1
    (( failed++ ))
}

check_pids() {
    new_pids=()
    done_pids=()
    for p in "${pids[@]}"; do
        if kill -0 "$p" 2>/dev/null; then
            new_pids+=("$p")
        else
            done_pids+=("$p")
        fi
    done
    pids=("${new_pids[@]}")
}

wait_pids() {
    pid_list=("$@")
    for p in "${pid_list[@]}"; do
        if ! wait "$p"; then
            local file_varname="file$p"
            fail "${!file_varname}"
        fi
    done
}

block_pids() {
    while [ ${#pids[@]} -ge "$JOBS" ]; do
        check_pids
        if [ ${#done_pids} -eq 0 ]; then
            sleep 0.2
        else
            wait_pids "${done_pids[@]}"
        fi
    done
}

wait_all_pids() {
    wait_pids "${pids[@]}"
}

# find tests/argon
tests_dir=$(dirname "$(readlink -f "$0")")
if [ -d "$tests_dir/argon" ]; then
    ARGON_DIR="$tests_dir/argon"
fi

while getopts ":d:a:g:c:t:j:w:f" opt; do
    case "$opt" in
        f)
            FAIL_FAST=1
            ;;
        d)
            DAV1D="$OPTARG"
            ;;
        a)
            ARGON_DIR="$OPTARG"
            ;;
        g)
            FILMGRAIN="$OPTARG"
            ;;
        c)
            CPUMASK="$OPTARG"
            ;;
        t)
            THREADS="$OPTARG"
            ;;
        j)
            JOBS="$OPTARG"
            ;;
        w)
            WRAP="$OPTARG"
            ;;
        \?)
            printf "Error! Invalid option: -%s\n" "$OPTARG" >&2
            usage
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

if [ "$JOBS" -eq 0 ]; then
    if [ "$THREADS" -gt 0 ]; then
        JOBS="$((($( (nproc || sysctl -n hw.logicalcpu || getconf _NPROCESSORS_ONLN || echo 1) 2>/dev/null)+THREADS-1)/THREADS))"
    else
        JOBS=1
    fi
fi

if [ "$#" -eq 0 ]; then
    # Everything except large scale tiles and stress files.
    dirs=("$ARGON_DIR/profile0_core"       "$ARGON_DIR/profile0_core_special"
          "$ARGON_DIR/profile0_not_annexb" "$ARGON_DIR/profile0_not_annexb_special"
          "$ARGON_DIR/profile1_core"       "$ARGON_DIR/profile1_core_special"
          "$ARGON_DIR/profile1_not_annexb" "$ARGON_DIR/profile1_not_annexb_special"
          "$ARGON_DIR/profile2_core"       "$ARGON_DIR/profile2_core_special"
          "$ARGON_DIR/profile2_not_annexb" "$ARGON_DIR/profile2_not_annexb_special"
          "$ARGON_DIR/profile_switching")
else
    mapfile -t dirs < <(printf "${ARGON_DIR}/%s\n" "$@" | sort -u)
fi

ver_info="dav1d $("$DAV1D" --filmgrain "$FILMGRAIN" --cpumask "$CPUMASK" --threads "$THREADS" -v 2>&1) filmgrain=$FILMGRAIN cpumask=$CPUMASK" || error "Error! Can't run $DAV1D"
files=()

for d in "${dirs[@]}"; do
    if [ -d "$d/streams" ]; then
        files+=("${d/%\//}"/streams/*.obu)
    fi
done

num_files="${#files[@]}"
if [ "$num_files" -eq 0 ]; then
    error "Error! No files found at ${dirs[*]}"
fi

failed=0
pids=()
for i in "${!files[@]}"; do
    f="${files[i]}"
    if [ "$FILMGRAIN" -eq 0 ]; then
        md5=${f/\/streams\//\/md5_no_film_grain\/}
    else
        md5=${f/\/streams\//\/md5_ref\/}
    fi
    md5=$(<"${md5/%obu/md5}") || error "Error! Can't read md5 ${md5} for file ${f}"
    md5=${md5/ */}

    printf '\033[1K\r[%3d%% %*d/%d] Verifying %s' "$(((i+1)*100/num_files))" "${#num_files}" "$((i+1))" "$num_files" "${f#"$ARGON_DIR"/}"
    cmd=($WRAP "$DAV1D" -i "$f" --filmgrain "$FILMGRAIN" --verify "$md5" --cpumask "$CPUMASK" --threads "$THREADS" -q)
    if [ "$JOBS" -gt 1 ]; then
        "${cmd[@]}" 2>/dev/null &
        p=$!
        pids+=("$p")
        declare "file$p=${f#"$ARGON_DIR"/}"
        block_pids
    else
        if ! "${cmd[@]}" 2>/dev/null; then
            fail "${f#"$ARGON_DIR"/}"
        fi
    fi
done

wait_all_pids

if [ "$failed" -ne 0 ]; then
    printf "\033[1K\r%d/%d files \033[1;91mfailed\033[0m to verify" "$failed" "$num_files"
else
    printf "\033[1K\r%d files \033[1;92msuccessfully\033[0m verified" "$num_files"
fi
printf " in %dm%ds (%s)\n" "$((SECONDS/60))" "$((SECONDS%60))" "$ver_info"

exit $failed
