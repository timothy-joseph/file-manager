## features:
1. yank files
2. move files
3. rename a single file
4. bulk rename
5. search

## install:
download one of the release or the latest master, cd into that directory and run:
`sudo make install`

### How to actually make this somewhat useful:
add the following to your .bashrc, .zsh etc
```sh
alias fm='stuifm; LASTDIR=`cat $HOME/.vcd`; cd "$LASTDIR"'
```
