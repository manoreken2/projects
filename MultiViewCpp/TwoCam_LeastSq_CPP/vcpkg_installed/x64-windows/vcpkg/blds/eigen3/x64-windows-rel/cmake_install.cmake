# Install script for directory: C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/src/3.4.0-1beca8819c.clean

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/pkgs/eigen3_x64-windows")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "OFF")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Devel" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/eigen3" TYPE FILE FILES "C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/src/3.4.0-1beca8819c.clean/signature_of_eigen3_matrix_library")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/x64-windows-rel/eigen3.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Devel" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/eigen3" TYPE DIRECTORY FILES "C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/src/3.4.0-1beca8819c.clean/Eigen")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/eigen3/Eigen3Targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/eigen3/Eigen3Targets.cmake"
         "C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/x64-windows-rel/CMakeFiles/Export/584ba2d95dead5aba0c59a38c7fd0121/Eigen3Targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/eigen3/Eigen3Targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/eigen3/Eigen3Targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/eigen3" TYPE FILE FILES "C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/x64-windows-rel/CMakeFiles/Export/584ba2d95dead5aba0c59a38c7fd0121/Eigen3Targets.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/eigen3" TYPE FILE FILES
    "C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/src/3.4.0-1beca8819c.clean/cmake/UseEigen3.cmake"
    "C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/x64-windows-rel/Eigen3Config.cmake"
    "C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/x64-windows-rel/Eigen3ConfigVersion.cmake"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/x64-windows-rel/unsupported/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "C:/work/projects/MultiViewCpp/TwoCam_LeastSq_CPP/vcpkg_installed/x64-windows/vcpkg/blds/eigen3/x64-windows-rel/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
