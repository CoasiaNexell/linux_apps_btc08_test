#!/bin/bash

args=()

format=preserve
delimiter="\n"
nonewline=false
join=false
align4=false
strip=false

while (( "$#" )); do
    case "$1" in
        -h|--help) usage;;
        -f) format=$2; shift 2;;
        --format=*) format="${1#*=}"; shift;;
        -d) delimiter=$2; shift 2;;
        --delimiter=*) delimiter="${1#*=}"; shift;;
        -n|--no-newline) nonewline=true; shift;;
        -4) align4=true; shift;;
        -j|--join) join=true; shift;;
        -s|--strip-null) strip=true; shift;;
        -*|--*) echo "Error: unsupported flag $1 specified"; exit 1;;
        *) args=( "${args[@]}" "$1" ); shift;;
    esac
done

case "$format" in
    preserve);;
    int) prefix="0x";;
    bb) prefix=" ";; 
    char) prefix="\x";; 
    raw) ;;
    *) echo "Error: unsupported format $format"; exit 1;;
esac

n=0
parts=()
for arg in ${args[@]}; do

    digest=""
    prefix=""

    # remove prefix if string begins with "0x"
    if [[ $arg =~ ^[0\\]x ]]; then
        if [ "$format" == "preserve" ]; then
            prefix=${arg:0:2}
        fi
        arg=${arg:2}
    fi

    # zero-pad if string has odd length
    if [ $[${#arg} % 2] != 0 ]; then
        arg="0$arg"
    fi

    part=""
    i=${#arg}
    while [ $i -gt 0 ]; do
        i=$[$i-2]
		ii=$i
		if [ $align4 == true ]; then
			ii=$((i^6))
		fi
        byte=${arg:$ii:2}
        if [ $strip == true ] && [ -z "$part" ] && [ $byte == "00" ]; then
            continue
        fi
        case "$format" in
            int) part="$part"'0x'"$byte ";;
            bb) part="$part$byte ";;
            char) part="$part\x$byte";;
            raw) part="$part$(printf "%b" "\x$byte")";;
            *) part="$part$byte";;
        esac
    done

    digest="$prefix$digest$part"

    parts=( "${parts[@]}" "$digest" )
    n=$[$n+1]

done

if [ $join == true ]; then
    case "$format" in
        *) printf "%s" "${parts[@]}";;
    esac
else
    i=0
    for part in "${parts[@]}"; do
        if [[ $(($i + 1)) < $n ]]; then
            printf "%s$delimiter" "$part"
        else
            printf "%s" "$part"
        fi
        i=$(($i+1))
    done
fi

if [ $nonewline == false ]; then
    echo
fi
