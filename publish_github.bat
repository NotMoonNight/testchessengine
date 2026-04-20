@echo off
setlocal

if not exist .git (
    echo Iniciando git...
    git init
    git branch -M main
    git remote add origin https://github.com/NotMoonNight/testchessengine.git
)

echo Adicionando mudanças...
git add .

SET /P msg="Commit message: "
git commit -m "%msg%"

echo Publicando...
git push -u origin main

echo Pronto.
@pause