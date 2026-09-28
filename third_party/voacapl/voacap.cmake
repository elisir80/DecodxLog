# DecoDXLog — VOACAP, il motore di propagazione dell'ITS, nella versione per
# gfortran di voacapl (vedi README.DecoDXLog.md).
#
# Si costruisce solo se c'e' un compilatore Fortran (MSYS2: il pacchetto
# mingw-w64-x86_64-gcc-fortran). Il risultato sta in <build>/voacap:
#   voacapl.exe            il programma, collegato statico (niente DLL Fortran)
#   itshfbc/coeffs         i coefficienti CCIR/URSI in binario, fatti qui dai .asc
#   itshfbc/database       i file che VOACAP legge all'avvio
#   itshfbc/antennas       le antenne di serie
# deploy.sh copia la cartella accanto a DecoDXLog.exe; il programma la copia a
# sua volta in una cartella scrivibile (VOACAP scrive nella sua).

set(VOACAPL_DIR ${CMAKE_CURRENT_LIST_DIR})
include(${VOACAPL_DIR}/sources.cmake)

set(VOACAP_OUT ${CMAKE_BINARY_DIR}/voacap)
set(VOACAP_DATA ${VOACAP_OUT}/itshfbc)
set(VOACAPL_MODDIR ${CMAKE_BINARY_DIR}/voacapl_mod)
# Le opzioni dei Makefile.am di voacapl.
set(VOACAPL_FFLAGS -w -fno-backslash -ffixed-line-length-none -fno-sign-zero)

add_library(voacapl_modules STATIC ${VOACAPL_MODULES_SOURCES})
target_compile_options(voacapl_modules PRIVATE -cpp -w)
target_compile_definitions(voacapl_modules PRIVATE "VERSION=\"0.7.7\"")

foreach(part voa_lib hfmufesw wp10dwin)
    string(TOUPPER ${part} PART)
    add_library(voacapl_${part} STATIC ${VOACAPL_${PART}_SOURCES})
    target_compile_options(voacapl_${part} PRIVATE ${VOACAPL_FFLAGS})
    target_link_libraries(voacapl_${part} PUBLIC voacapl_modules)
endforeach()

add_executable(voacapl ${VOACAPL_VOACAPW_SOURCES})
target_compile_options(voacapl PRIVATE -cpp ${VOACAPL_FFLAGS})
target_include_directories(voacapl PRIVATE ${VOACAPL_DIR}/src/voacapw)
# Come fa automake: le librerie come archivi, cosi' un simbolo che sta in due
# (invcon) si prende dal primo che serve.
target_link_libraries(voacapl PRIVATE voacapl_hfmufesw voacapl_voa_lib voacapl_wp10dwin voacapl_modules)
# Su Windows collegato statico: nel pacchetto non servono le DLL di gfortran.
if(WIN32)
    target_link_options(voacapl PRIVATE -static)
endif()

foreach(t voacapl_modules voacapl_voa_lib voacapl_hfmufesw voacapl_wp10dwin voacapl)
    set_target_properties(${t} PROPERTIES Fortran_MODULE_DIRECTORY ${VOACAPL_MODDIR})
    target_include_directories(${t} PRIVATE ${VOACAPL_MODDIR})
endforeach()
set_target_properties(voacapl PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${VOACAP_OUT})

# I coefficienti: VOACAP li legge in binario, e il binario lo fa il compilatore
# che si ha. Si convertono una volta, qui.
foreach(tool coefbinw fof2binw)
    add_executable(voacapl_${tool} ${VOACAPL_DIR}/itshfbc/coeffs/${tool}.f)
    target_compile_options(voacapl_${tool} PRIVATE -w -ffixed-line-length-none)
    if(WIN32)
        target_link_options(voacapl_${tool} PRIVATE -static)
    endif()
endforeach()

# I programmi scrivono accanto ai .asc: si lavora in una cartella a parte e
# nei dati vanno solo i binari.
file(GLOB VOACAPL_ASC ${VOACAPL_DIR}/itshfbc/coeffs/*.asc)
set(VOACAPL_WORK ${VOACAP_OUT}/coeffs-work)
set(VOACAPL_BIN_NAMES fof2CCIR.daw fof2URSI.daw fof2dalw.bin)
foreach(m 01 02 03 04 05 06 07 08 09 10 11 12)
    list(APPEND VOACAPL_BIN_NAMES coeff${m}w.bin)
endforeach()
set(VOACAPL_BINS)
set(VOACAPL_WORK_BINS)
foreach(name IN LISTS VOACAPL_BIN_NAMES)
    list(APPEND VOACAPL_BINS ${VOACAP_DATA}/coeffs/${name})
    list(APPEND VOACAPL_WORK_BINS ${VOACAPL_WORK}/${name})
endforeach()
add_custom_command(
    OUTPUT ${VOACAPL_BINS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${VOACAPL_WORK} ${VOACAP_DATA}/coeffs
    COMMAND ${CMAKE_COMMAND} -E copy ${VOACAPL_ASC} ${VOACAPL_WORK}
    COMMAND $<TARGET_FILE:voacapl_coefbinw> ${VOACAPL_WORK}
    COMMAND $<TARGET_FILE:voacapl_fof2binw> ${VOACAPL_WORK}
    COMMAND ${CMAKE_COMMAND} -E copy ${VOACAPL_WORK_BINS} ${VOACAP_DATA}/coeffs
    DEPENDS voacapl_coefbinw voacapl_fof2binw ${VOACAPL_ASC}
    WORKING_DIRECTORY ${VOACAP_OUT}
    COMMENT "VOACAP: coefficients to binary"
    VERBATIM)
add_custom_target(voacap_data ALL
    DEPENDS ${VOACAPL_BINS}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${VOACAPL_DIR}/itshfbc/database ${VOACAP_DATA}/database
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${VOACAPL_DIR}/itshfbc/antennas ${VOACAP_DATA}/antennas
    COMMAND ${CMAKE_COMMAND} -E copy ${VOACAPL_DIR}/LICENSE ${VOACAP_OUT}/LICENSE.txt
    COMMENT "VOACAP: data files")
