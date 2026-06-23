# --- Stage 1: Сборка проекта ---
FROM debian:12-slim AS builder

# Установка необходимых пакетов для сборки
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Настройка vcpkg
WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git
RUN ./vcpkg/bootstrap-vcpkg.sh

# Копируем проект
WORKDIR /app
COPY vcpkg.json /app/
COPY CMakeLists.txt /app/

# Предварительно устанавливаем зависимости через vcpkg (для кэширования слоев Docker)
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# Копируем исходный код
COPY src /app/src
COPY include /app/include

# Конфигурируем и собираем проект
WORKDIR /app/build
RUN cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release
RUN cmake --build .

# --- Stage 2: Запуск готового бота ---
FROM debian:12-slim

WORKDIR /app

# Устанавливаем сертификаты (нужны для HTTPS-запросов к Telegram API)
RUN apt-get update && apt-get install -y ca-certificates && rm -rf /var/lib/apt/lists/*

# Копируем скомпилированный бинарник из builder stage
COPY --from=builder /app/build/WorkTimeBot /app/WorkTimeBot

# Создаем папку для сохранения базы данных
RUN mkdir -p /app/data

# Точка входа
CMD ["/app/WorkTimeBot"]
