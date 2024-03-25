" syntax file for vim
:syn keyword Type i8 i32 i64 void string
:syn keyword Repeat while for do
:syn keyword Conditional if else
:syn keyword Function main
:syn match stdLib "std\.\a*"
:syn match Comment "#.*$"
:syntax region String start=+"+ end=+"+ skip=+\\"+
hi stdLib term=bold ctermfg=green
