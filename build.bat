@echo off

set BaseFile1="task1.cpp"
set MsvcLinkFlags=-incremental:no -opt:ref -machine:x64 -manifest:no
set MsvcCompileFlags=-Zi -Zo -Gy -GF -GR- -EHs- -EHc- -EHa- -WX -W4 -nologo -FC -diagnostics:column -fp:except- -fp:fast -wd4100 -wd4189 -wd4201 -wd4505 -wd4996 -arch:AVX

set ClangCompileFlags=-Wno-missing-braces -Wno-writable-strings -Wno-unused-function -mavx


echo -----------------
echo ---- Building debug (task 1):
call cl -Fetask1_debug_msvc.exe -Od %MsvcCompileFlags% %BaseFile1% /link %MsvcLinkFlags% -RELEASE
call clang-cl -Fetask1_debug_clang.exe -Od %MsvcCompileFlags% %ClangCompileFlags% %BaseFile1% /link %MsvcLinkFlags% -RELEASE

echo -----------------
echo ---- Building release (task 1):
call cl -Fetask1_release_msvc.exe -Oi -Oxb2 -O2 %CLCompileFlags% %BaseFile1% /link %CLLinkFlags% -RELEASE
call clang-cl -Fetask1_release_clang.exe -Oi -Oxb2 -O2 %MscvCompileFlags% %ClangCompileFlags% %BaseFile1% /link %MsvcLinkFlags% -RELEASE
