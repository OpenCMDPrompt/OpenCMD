@echo off
set "CLANG_FORMAT=clang-format"

for /r %%f in (*.cpp *.c *.h *.hpp) do (
    echo Formatting %%f
    "%CLANG_FORMAT%" -i "%%f"
)
