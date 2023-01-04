FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update

RUN apt-get -y install python3-pip build-essential manpages-dev software-properties-common git cmake

RUN add-apt-repository ppa:ubuntu-toolchain-r/test

RUN apt-get update && apt-get -y install gcc-11 g++-11

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 20

RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 20

RUN pip install conan

RUN conan config set general.revisions_enabled=1
RUN conan profile new default --detect
RUN conan profile update settings.compiler.libcxx=libstdc++11 default

#secp256k1
WORKDIR /home/conan/
RUN rm -rf secp256k1
RUN git clone --depth 1 --branch master https://github.com/Gigamonkey-BSV/secp256k1.git
WORKDIR /home/conan/secp256k1
RUN conan install .
RUN CONAN_CPU_COUNT=1 conan create . proofofwork/stable

#data
WORKDIR /home/conan/
RUN rm -rf data
RUN git clone --depth 1 --branch production https://github.com/DanielKrawisz/data.git
WORKDIR /home/conan/data
RUN conan install .
RUN CONAN_CPU_COUNT=1 conan create . proofofwork/stable

#gigamonkey
WORKDIR /home/conan/
RUN rm -rf Gigamonkey
RUN git clone --depth 1 --branch production https://github.com/Gigamonkey-BSV/Gigamonkey.git
WORKDIR /home/conan/Gigamonkey
RUN conan install .
RUN CONAN_CPU_COUNT=1 conan create . proofofwork/stable

COPY . /home/conan/boostminer
WORKDIR /home/conan/boostminer
RUN chmod -R 777 .

RUN conan install .
RUN CONAN_CPU_COUNT=1 conan build .

CMD ./bin/BoostMiner


