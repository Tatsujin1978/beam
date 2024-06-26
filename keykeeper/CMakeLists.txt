project(keykeeper)
cmake_minimum_required(VERSION 3.13)

set(TARGET_NAME wasm-key-keeper)

if(EMSCRIPTEN)
    if (BEAM_CPP_20_STANDARD)
        set(CMAKE_CXX_STANDARD 20)
    else()
        set(CMAKE_CXX_STANDARD 17)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

add_library(keykeeper STATIC)

target_sources(keykeeper
    PRIVATE
        local_private_key_keeper.cpp
        remote_key_keeper.cpp
        hid_key_keeper.cpp
        ledger_loader.cpp
        ledger_app_nanos.cpp
        ledger_app_nanosplus.cpp
)

if(EMSCRIPTEN AND NOT BEAM_WALLET_CLIENT_LIBRARY)
    target_compile_definitions(keykeeper PUBLIC EMSCRIPTEN)
    target_link_libraries(keykeeper PRIVATE secp256k1)
    target_include_directories(keykeeper PUBLIC 
        "${PROJECT_SOURCE_DIR}/.." 
        "${PROJECT_SOURCE_DIR}/../3rdparty" 
        "${PROJECT_SOURCE_DIR}/../3rdparty/libuv/include"
        $ENV{BOOST_ROOT})
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-value")

    add_executable(${TARGET_NAME} wasm_key_keeper.cpp)
    target_link_libraries(${TARGET_NAME} PRIVATE keykeeper)

    add_subdirectory(${PROJECT_SOURCE_DIR}/../3rdparty/secp256k1 secp256k1)

    target_sources(keykeeper
        PRIVATE
            ${PROJECT_SOURCE_DIR}/../core/uintBig.cpp
            ${PROJECT_SOURCE_DIR}/../core/ecc.cpp
            ${PROJECT_SOURCE_DIR}/../core/ecc_bulletproof.cpp
            ${PROJECT_SOURCE_DIR}/../core/block_crypt.cpp
            ${PROJECT_SOURCE_DIR}/../core/block_rw.cpp
            ${PROJECT_SOURCE_DIR}/../core/merkle.cpp
            ${PROJECT_SOURCE_DIR}/../core/block_validation.cpp
            ${PROJECT_SOURCE_DIR}/../core/aes.cpp
            ${PROJECT_SOURCE_DIR}/../utility/common.cpp
            ${PROJECT_SOURCE_DIR}/../utility/hex.cpp

            ${PROJECT_SOURCE_DIR}/../mnemonic/dictionary.cpp
            ${PROJECT_SOURCE_DIR}/../mnemonic/mnemonic.cpp
            ${PROJECT_SOURCE_DIR}/../wallet/core/private_key_keeper.cpp
            ${PROJECT_SOURCE_DIR}/../wallet/core/common.cpp
            ${PROJECT_SOURCE_DIR}/../wallet/core/base58.cpp

            ${PROJECT_SOURCE_DIR}/../3rdparty/hmac_sha512.c
            ${PROJECT_SOURCE_DIR}/../3rdparty/pkcs5_pbkdf2.c
            ${PROJECT_SOURCE_DIR}/../3rdparty/sha512.c
            ${PROJECT_SOURCE_DIR}/../3rdparty/zeroize.c
            ${PROJECT_SOURCE_DIR}/../3rdparty/utilstrencodings.cpp
    )

    set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "--bind -s WASM=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s MODULARIZE=1 -s USE_BOOST_HEADERS=1")
else()

    target_link_libraries(keykeeper PRIVATE Boost::boost utility)
    
endif()

if (APPLE AND NOT IOS)
	find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
	find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
	message(STATUS "Framework IOKit: ${IOKIT_FRAMEWORK}")
	message(STATUS "Framework CoreFoundation: ${CORE_FOUNDATION_FRAMEWORK}")
	target_link_libraries(keykeeper PRIVATE ${IOKIT_FRAMEWORK})
	target_link_libraries(keykeeper PRIVATE ${CORE_FOUNDATION_FRAMEWORK})
endif()

if (NOT ANDROID AND NOT IOS)
    if(BEAM_HW_WALLET)

        # TREZOR client library
        add_library(trezor_client STATIC 
            ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client/src/messages/messages.pb.cc
            ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client/src/messages/messages-common.pb.cc
            ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client/src/messages/messages-management.pb.cc
            ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client/src/messages/messages-beam.pb.cc
        )

        if(MSVC)
            target_compile_options(trezor_client PRIVATE "/wd4125")
            target_compile_options(trezor_client PRIVATE "/wd4127")
            target_compile_options(trezor_client PRIVATE "/wd4244")
            target_compile_options(trezor_client PRIVATE "/wd4996")
        endif()
        
        target_include_directories(trezor_client 
            PUBLIC
                ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client
                ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client/src
                ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client/src/messages
                ${PROJECT_SOURCE_DIR}/../3rdparty/curl/include
                ${PROJECT_SOURCE_DIR}/../3rdparty/protobuf/src)

        target_link_libraries(trezor_client
            PUBLIC 
                core
                libprotobuf
                libcurl
        )

        # HW wallet hub library
        add_library(hw_wallet STATIC)

        target_sources(hw_wallet 
            PRIVATE 
                hw_wallet.cpp
                trezor_key_keeper.cpp)
        
        target_link_libraries(hw_wallet
            PRIVATE 
                trezor_client
                Boost::boost
                utility
        )

        target_link_libraries(keykeeper PUBLIC hw_wallet)

        # build trezor usage demo
        #add_executable(trezor-cpp-client ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client/main.cpp)
        #if(MSVC)
        #    target_compile_options(trezor-cpp-client PRIVATE "/wd4125")
        #    target_compile_options(trezor-cpp-client PRIVATE "/wd4127")
        #    target_compile_options(trezor-cpp-client PRIVATE "/wd4244")
        #    target_compile_options(trezor-cpp-client PRIVATE "/wd4996")
        #endif()
        #
        #target_include_directories(trezor-cpp-client PRIVATE 
        #    ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client
        #    ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client/src
        #    ${PROJECT_SOURCE_DIR}/../3rdparty/trezor-cpp-client/src/messages)
        #
        #target_link_libraries(trezor-cpp-client PRIVATE hw_wallet)

    endif()
endif()