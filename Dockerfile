FROM devkitpro/devkitarm:20240511

ENV PATH=$DEVKITARM/bin:$PATH

RUN apt-get update && apt-get -y install --no-install-recommends build-essential cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib && rm -rf /var/lib/apt/lists/*

# clone pico-sdk
RUN git clone https://github.com/raspberrypi/pico-sdk
ENV PICO_SDK_PATH=/pico-sdk

WORKDIR /project
