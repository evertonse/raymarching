#!/bin/env sh
set -xe
pwsh=$(wslpath 'C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe')
# $bin -noe -c "&{Import-Module """C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"""; Enter-VsDevShell dffc9763}"
exe_name='main.exe'
exe_dir=$(wslpath -w "$(pwd)")
exe_path="$exe_dir/$exe_name"

cmds=

cmds_append() {
  if [ -z "$cmds" ]; then
    cmds="$1"
  else
    cmds="${cmds};$1"
  fi
}

cmds_append "Import-Module 'C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\Tools\\Microsoft.VisualStudio.DevShell.dll'"
cmds_append 'Enter-VsDevShell dffc9763'
cmds_append "cd $exe_dir"
cmds_append "devenv $exe_path"

echo $cmds

$pwsh -NoProfile -ExecutionPolicy Bypass -Command "&{$cmds}"


# cmds=Import-Module 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'; Enter-VsDevShell dffc9763; devenv $exe_path
# cmds=Import-Module 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'; Enter-VsDevShell dffc9763; devenv $exe_path
# cmds=';'Enter-VsDevShell dffc9763; devenv $exe_path

# "$bin" -NoProfile -ExecutionPolicy Bypass -Command "& {Import-Module 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'; Enter-VsDevShell dffc9763}"

