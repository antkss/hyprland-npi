# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/as/cac/hyprmake/src/hyprland/subprojects/wlroots-hyprland"
  "/home/as/cac/hyprmake/src/hyprland/subprojects/wlroots-hyprland/src/wlroots-hyprland-build"
  "/home/as/cac/hyprmake/src/hyprland/subprojects/wlroots-hyprland"
  "/home/as/cac/hyprmake/src/hyprland/subprojects/wlroots-hyprland/tmp"
  "/home/as/cac/hyprmake/src/hyprland/subprojects/wlroots-hyprland/src/wlroots-hyprland-stamp"
  "/home/as/cac/hyprmake/src/hyprland/subprojects/wlroots-hyprland/src"
  "/home/as/cac/hyprmake/src/hyprland/subprojects/wlroots-hyprland/src/wlroots-hyprland-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/as/cac/hyprmake/src/hyprland/subprojects/wlroots-hyprland/src/wlroots-hyprland-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/as/cac/hyprmake/src/hyprland/subprojects/wlroots-hyprland/src/wlroots-hyprland-stamp${cfgdir}") # cfgdir has leading slash
endif()
