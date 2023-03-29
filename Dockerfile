FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update

RUN apt-get -y install python3-pip build-essential manpages-dev software-properties-common git cmake

RUN apt-get -y install autoconf
RUN apt-get -y install libtool

RUN add-apt-repository ppa:ubuntu-toolchain-r/test

RUN apt-get update && apt-get -y install gcc-11 g++-11

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 20

RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 20

RUN pip install conan
RUN conan --version

RUN conan profile detect

#secp256k1
WORKDIR /home
RUN rm -rf secp256k1
RUN git clone https://github.com/Gigamonkey-BSV/secp256k1-conan-recipe.git
WORKDIR /home/secp256k1-conan-recipe
RUN conan create . --user=proofofwork --channel=stable

#data
WORKDIR /home
RUN rm -rf data
RUN git clone --depth 1 --branch master https://github.com/DanielKrawisz/data.git
WORKDIR /home/data
RUN conan install . --build=missing
WORKDIR /home/data/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
WORKDIR /home/data
RUN CONAN_CPU_COUNT=1 conan create . --user=proofofwork --channel=stable

#gigamonkey
WORKDIR /home
RUN rm -rf Gigamonkey
RUN git clone --depth 1 --branch master https://github.com/Gigamonkey-BSV/Gigamonkey.git
WORKDIR /home/Gigamonkey
RUN conan install . --build=missing
WORKDIR /home/Gigamonkey/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
WORKDIR /home/Gigamonkey
RUN CONAN_CPU_COUNT=1 conan create . --user=proofofwork --channel=stable

COPY . /home/boostminer
WORKDIR /home/boostminer
RUN chmod -R 777 .

RUN conan install . --build=missing
WORKDIR home/boostminer/build
RUN cmake ..
RUN make
#RUN CONAN_CPU_COUNT=1 conan build .

CMD ./bin/BoostMiner


