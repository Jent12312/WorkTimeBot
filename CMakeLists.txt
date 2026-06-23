cmake_minimum_required(VERSION 3.20)
project(WorkTimeBot CXX)

# Убираем системное предупреждение CMake о поиске Boost (Policy CMP0167)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

# Устанавливаем стандарт C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ищем библиотеки, которые скачал vcpkg
find_package(TgBot CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# В vcpkg sqlite3 ищется под именем unofficial-sqlite3
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# Добавляем исходники
add_executable(${PROJECT_NAME} 
    src/main.cpp 
    src/Database.cpp
    src/BotCore.cpp
)

# Подключаем папку include для заголовочных файлов (если она есть)
target_include_directories(${PROJECT_NAME} PRIVATE include)

# Линкуем библиотеки к нашему проекту
target_link_libraries(${PROJECT_NAME} PRIVATE 
    TgBot::TgBot 
    SQLiteCpp 
    fmt::fmt 
    unofficial::sqlite3::sqlite3
)