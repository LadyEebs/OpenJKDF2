# These are the standard features for full game support
set(TARGET_USE_PHYSFS TRUE)
#set(TARGET_USE_BASICSOCKETS TRUE)
set(TARGET_USE_GAMENETWORKINGSOCKETS TRUE)
set(TARGET_USE_LIBSMACKER TRUE)
set(TARGET_USE_LIBSMUSHER TRUE)
set(TARGET_USE_SDL2 TRUE)
set(TARGET_USE_OPENGL TRUE)
set(TARGET_USE_OPENAL TRUE)
set(TARGET_POSIX TRUE)
set(TARGET_NO_BLOBS TRUE)
set(TARGET_CAN_JKGM TRUE)
set(OPENJKDF2_NO_ASAN TRUE)
set(TARGET_USE_CURL TRUE)
set(TARGET_FIND_OPENAL TRUE)

if(OPENJKDF2_USE_BLOBS)
    set(TARGET_NO_BLOBS FALSE)
endif()

macro(plat_sdl2_deps)
    set(SDL2_COMMON_LIBS SDL2main SDL::SDL ${SDL_MIXER_DEPS} SDL::Mixer)
endmacro()