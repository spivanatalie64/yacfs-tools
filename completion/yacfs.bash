# YAcFS bash completion
_yacfs() {
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    opts="-o -f -d --help"

    case "${prev}" in
        yacfs)
            COMPREPLY=($(compgen -W "${opts}" -- ${cur}))
            ;;
        -o)
            COMPREPLY=($(compgen -W "pool= compress= snapdir=" -- ${cur}))
            ;;
        pool=)
            COMPREPLY=($(compgen -d -- ${cur}))
            ;;
        compress=)
            COMPREPLY=($(compgen -W "0 1 2 3 5 10 19" -- ${cur}))
            ;;
        *)
            COMPREPLY=($(compgen -d -- ${cur}))
            ;;
    esac
}
complete -F _yacfs yacfs

_yacfs_ctl() {
    local cur prev commands
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    commands="status snapshots snapshot rollback info"

    if [ ${COMP_CWORD} -eq 1 ]; then
        COMPREPLY=($(compgen -d -- ${cur}))
    elif [ ${COMP_CWORD} -eq 2 ]; then
        COMPREPLY=($(compgen -W "${commands}" -- ${cur}))
    fi
}
complete -F _yacfs_ctl yacfs-ctl
