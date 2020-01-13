@echo off

REM get thirdparty libs
mkdir thirdparty
pushd thirdparty

REM GLFW
git clone https://github.com/glfw/glfw.git
pushd glfw
git checkout -b v3_2_1 3.2.1
popd


REM GLM
git clone https://github.com/g-truc/glm.git
pushd glm
git checkout -b v0_9_9_3 0.9.9.3
popd

popd

REM Setup build
mkdir build
pushd build
cmake -G "Visual Studio 16 2019" ..
popd
@echo on